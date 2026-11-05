import { common } from '@kit.AbilityKit';
import { picker, fileIo as fs } from '@kit.CoreFileKit';
import hash from '@ohos.file.hash';
import statvfs from '@ohos.file.statvfs';
import util from '@ohos.util';
import sdk from 'libpicosdk.so';
@Entry
@Component
struct Index {
  @StorageLink('sdkReport') report: string = 'In-process CPU SDK. No Ollama, HTTP or child service.';
  @State copying: boolean = false;
  private async choose(): Promise<void> {
    if (sdk.running() || this.copying) return;
    this.copying = true;
    const context = getContext(this) as common.UIAbilityContext;
    const temporary = context.filesDir + '/model.part-' + util.generateRandomUUID();
    let input: fs.File | undefined = undefined;
    let output: fs.File | undefined = undefined;
    try {
      const options = new picker.DocumentSelectOptions(); options.maxSelectNumber = 1;
      const uris = await new picker.DocumentViewPicker(context).select(options);
      if (uris.length === 0) { this.report = 'Selection cancelled'; return; }
      input = fs.openSync(uris[0], fs.OpenMode.READ_ONLY | fs.OpenMode.NOFOLLOW);
      const size = fs.statSync(input.fd).size;
      if (size < 24 || size > 1024 * 1024 * 1024 || await statvfs.getFreeSize(context.filesDir) < size + 256 * 1024 * 1024) throw new Error('SDK test expects one GGUF up to 1 GiB and sufficient private space');
      this.report = 'Copying selected model asynchronously…';
      output = fs.openSync(temporary, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE | fs.OpenMode.NOFOLLOW);
      await fs.copyFile(input.fd, output.fd); fs.fsyncSync(output.fd);
      if (fs.statSync(output.fd).size !== size) throw new Error('Model copy size mismatch');
      fs.closeSync(output); output = undefined;
      const digest = await hash.hash(temporary, 'sha256');
      fs.renameSync(temporary, context.filesDir + '/model.gguf');
      const note = fs.openSync(context.filesDir + '/model-identity.json', fs.OpenMode.CREATE | fs.OpenMode.WRITE_ONLY | fs.OpenMode.TRUNC);
      try { fs.writeSync(note.fd, JSON.stringify({ size, sha256: digest, source: uris[0] })); fs.fsyncSync(note.fd); }
      finally { fs.closeSync(note); }
      this.report = 'Model ready: ' + size.toString() + ' bytes, SHA256 ' + digest;
    } catch (error) { this.report = String(error); }
    finally {
      if (input !== undefined) fs.closeSync(input);
      if (output !== undefined) fs.closeSync(output);
      try { fs.unlinkSync(temporary); } catch (_) {}
      this.copying = false;
    }
  }
  private async run(): Promise<void> {
    const context = getContext(this) as common.UIAbilityContext;
    const id = util.generateRandomUUID().replace(/-/g, '');
    this.report = 'Running on native worker: ' + id;
    try { this.report = await sdk.run(context.filesDir, id); }
    catch (error) { this.report = String(error); }
  }
  build() {
    Column({ space: 16 }) {
      Text('libllama — application-process CPU SDK').fontSize(24)
      Button('Choose SDK GGUF').enabled(!this.copying).onClick(() => this.choose())
      Button('Run SDK suite').enabled(!this.copying).onClick(() => this.run())
      Button('Cancel SDK work').onClick(() => sdk.cancel())
      Scroll() { Text(this.report).fontSize(16).width('100%') }.layoutWeight(1)
    }.padding(24).width('100%').height('100%')
  }
}
