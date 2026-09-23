# Jev 逻辑单元测试

`RimeWithWeasel/JevLogic.h` 和 `JevLogic.cpp` 是小狼毫实际使用的 Jev 纯逻辑模块。该目录中的 35 个测试直接编译这份生产代码，覆盖：

- 应用白名单的切分、清理、大小写折叠和去重；
- Jev JSON 响应解析、非法 choice、候选越界和评分来源；
- 概率分布的“60% 或 40% 且领先第二名 10 个百分点”规则，以及 `confidence` 回退值的独立 60% 门槛；
- Rime 候选重排后的文字重映射，以及候选缺失、拼音变化和重复文字的安全拒绝；
- 首页、候选数量、preedit、白名单、重复请求和最多 10 个候选的调度规则。

CI 使用以下命令在 Ubuntu 上编译运行，不依赖 Windows、WinHTTP 或 Rime：

```bash
g++ -std=c++17 -I RimeWithWeasel \
  RimeWithWeasel/JevLogic.cpp \
  test/TestJevLogic/TestJevLogic.cpp \
  -o test_jev_logic
./test_jev_logic
```

这些测试验证程序逻辑是否符合规则。模型对中文候选的实际准确率由 `test/TestJevAccuracy` 中的联网基准测试衡量。
