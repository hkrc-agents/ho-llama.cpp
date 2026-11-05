import { UIAbility, Want } from '@kit.AbilityKit';
import { window } from '@kit.ArkUI';
import sdk from 'libpicosdk.so';
export default class EntryAbility extends UIAbility {
  private dispatch(want: Want): void {
    if (want.parameters?.['action'] === 'cancel') { sdk.cancel(); return; }
    const id = want.parameters?.['runId'];
    if (typeof id !== 'string' || !/^[0-9a-f]{32}$/.test(id)) return;
    AppStorage.setOrCreate('sdkReport', 'Running: ' + id);
    try {
      sdk.run(this.context.filesDir, id).then((report: string) => {
        AppStorage.setOrCreate('sdkReport', report);
      }).catch((error: Error) => { AppStorage.setOrCreate('sdkReport', String(error)); });
    } catch (error) { AppStorage.setOrCreate('sdkReport', String(error)); }
  }
  onCreate(want: Want): void { this.dispatch(want); }
  onNewWant(want: Want): void { this.dispatch(want); }
  onDestroy(): void { sdk.cancel(); }
  onWindowStageCreate(stage: window.WindowStage): void { stage.loadContent('pages/Index'); }
}
