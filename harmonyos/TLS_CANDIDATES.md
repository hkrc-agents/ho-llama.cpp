# OHOS TLS / router 候选边界

这些文件属于下游OHOS适配，不属于最小上游导出。
当前模拟器server已更新为`dfc420d9…`的目录CA/重定向候选；它的独立15+2项
TLS/router设备矩阵及真实模型故障测试、正式HAP增量与正反登录重启回归已通过，
后续原始593项完整重放为399通过/7失败/187跳过，没有新增失败或skip。
六项小模型模板用例与Linux FP64基线同样失败，另有一项主机HDC IPv6桥接失败；
18项配对状态相同，445份实际退出回执独立审计通过。这不是全部上游测试通过。
本目录没有替换任何系统库，具体包身份与资格以本轮验收记录为准。

`scripts/build_harmonyos_ollama_runtime.py` 的默认行为保持 TLS/router 关闭。
`--tls --router` 生成 LibreSSL/subprocess 候选并应用仅OHOS生效的
`http-ca-store.patch` / `http_ca_store.h`，必须另行部署到隔离诊断包验收。
LibreSSL 4.3.2 源包 SHA256 固定，COPYING 随候选保留；不分发 SDK stub 库。

最小候选通过HAP公开CertificateManager获取只读系统CA目录，以
`LLAMA_OHOS_CA_DIRECTORY`传入native HTTPS client。显式设置CA路径并始终开启证书链与
hostname校验，覆盖普通下载/图片及router代理；SSL_CERT_FILE显式配置仍优先。
不改Go原有SSL_CERT_FILE/DIR、不修改系统信任根、不加载NetStack SDK stub。
独立诊断版与最小无诊断版分别实测；不能根据编译完成或旧二进制结果晋升。
独立公共HNP CLI不会自动继承HAP的环境。CLI在线下载应由调用者显式配置可信
`SSL_CERT_FILE`、`LLAMA_OHOS_CA_DIRECTORY`或`SSL_CERT_DIR`；不能把HAP已注入目录
解释为其他Client已经配置，也不通过关闭验证解决。

随后真实远程preset暴露目录CA在跨host HTTPS redirect时未传递：固定vendor的
`setup_redirect_client`不复制CA配置，旧分支又只在CA文件非空时调用setter。
补丁现仅在`__OHOS__`分支接受目录非空；非OHOS条件保持原样。主机真实vendor A/B
中，两分支直接TLS均200、原分支redirect失败、OHOS修复分支redirect200。
此对照使用隔离的一日测试CA，不安装系统根，不代替新OHOS二进制实测或晋升门禁。
后续`dfc420d9…`已通过真实远程HF preset/子模型下载、正确hostname跨host redirect、
错误hostname直接与redirect拒绝且无HTTP数据；完整证据见文末实验链接。

`--experimental-system-tls` 另行启用 `system-tls.patch` / `system_tls.h`。
该方案通过公开 NetStack API读取信任锚，并保持证书链和hostname校验。
2026-10-23在API23模拟器的HNP ELF中因 `libnet_ssl.so` 加载失败而未获得资格；
不是“编译成功就支持HTTPS”，也不能直接外推到HAP Native SO或真机。

诊断器另支持 `--system-ca-host <hostname>`：由HAP Native桥读取指定hostname的信任锚，
将私有PEM快照通过 `SSL_CERT_FILE` 交给上游LibreSSL。它只用于区分链接器边界与信任根问题；
不代表已实现通用的逐host策略、pinning、撤销与热更新。不得将该快照当作通用产品信任策略，
不得导入开发主机CA、关闭验证或读取修改系统证书目录来换取通过。

router兼容只在OHOS且启用subprocess时选择vendor已有fork/exec回退，不改其他平台
的默认spawn路径；上面的vendor HTTPS补丁是另一项独立OHOS条件修复。独立测试
supervisor负责按leader/PGID收尾，不能把测试器的
回收资格直接当产品完整router生命周期资格。

实际失败、复测、包身份和晋升边界见
[CPU后续验收](../../../exp/HARMONYOS_CPU_FOLLOWTHROUGH_20261023.md)。
