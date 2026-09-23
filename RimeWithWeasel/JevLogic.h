// JevLogic.h
//
// Jev 候选推荐功能中与平台无关的生产逻辑。该模块不依赖 Windows、
// WinHTTP 或 Rime，因此可以在 Linux CI 中直接编译和单元测试。

#pragma once

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include <boost/property_tree/ptree.hpp>

namespace jev {

enum class ScoreSource {
  kProbabilities,
  kConfidence,
  kMissing,
};

// session 使用 uint64_t，从而不依赖 Windows 的 WeaselSessionId/DWORD。
struct Request {
  uint64_t generation = 0;
  uint64_t session = 0;
  std::string context;
  std::string preedit;
  std::vector<std::string> candidates;
};

// 与 JevState::Result 对应。
struct Result : Request {
  size_t candidate = 0;
  double probability = 0;
  double runner_up_probability = -1;
  ScoreSource score_source = ScoreSource::kMissing;
};

const char* ScoreSourceName(ScoreSource source);

// 等价于原 LowerAscii：仅做 ASCII 范围的大小写折叠。
std::string LowerAscii(std::string value);

// 等价于原 ParseAllowedApps：按逗号/分号切分、去首尾空白、转小写、去空项。
std::set<std::string> ParseAllowedApps(const std::string& value);

// 等价于原 ParseJevResponse：解析 Jev API 返回的 JSON。
// 失败（JSON 格式错误 / choice 字段缺失或格式非法 / 候选下标越界）时返回
// std::nullopt，调用方应视为“本次不采用 Jev 结果”，而不是异常/崩溃。
std::optional<Result> ParseJevResponse(const Request& request,
                                       const std::string& json);

// A high-confidence choice is accepted directly. A moderately confident
// choice is accepted only when it has a clear lead over the runner-up.
bool IsRecommendationConfident(const Result& result);

// Map the selected text from the candidate snapshot sent to Jev onto the
// current Rime list. Reordering and additional current candidates are safe;
// changed preedit, removed request candidates, and duplicate selected text are
// rejected so a stale result cannot select an ambiguous candidate.
std::optional<size_t> RemapCandidate(
    const Result& result,
    const std::string& current_preedit,
    const std::vector<std::string>& current_candidates);

// 与 _ScheduleJev 里“是否应该发起一次新的 Jev 请求”的判定逻辑等价，
// 抽成不依赖 RimeContext / SessionStatus / m_jev 成员的纯函数，便于测试。
//
// client_app 可以保留原始大小写；函数在匹配 allowed_apps 前会做 ASCII
// 小写折叠。previous_signature 用于阻止相同上下文、拼音和候选重复请求。
//
// 返回值：
//   若应发起新请求，返回构造好的 Request（不含 generation，调用方在加锁后自行
//   递增赋值）以及本次的新 signature；否则返回 std::nullopt，调用方不应更新
//   previous_signature、也不应发起请求。
//
// 注意：最多只取前 10 个候选（与原实现一致）。
struct ScheduleDecision {
  Request request;
  std::string new_signature;
};

std::optional<ScheduleDecision> TryScheduleJev(
    int page_no,
    int num_candidates,
    bool has_preedit,
    const std::string& client_app,
    const std::set<std::string>& allowed_apps,
    const std::string& context,
    const std::string& preedit,
    const std::vector<std::string>& page_candidates,
    const std::string& previous_signature);

}  // namespace jev
