# 小狼毫 Weasel · Jev 候选推荐

这是基于 [Rime 小狼毫（Weasel）](https://github.com/rime/weasel) 的 Windows 输入法 fork。保留小狼毫原有的输入方案与配置方式，增加可选的 Jev 语境候选推荐、自定义决策模型接口，以及候选窗口状态图标。**Jev 默认关闭**；不启用时可按普通小狼毫使用。

## 下载与安装

请在本项目的 [Releases 页面](https://github.com/sunqfeng/weasel/releases)下载 `weasel` 开头的 `.exe` 安装包；如果尚无可用安装包，请等待 [GitHub Actions](https://github.com/sunqfeng/weasel/actions) 构建并发布完成。安装包未进行代码签名，Windows SmartScreen 可能显示提示，请核对下载来源。

小狼毫适用于 Windows 8.1 至 Windows 11。安装完成后，从系统输入法列表切换到“小狼毫”即可使用。已有 Rime 配置通常位于 `%AppData%\Rime`，升级前建议先备份。安装包不包含私人服务地址、模型选择或 API Key，也不会替你启用云端处理。

## 本 fork 新增特性

- **可配置的决策接口与模型**：通过环境变量选择兼容 Jev 协议的 HTTPS endpoint 和 model，无需修改源码。保留 TLS 验证，拒绝重定向，避免凭据被转发。
- **异步状态可视化**：候选窗以旋转圆环、星标、空心圆点和带斜线云朵表示请求与结果状态。
- **结果返回后更新候选窗**：TSF 客户端轮询已完成的决策，用户无需先按空格才能看到推荐。
- **只高亮，不重排**：保留 Rime 候选顺序；若请求期间用户已导航，不再自动移动高亮，仍用星标标出模型推荐。
- **全应用与排除规则**：允许列表支持 `*` 和 `!app.exe`，排除项优先。
- **可选诊断日志**：按需记录请求完成、决策概率与校验结果，候选文本日志默认关闭。

## Jev 推荐行为与状态图标

输入时，Rime 先生成普通候选，后台再向决策服务发送近期上屏文字、当前输入码及候选。模型只能从声明的候选或原始输入中选择，不能提交选择集合以外的自由文本。

| 图标 | 含义 |
| --- | --- |
| 候选窗角落的旋转圆环 | 决策请求进行中 |
| 候选旁的星标 | 模型推荐该候选，选择概率至少为 60% |
| 候选窗角落的星标 | 模型推荐保留原始输入；满足校验条件时按空格提交 |
| 空心圆点 | 选择概率不足 60%，保留原生行为 |
| 带斜线的云朵 | 请求失败或超时，沿用普通 Rime 行为 |
| 无图标 | Jev 未启用、应用不在允许列表中，或没有可展示的决策状态 |

推荐结果不会改变候选顺序。仅在请求期间用户尚未导航时，程序自动高亮推荐候选；方向键、翻页、鼠标等交互不会被迟到的结果覆盖。原始输入推荐只有在第一页且输入与交互状态未变时，才会由空格提交。

请求在后台执行，不阻塞普通按键。TSF 在请求期间约每 90 ms 轮询一次、最多约 4 秒，失去焦点或停用后停止轮询。过期或不匹配的结果会被丢弃；请求未及时返回时，继续使用普通输入行为。

## 配置决策服务

服务器读取以下环境变量。修改后必须让 **WeaselServer 在新的环境中重新启动**；仅修改环境变量不会更新已经运行的进程。

| 变量 | 默认值 / 用途 |
| --- | --- |
| `WEASEL_JEV_ENABLED` | 设为 `1` 启用 Jev，默认关闭 |
| `TYPESAFE_API_KEY` | 决策服务的 Bearer 凭据 |
| `WEASEL_JEV_APPS` | 必填；允许发送请求的可执行文件名，支持逗号或分号分隔 |
| `WEASEL_JEV_ENDPOINT` | 可选；兼容决策服务的完整 HTTPS URL，未设置时使用上游 TypeSafe 接口 |
| `WEASEL_JEV_MODEL` | 可选；请求中的模型名，默认 `jev-latest` |

例如，在 PowerShell 中为当前进程及其随后启动的子进程设置：

```powershell
$env:WEASEL_JEV_ENABLED = '1'
$env:TYPESAFE_API_KEY = '<your-api-key>'
$env:WEASEL_JEV_APPS = 'notepad.exe,winword.exe'
# 使用兼容的自定义服务时，再设置以下两项：
# $env:WEASEL_JEV_ENDPOINT = 'https://example.com/decisions'
# $env:WEASEL_JEV_MODEL = '<your-model>'
```

先退出正在运行的小狼毫服务，再从同一个 PowerShell 窗口启动所安装的 `WeaselServer.exe`，它才会继承这些变量。以上配置不会永久保存；可使用自己的本地启动脚本管理，但不要把真实凭据提交到仓库。

允许列表示例：

```text
notepad.exe,winword.exe   # 仅启用指定应用
*                        # 启用所有应用
*, !keepass.exe          # 所有应用，但排除 keepass.exe
```

上面的注释仅供说明，不要把 `#` 后的文字写进变量值。程序名匹配不区分大小写；排除项优先，只有排除项而没有 `*` 或明确允许项时不会启用任何应用。建议先仅允许 `notepad.exe` 测试，再按隐私需求扩大范围。

自定义 endpoint 必须使用 HTTPS，不能包含用户名、密码、查询参数或 fragment。服务必须兼容现有的 `state`、`questions.intent` 请求和 `answers.intent` 选择响应；并非任意聊天补全 API 都可以直接使用。

### 诊断与隐私

| 变量 | 用途 |
| --- | --- |
| `WEASEL_JEV_DIAGNOSTIC_LOG` | 本地追加日志文件路径；未设置则关闭 |
| `WEASEL_JEV_LOG_DECISIONS=1` | 额外记录返回选择、置信度和概率 |
| `WEASEL_JEV_LOG_CANDIDATES=1` | 额外记录候选与原始输入文本；默认关闭 |

允许应用中的请求会包含最多 128 个近期上屏字符、当前预编辑文本及候选词。上下文按标点或换行边界截取，输入区域失去焦点后清除本地上下文。请仅允许愿意向所选服务发送输入内容的应用；启用 `*` 前尤其应考虑隐私。

诊断日志不记录授权头或凭据，但候选文本日志可能暴露私人输入，只应在有意测试时开启，且应保存在仓库外。推荐只向服务器进程提供凭据，而不是永久保存到用户环境变量。

关闭 Jev：将 `WEASEL_JEV_ENABLED` 设为 `0`，随后重启服务器；这不会自动删除已保存的凭据。

协议、安全限制、超时与图标实现细节见 [决策接口配置文档](docs/jev-endpoint-configuration.md)。

## 小狼毫基本使用

- 使用 <kbd>Ctrl</kbd> + <kbd>`</kbd> 或 <kbd>F4</kbd> 打开输入方案菜单。
- 用户词库与配置文件位于 `%AppData%\Rime`。修改后需要执行“重新部署”。
- 输入方案自定义参见 [Rime 定制指南](https://github.com/rime/home/wiki/CustomizationGuide)；样式与行为设置参见[上游 Wiki](https://github.com/rime/weasel/wiki)。
- 默认中英文状态由用户的 Rime 方案配置决定；本 fork 不强制覆盖已有用户设置。

## 从源码构建

沿用上游 Windows 构建流程，依赖和命令见 [INSTALL.md](INSTALL.md)。GitHub Actions 执行格式检查，并使用 MSBuild 与 xmake 构建。CI 成功不等同于对所有决策服务完成端到端验证。

## 问题反馈

本 fork 的新增功能或安装包问题，请在[本项目 Issues](https://github.com/sunqfeng/weasel/issues)反馈；小狼毫本体问题可参考[上游 Issues](https://github.com/rime/weasel/issues)。请提供 Windows 版本、输入法版本、输入方案及复现步骤，不要公开 API Key 或包含敏感输入的日志。

## 上游与许可

本项目基于 [rime/weasel](https://github.com/rime/weasel)，输入引擎、输入方案、界面及大量依赖来自 Rime 社区与上游贡献者。感谢[上游贡献者](https://github.com/rime/weasel/graphs/contributors)。沿用 [GNU GPLv3 许可](LICENSE.txt)，第三方组件遵循各自许可证。
