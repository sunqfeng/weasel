# Rime、Octagram 与 Jev 分层候选管线

## 目标行为

1. Rime 生成原生候选，并继续使用用户词库与自适应学习。
2. Octagram 在本机结合已提交的上下文调整候选顺序。
3. Jev 默认关闭。用户明确开启后，也只在本地候选质量不足时异步请求云端建议。
4. Jev 结果到达时立即刷新候选窗口；提交时再做一次过期和候选一致性校验。

## 当前状态

- 安装包会下载经过 SHA-256 校验的官方简体 Octagram 模型，并为 `luna_pinyin_simp` 启用 `grammar:/hans`。
- 模型来源的 master 与 hans 提交均已锁定，避免模型更新让回归结果失去可比性。
- Jev 默认关闭，只有 `WEASEL_JEV_ENABLED=1` 且存在 API Key 时才启动。
- Jev 启用后作用于所有应用；不再读取 `WEASEL_JEV_APPS`。
- Jev 工作线程完成请求后会向 UI 线程发送结果消息，并立即调用 `_UpdateUI`。空格提交路径保留最后一次应用机会。
- Jev 候选会按文本重新映射到当前候选列表，不要求返回前后的候选位置完全一致。

## 本地置信度接口

当前 librime C API 的 `RimeCandidate` 只公开 `text`、`comment` 和未使用的 `reserved` 字段。内部 C++ `Candidate` 有 `quality()`，但 Weasel 无法通过稳定的公开接口读取它。因此门控逻辑不能用候选名次冒充概率。

下一步在锁定的 librime 版本末尾追加版本化 API，例如：

```cpp
Bool (*get_candidate_quality)(RimeSessionId session_id,
                              size_t index,
                              double* quality);
```

追加函数指针保持旧客户端的结构前缀兼容。Weasel 必须先用 `RIME_API_AVAILABLE` 检查接口，再读取同一代候选的 top-1、top-2 质量值。接口不可用时采用保守策略，不把名次差当作置信度。

初始门控规则不写死阈值。评测程序记录 `top1_quality - top2_quality`、本地 top-1 是否正确、Octagram 是否改变 top-1，再从标注数据上生成 precision/coverage 曲线。选择阈值时先约束本地精确率，再观察 Jev 请求覆盖率。

## 回归评测

评测使用两个隔离的 Rime 用户目录和相同的词典、用户词库快照：

- 基线目录显式禁用 grammar；
- Octagram 目录启用 `grammar:/hans`；
- 每条样本先在真实 Rime session 中提交上下文，再输入目标拼音并读取实际候选；
- 记录 top-1 准确率、正确答案名次、Octagram 改善数、退化数及无变化数；
- Jev 层另行记录被门控样本上的准确率、采用精确率、覆盖率和延迟分位数。

只向 Jev 直接提交人工候选的现有测试不能替代这项引擎级对照测试，因为它没有运行 Rime 和 Octagram。

## 用户同意与本地反馈

环境变量保持默认关闭。首次启用时的图形提示需要说明：发送目标域名、最近上下文、当前拼音和候选列表，以及功能可随时关闭。用户已经明确要求全应用生效，因此首版不恢复应用白名单；以后可以增加可选的应用范围设置。

本地反馈只记录不含正文的事件和计数：是否采用 Jev、置信度来源、延迟、下一操作是否为退格或撤销。原始上下文、拼音、候选和 API Key 不写入反馈文件。统计应区分“立即纠正”与普通后续编辑，并提供关闭与清除入口。
