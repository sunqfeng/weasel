# 小狼毫 Weasel · Jev 候选推荐实验版

这是基于 [Rime 小狼毫（Weasel）](https://github.com/rime/weasel) 的个人 fork，面向 Windows。项目保留小狼毫原有的输入方案与配置方式，并增加了可选的 [TypeSafe Jev](https://typesafe.ai/) 候选推荐功能。Jev 功能仍处于实验阶段，默认关闭。

本仓库的安装包和问题反馈请使用本项目链接；小狼毫及 Rime 的通用使用资料仍以[上游项目](https://github.com/rime/weasel)为准。

## 下载与安装

在本项目的 [Releases 页面](https://github.com/sunqfeng/weasel/releases)下载 `weasel` 开头的 `.exe` 安装包。目前提供的是**预发布测试版**。安装包未进行代码签名，Windows SmartScreen 可能显示提示；请核对下载来源后再决定是否运行。

小狼毫适用于 Windows 8.1 至 Windows 11。安装时可选择输入语言；安装完成后，从系统输入法列表切换到“小狼毫”即可使用。已有 Rime 配置通常位于 `%AppData%\Rime`，安装测试版前建议先备份该目录。

即使不启用 Jev，本项目仍可按普通小狼毫使用。

## Jev 候选推荐

输入拼音时，小狼毫会先显示 Rime 原有候选，再异步向 Jev 请求推荐。Jev 只针对第一页、至少有两个候选的输入进行判断，最多提交当前页前 10 个候选。推荐结果返回后会尝试更新高亮候选，按空格时还会再检查一次。概率达到 60% 时直接采用；在多候选场景中，概率达到 40% 且比第二名领先至少 10 个百分点时也会采用。结果尚未返回、已过期、请求失败或判断不够明确时，沿用 Rime 原有选择。

功能需要 TypeSafe API Key，并且只会在你指定的应用中发送请求。启用后，请在 PowerShell 中运行：

```powershell
setx WEASEL_JEV_ENABLED "1"
setx TYPESAFE_API_KEY "你的 TypeSafe API Key"
setx WEASEL_JEV_APPS "notepad.exe,winword.exe"
```

`WEASEL_JEV_APPS` 是必填的应用白名单，可用逗号或分号分隔程序名。建议先只填写 `notepad.exe` 进行测试。`setx` 写入的是后续进程使用的用户环境变量；设置完成后，需要让小狼毫服务在新环境中重新启动，必要时退出并重新登录 Windows。

对于白名单中的应用，请求会发往 `https://api.typesafe.ai/v1/systemone`，内容包括最多 128 个最近提交的输入字符、当前预编辑文本和候选词。输入区域失去焦点后，本地保存的这段输入上下文会被清除。请只把你愿意发送这些内容的应用加入白名单。

关闭功能可运行 `setx WEASEL_JEV_ENABLED "0"`，随后重新启动小狼毫服务。环境变量中的 API Key 不会因此自动删除。

## 小狼毫基本使用

- 使用 <kbd>Ctrl</kbd> + <kbd>`</kbd> 或 <kbd>F4</kbd> 打开输入方案菜单。
- 用户词库与配置文件位于 `%AppData%\Rime`。修改后需要执行“重新部署”。
- 输入方案的自定义方法参见 [Rime 定制指南](https://github.com/rime/home/wiki/CustomizationGuide)；小狼毫的样式与行为设置参见[上游 Wiki](https://github.com/rime/weasel/wiki)。

## 从源码构建

本项目沿用上游小狼毫的 Windows 构建流程，具体依赖和命令见 [INSTALL.md](INSTALL.md)。GitHub Actions 会执行格式检查，并使用 MSBuild 与 xmake 构建；通过构建不代表 Jev 功能已经完成真实 API 的端到端验证。

## 问题反馈

与本 fork 的 Jev 功能、测试版安装包有关的问题，请在[本项目 Issues](https://github.com/sunqfeng/weasel/issues)反馈。小狼毫本体的问题可参考[上游 Issues](https://github.com/rime/weasel/issues)。反馈时请说明 Windows 版本、小狼毫版本、使用的输入方案及复现步骤；不要公开 API Key 或包含敏感输入的日志。

## 上游与许可

本项目基于 [rime/weasel](https://github.com/rime/weasel)，输入引擎、输入方案、界面和大量依赖均来自 Rime 社区及上游贡献者。感谢[上游贡献者](https://github.com/rime/weasel/graphs/contributors)。本项目沿用 [GNU GPLv3 许可](LICENSE.txt)；各第三方组件仍遵循其各自的许可证。
