# Jev 输入意图路由实施计划

## 第 1 步：锁定纯逻辑契约

- 新增路由类型和纯函数接口。
- 先补充失败测试，覆盖选择集合、响应解析和安全校验。
- 用最小实现使测试通过。
- 里程碑：不接入输入法主流程也能证明模型输出不能越权。

## 第 2 步：接入 Jev 请求

- 将现有候选索引请求改为稳定选项键请求。
- 加入合法 `raw_input` 选项和候选文本类型。
- 保留异步、防抖、超时和 1 MiB 响应上限。
- 里程碑：网络层只产生经过解析的 `JevDecision`。

## 第 3 步：接入空格提交路径

- 候选结果继续使用高亮后交给 Rime 提交。
- 原始字母结果使用会话级待提交字段，经现有 IPC 输出。
- 验证旧结果、低置信度和状态变化均不影响 Rime 原行为。
- 里程碑：三路选择可用，所有失败路径可降级。

## 第 4 步：CI 与文档

- 让测试程序非交互运行，并在 GitHub Actions 中执行。
- 更新简体中文 README。
- 运行格式检查、MSBuild、单元测试和 xmake 构建。
- 里程碑：CI 全绿并产出可安装构件。

## 提交策略

每个完成的里程碑单独提交：

1. `docs: define Jev intent routing contract`
2. `test: cover Jev intent routing decisions`
3. `feat: route Jev decisions to candidates or raw input`
4. `ci: run intent routing tests`
5. `docs: explain smarter Jev routing`
