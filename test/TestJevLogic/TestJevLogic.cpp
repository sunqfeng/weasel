// TestJevLogic.cpp
//
// 针对 JevLogic.h/.cpp 中平台无关逻辑的单元测试。
// 沿用项目现有 test/TestResponseParser
// 的风格（boost::detail::lightweight_test）， 但入口用普通
// main()，可跨平台（含本沙箱的 Linux 环境）直接编译运行， 不依赖 Windows 的
// _tmain / _TCHAR。

#include <boost/detail/lightweight_test.hpp>

#include "JevLogic.h"

using jev::IsRecommendationConfident;
using jev::LowerAscii;
using jev::ParseAllowedApps;
using jev::ParseJevResponse;
using jev::RemapCandidate;
using jev::Request;
using jev::Result;
using jev::ScoreSource;
using jev::TryScheduleJev;

// ---------- LowerAscii ----------

void test_lower_ascii_basic() {
  BOOST_TEST(LowerAscii("Notepad.EXE") == "notepad.exe");
  BOOST_TEST(LowerAscii("") == "");
  BOOST_TEST(LowerAscii("already-lower") == "already-lower");
}

// ---------- ParseAllowedApps ----------

void test_parse_allowed_apps_basic() {
  auto apps = ParseAllowedApps(" Notepad.exe; winword.exe ");
  BOOST_TEST(apps.count("notepad.exe") == 1);
  BOOST_TEST(apps.count("winword.exe") == 1);
  BOOST_TEST(apps.size() == 2u);
}

void test_parse_allowed_apps_comma_and_semicolon_mixed() {
  auto apps = ParseAllowedApps("a.exe,b.exe;c.exe");
  BOOST_TEST(apps.size() == 3u);
  BOOST_TEST(apps.count("a.exe") == 1);
  BOOST_TEST(apps.count("b.exe") == 1);
  BOOST_TEST(apps.count("c.exe") == 1);
}

void test_parse_allowed_apps_empty_and_whitespace_only() {
  BOOST_TEST(ParseAllowedApps("").empty());
  BOOST_TEST(ParseAllowedApps("   ").empty());
  // 连续分隔符 / 首尾分隔符产生的空项应被丢弃，而不是插入空字符串。
  auto apps = ParseAllowedApps(";, a.exe ,,; ");
  BOOST_TEST(apps.size() == 1u);
  BOOST_TEST(apps.count("a.exe") == 1);
}

void test_parse_allowed_apps_dedup_case_insensitive() {
  // 大小写不同但实际是同一个程序名，应该折叠成一项。
  auto apps = ParseAllowedApps("Notepad.exe,NOTEPAD.EXE,notepad.exe");
  BOOST_TEST(apps.size() == 1u);
  BOOST_TEST(apps.count("notepad.exe") == 1);
}

// ---------- ParseJevResponse ----------

Request MakeRequest(std::vector<std::string> candidates) {
  Request r;
  r.session = 1;
  r.context = "ctx";
  r.preedit = "hanzi";
  r.candidates = std::move(candidates);
  return r;
}

void test_parse_response_normal_with_probabilities() {
  auto req =
      MakeRequest({"\xe6\xb1\x89\xe5\xad\x90", "\xe6\xb1\x89\xe5\xad\x97"});
  auto result = ParseJevResponse(
      req,
      R"({"answers":{"candidate":{"choice":"c1","probabilities":{"c0":0.02,"c1":0.98},"confidence":0.97}}})");
  BOOST_ASSERT(result.has_value());
  BOOST_TEST(result->candidate == 1u);
  BOOST_TEST(result->probability == 0.98);
  BOOST_TEST(result->runner_up_probability == 0.02);
  BOOST_TEST(static_cast<int>(result->score_source) ==
             static_cast<int>(ScoreSource::kProbabilities));
}

void test_parse_response_falls_back_to_confidence() {
  // probabilities 里没有对应 choice 的 key 时，应回退到 confidence。
  auto req = MakeRequest({"A", "B"});
  auto result = ParseJevResponse(
      req, R"({"answers":{"candidate":{"choice":"c1","confidence":0.81}}})");
  BOOST_ASSERT(result.has_value());
  BOOST_TEST(result->candidate == 1u);
  BOOST_TEST(result->probability == 0.81);
  BOOST_TEST(result->runner_up_probability == -1.0);
  BOOST_TEST(static_cast<int>(result->score_source) ==
             static_cast<int>(ScoreSource::kConfidence));
}

void test_parse_response_no_probability_info_defaults_zero() {
  // 既没有 probabilities.c1 也没有 confidence 时，默认
  // 0（应被上层阈值判断拒绝采用）。
  auto req = MakeRequest({"A", "B"});
  auto result =
      ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"c0"}}})");
  BOOST_ASSERT(result.has_value());
  BOOST_TEST(result->probability == 0.0);
  BOOST_TEST(static_cast<int>(result->score_source) ==
             static_cast<int>(ScoreSource::kMissing));
}

void test_parse_response_choice_out_of_range() {
  auto req = MakeRequest({"A", "B"});  // 只有 2 个候选，下标 0/1 合法
  auto result = ParseJevResponse(
      req, R"({"answers":{"candidate":{"choice":"c5","confidence":0.9}}})");
  BOOST_TEST(!result.has_value());
}

void test_parse_response_malformed_choice_field() {
  auto req = MakeRequest({"A", "B"});
  // choice 不以 'c' 开头
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"x1"}}})")
           .has_value());
  // choice 长度不足 2（只有一个字符，没有数字部分）
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"c"}}})")
           .has_value());
  // choice 数字部分不是合法数字
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"cX"}}})")
           .has_value());
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"c1junk"}}})")
           .has_value());
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"c-1"}}})")
           .has_value());
}

void test_confidence_accepts_high_probability() {
  Result result;
  result.score_source = ScoreSource::kProbabilities;
  result.probability = 0.60;
  BOOST_TEST(IsRecommendationConfident(result));
}

void test_confidence_accepts_clear_contextual_lead() {
  Result result;
  result.score_source = ScoreSource::kProbabilities;
  result.probability = 0.48;
  result.runner_up_probability = 0.21;
  BOOST_TEST(IsRecommendationConfident(result));
}

void test_confidence_rejects_ambiguous_choice() {
  Result result;
  result.score_source = ScoreSource::kProbabilities;
  result.probability = 0.44;
  result.runner_up_probability = 0.38;
  BOOST_TEST(!IsRecommendationConfident(result));
}

void test_confidence_fallback_requires_high_threshold() {
  Result result;
  result.score_source = ScoreSource::kConfidence;
  result.probability = 0.44;
  result.runner_up_probability = 0.10;
  BOOST_TEST(!IsRecommendationConfident(result));
  result.probability = 0.60;
  BOOST_TEST(IsRecommendationConfident(result));
}

void test_missing_score_is_never_confident() {
  Result result;
  result.score_source = ScoreSource::kMissing;
  result.probability = 1.0;
  BOOST_TEST(!IsRecommendationConfident(result));
}

// ---------- RemapCandidate ----------

Result MakeResult(size_t candidate, std::vector<std::string> candidates) {
  Result result;
  result.preedit = "pe";
  result.candidate = candidate;
  result.candidates = std::move(candidates);
  return result;
}

void test_remap_candidate_unchanged_order() {
  auto result = MakeResult(1, {"a", "b", "c"});
  auto mapped = RemapCandidate(result, "pe", {"a", "b", "c"});
  BOOST_ASSERT(mapped.has_value());
  BOOST_TEST(*mapped == 1u);
}

void test_remap_candidate_reordered() {
  auto result = MakeResult(1, {"a", "b", "c"});
  auto mapped = RemapCandidate(result, "pe", {"c", "a", "b"});
  BOOST_ASSERT(mapped.has_value());
  BOOST_TEST(*mapped == 2u);
}

void test_remap_candidate_allows_extra_current_candidates() {
  auto result = MakeResult(0, {"a", "b"});
  auto mapped = RemapCandidate(result, "pe", {"x", "b", "a"});
  BOOST_ASSERT(mapped.has_value());
  BOOST_TEST(*mapped == 2u);
}

void test_remap_candidate_rejects_removed_candidate() {
  auto result = MakeResult(1, {"a", "b", "c"});
  BOOST_TEST(!RemapCandidate(result, "pe", {"a", "b"}).has_value());
}

void test_remap_candidate_rejects_changed_preedit() {
  auto result = MakeResult(1, {"a", "b"});
  BOOST_TEST(!RemapCandidate(result, "other", {"a", "b"}).has_value());
}

void test_remap_candidate_rejects_ambiguous_selected_text() {
  auto result = MakeResult(1, {"a", "b"});
  BOOST_TEST(!RemapCandidate(result, "pe", {"b", "a", "b"}).has_value());
}

void test_parse_response_missing_choice_field() {
  auto req = MakeRequest({"A", "B"});
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{}}})").has_value());
}

void test_parse_response_invalid_json() {
  auto req = MakeRequest({"A", "B"});
  BOOST_TEST(!ParseJevResponse(req, "not json at all").has_value());
  BOOST_TEST(!ParseJevResponse(req, "").has_value());
}

void test_parse_response_empty_candidate_list_always_out_of_range() {
  auto req = MakeRequest({});
  BOOST_TEST(
      !ParseJevResponse(req, R"({"answers":{"candidate":{"choice":"c0"}}})")
           .has_value());
}

// ---------- TryScheduleJev ----------

void test_schedule_rejects_when_not_first_page() {
  auto decision = TryScheduleJev(/*page_no=*/1, /*num_candidates=*/5,
                                 /*has_preedit=*/true, "notepad.exe",
                                 {"notepad.exe"}, "ctx", "pe", {"a", "b"}, "");
  BOOST_TEST(!decision.has_value());
}

void test_schedule_rejects_when_fewer_than_two_candidates() {
  auto decision = TryScheduleJev(0, /*num_candidates=*/1, true, "notepad.exe",
                                 {"notepad.exe"}, "ctx", "pe", {"a"}, "");
  BOOST_TEST(!decision.has_value());
}

void test_schedule_rejects_when_no_preedit() {
  auto decision = TryScheduleJev(0, 5, /*has_preedit=*/false, "notepad.exe",
                                 {"notepad.exe"}, "ctx", "pe", {"a", "b"}, "");
  BOOST_TEST(!decision.has_value());
}

void test_schedule_rejects_when_app_not_whitelisted() {
  auto decision = TryScheduleJev(0, 5, true, "chrome.exe", {"notepad.exe"},
                                 "ctx", "pe", {"a", "b"}, "");
  BOOST_TEST(!decision.has_value());
}

void test_schedule_accepts_case_insensitive_app_match() {
  // 即便调用方传入的 client_app
  // 大小写与白名单不同（防御性场景），也应正确匹配。
  auto decision = TryScheduleJev(0, 5, true, "NOTEPAD.EXE", {"notepad.exe"},
                                 "ctx", "pe", {"a", "b"}, "");
  BOOST_ASSERT(decision.has_value());
  BOOST_TEST(decision->request.candidates.size() == 2u);
}

void test_schedule_dedups_identical_signature() {
  auto first = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"}, "ctx",
                              "pe", {"a", "b"}, "");
  BOOST_ASSERT(first.has_value());
  // 用上一次返回的 signature 作为 previous_signature 再请求一次，应被去重。
  auto second = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"},
                               "ctx", "pe", {"a", "b"}, first->new_signature);
  BOOST_TEST(!second.has_value());
}

void test_schedule_reschedules_when_candidates_change() {
  auto first = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"}, "ctx",
                              "pe", {"a", "b"}, "");
  BOOST_ASSERT(first.has_value());
  // 候选内容变化（哪怕只是顺序变化）应触发新的请求，而不是被去重逻辑吞掉。
  auto second = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"},
                               "ctx", "pe", {"b", "a"}, first->new_signature);
  BOOST_ASSERT(second.has_value());
  BOOST_TEST(second->new_signature != first->new_signature);
}

void test_schedule_reschedules_when_context_changes() {
  auto first = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"},
                              "ctx1", "pe", {"a", "b"}, "");
  BOOST_ASSERT(first.has_value());
  // 只有 committed context 变了，preedit/candidates 都没变，也应该触发新请求，
  // 因为 signature 是 context+preedit+candidates 的拼接。
  auto second = TryScheduleJev(0, 5, true, "notepad.exe", {"notepad.exe"},
                               "ctx2", "pe", {"a", "b"}, first->new_signature);
  BOOST_ASSERT(second.has_value());
  BOOST_TEST(second->new_signature != first->new_signature);
}

void test_schedule_truncates_to_first_ten_candidates() {
  std::vector<std::string> many;
  for (int i = 0; i < 15; ++i)
    many.push_back("cand" + std::to_string(i));
  auto decision = TryScheduleJev(0, 15, true, "notepad.exe", {"notepad.exe"},
                                 "ctx", "pe", many, "");
  BOOST_ASSERT(decision.has_value());
  BOOST_TEST(decision->request.candidates.size() == 10u);
  BOOST_TEST(decision->request.candidates.front() == "cand0");
  BOOST_TEST(decision->request.candidates.back() == "cand9");
}

void test_schedule_respects_reported_candidate_count() {
  auto decision = TryScheduleJev(0, 2, true, "notepad.exe", {"notepad.exe"},
                                 "ctx", "pe", {"a", "b", "stale"}, "");
  BOOST_ASSERT(decision.has_value());
  BOOST_TEST(decision->request.candidates.size() == 2u);
}

// 用于警示：signature 拼接用 "\n" 分隔候选，若某个候选本身包含 "\n"，
// 理论上可能与相邻候选的边界产生歧义（signature 碰撞导致误判去重）。
// 目前的输入源（Rime 候选文本）正常不会包含换行，这里只是记录这个隐含假设，
// 便于以后候选来源变化时有据可查，而不是断言一个当前必然成立的具体值。
void test_schedule_signature_assumption_no_newline_in_candidates() {
  auto decision = TryScheduleJev(0, 2, true, "notepad.exe", {"notepad.exe"},
                                 "ctx", "pe", {"a", "b"}, "");
  BOOST_ASSERT(decision.has_value());
  BOOST_TEST(decision->new_signature.find('\n') != std::string::npos);
}

int main() {
  test_lower_ascii_basic();

  test_parse_allowed_apps_basic();
  test_parse_allowed_apps_comma_and_semicolon_mixed();
  test_parse_allowed_apps_empty_and_whitespace_only();
  test_parse_allowed_apps_dedup_case_insensitive();

  test_parse_response_normal_with_probabilities();
  test_parse_response_falls_back_to_confidence();
  test_parse_response_no_probability_info_defaults_zero();
  test_parse_response_choice_out_of_range();
  test_parse_response_malformed_choice_field();
  test_parse_response_missing_choice_field();
  test_parse_response_invalid_json();
  test_parse_response_empty_candidate_list_always_out_of_range();
  test_confidence_accepts_high_probability();
  test_confidence_accepts_clear_contextual_lead();
  test_confidence_rejects_ambiguous_choice();
  test_confidence_fallback_requires_high_threshold();
  test_missing_score_is_never_confident();

  test_remap_candidate_unchanged_order();
  test_remap_candidate_reordered();
  test_remap_candidate_allows_extra_current_candidates();
  test_remap_candidate_rejects_removed_candidate();
  test_remap_candidate_rejects_changed_preedit();
  test_remap_candidate_rejects_ambiguous_selected_text();

  test_schedule_rejects_when_not_first_page();
  test_schedule_rejects_when_fewer_than_two_candidates();
  test_schedule_rejects_when_no_preedit();
  test_schedule_rejects_when_app_not_whitelisted();
  test_schedule_accepts_case_insensitive_app_match();
  test_schedule_dedups_identical_signature();
  test_schedule_reschedules_when_candidates_change();
  test_schedule_reschedules_when_context_changes();
  test_schedule_truncates_to_first_ten_candidates();
  test_schedule_respects_reported_candidate_count();
  test_schedule_signature_assumption_no_newline_in_candidates();

  return boost::report_errors();
}
