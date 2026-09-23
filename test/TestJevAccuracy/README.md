# Jev 准确率测试

该测试使用带标准答案的中文上下文样本调用真实 Jev API，分别报告：

- `top1_accuracy`：Jev 第一选择与标准答案一致的比例。
- `effective_accuracy`：第一选择正确且达到小狼毫采用门槛的比例。

它属于联网集成测试，不放入普通 CI：模型输出可能变化，而且每次运行都会调用外部 API。

```powershell
$env:TYPESAFE_API_KEY = [Environment]::GetEnvironmentVariable('TYPESAFE_API_KEY', 'User')
python .\test\TestJevAccuracy\jev_accuracy.py `
  --output .\test\TestJevAccuracy\latest-results.json
```

增加样本时，应给出完整的 `context`、`preedit`、候选列表和唯一的 `expected`，并让正确答案分布在不同候选位置，避免只测第一候选偏好。
