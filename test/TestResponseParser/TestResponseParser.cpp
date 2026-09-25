// TestResponseParser.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <boost/detail/lightweight_test.hpp>
#include <JevIntentRouter.h>
#include <ResponseParser.h>
#include <string>

void test_1() {
  WCHAR resp[] = L"action=noop\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.empty());
}

void test_2() {
  WCHAR resp[] =
      L"action=commit\n"
      L"commit=教這句話上屏=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  ctx.aux.str = L"從前的值";
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit == L"教這句話上屏=3.14");
  BOOST_TEST(ctx.preedit.empty());
  BOOST_TEST(ctx.aux.str == L"從前的值");
  BOOST_TEST(ctx.cinfo.candies.empty());
}

void test_3() {
  WCHAR resp[] =
      L"action=ctx\n"
      L"ctx.preedit=寫作串=3.14\n"
      L"ctx.aux=sie'zuoh'chuan=3.14\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.preedit.str == L"寫作串=3.14");
  BOOST_TEST(ctx.preedit.attributes.empty());
  BOOST_TEST(ctx.aux.str == L"sie'zuoh'chuan=3.14");
}

void test_4() {
  WCHAR resp[] =
      L"action=commit,ctx\n"
      L"ctx.preedit=候選乙=3.14\n"
      L"ctx.preedit.cursor=0,3\n"
      L"ctx.cand.length=2\n"
      L"ctx.cand.0=候選甲\n"
      L"ctx.cand.1=候選乙\n"
      L"ctx.cand.cursor=1\n"
      L"ctx.cand.page=0/1\n";
  DWORD len = wcslen(resp);
  std::wstring commit;
  weasel::Context ctx;
  weasel::Status status;
  weasel::ResponseParser parser(&commit, &ctx, &status);
  parser(resp, len);
  BOOST_TEST(commit.empty());
  BOOST_TEST(ctx.preedit.str == L"候選乙=3.14");
  BOOST_ASSERT(1 == ctx.preedit.attributes.size());
  weasel::TextAttribute attr0 = ctx.preedit.attributes[0];
  BOOST_TEST_EQ(weasel::HIGHLIGHTED, attr0.type);
  BOOST_TEST_EQ(0, attr0.range.start);
  BOOST_TEST_EQ(3, attr0.range.end);
  BOOST_TEST(ctx.aux.empty());
  weasel::CandidateInfo& c = ctx.cinfo;
  BOOST_ASSERT(2 == c.candies.size());
  BOOST_TEST(c.candies[0].str == L"候選甲");
  BOOST_TEST(c.candies[1].str == L"候選乙");
  BOOST_TEST_EQ(1, c.highlighted);
  BOOST_TEST_EQ(0, c.currentPage);
  BOOST_TEST_EQ(1, c.totalPages);
}

void test_jev_choices() {
  using namespace weasel::jev;
  BOOST_TEST(ClassifyText("\xe6\x88\x91\xe7\x9f\xa5\xe9\x81\x93") == "han");
  BOOST_TEST(ClassifyText("OpenAI") == "latin");
  BOOST_TEST(ClassifyText("OpenAI\xe5\x8a\xa9\xe6\x89\x8b") == "mixed");
  BOOST_TEST(IsSafeRawInput("wozhidao"));
  BOOST_TEST(!IsSafeRawInput("wo zhi dao"));
  BOOST_TEST(!IsSafeRawInput("\xe6\x88\x91\xe7\x9f\xa5\xe9\x81\x93"));

  const std::vector<std::string> candidates = {
      "\xe6\x88\x91\xe6\x8c\x87\xe5\xae\x9a",
      "\xe6\x88\x91\xe7\x9f\xa5\xe9\x81\x93", "wozhidao"};
  const auto choices = BuildChoices(candidates, "wozhidao");
  BOOST_TEST_EQ(4, choices.size());
  BOOST_TEST(choices[0].key == "candidate_0");
  BOOST_TEST(choices[3].key == "raw_input");

  const auto limited = BuildChoices(candidates, "wozhidao", 2);
  BOOST_TEST_EQ(3, limited.size());
  BOOST_TEST(limited[1].key == "candidate_1");
  BOOST_TEST(limited[2].key == "raw_input");

  const auto without_raw = BuildChoices(candidates, "wo zhi dao", 10);
  BOOST_TEST_EQ(3, without_raw.size());
}

void test_jev_ranking_and_validation() {
  using namespace weasel::jev;
  RequestSnapshot request;
  request.generation = 7;
  request.session = 42;
  request.context = "\xe8\xbf\x99\xe4\xb8\xaa\xe9\x97\xae\xe9\xa2\x98";
  request.input = "wozhid";
  request.candidates = {"\xe6\x88\x91\xe6\x8c\x87\xe5\xae\x9a",
                        "\xe6\x88\x91\xe7\x9f\xa5\xe9\x81\x93",
                        "\xe6\x88\x91\xe5\x8f\xaa\xe5\xaf\xb9"};
  request.choices = BuildChoices(request.candidates, request.input);
  const auto decision = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"candidate_1","probabilities":{"candidate_0":0.12,"candidate_1":0.76,"candidate_2":0.12,"raw_input":0.0}}}})");
  BOOST_TEST(decision.has_value());
  if (!decision)
    return;
  BOOST_TEST_EQ(1, decision->selected.candidate_index);
  BOOST_TEST_EQ(1, decision->ranking[0].candidate_index);
  BOOST_TEST_EQ(0, decision->ranking[1].candidate_index);
  BOOST_TEST_EQ(2, decision->ranking[2].candidate_index);
  BOOST_TEST(MatchesSnapshot(*decision, 42, "wozhid", request.candidates));
  BOOST_TEST(!MatchesSnapshot(*decision, 43, "wozhid", request.candidates));
  BOOST_TEST(!MatchesSnapshot(*decision, 42, "changed", request.candidates));
  BOOST_TEST(!MatchesSnapshot(*decision, 42, "wozhid",
                              {request.candidates[0], request.candidates[2]}));
  BOOST_TEST(!MatchesSnapshot(*decision, 42, "wozhid", request.candidates,
                              0.80));

  const auto invalid = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"invented_text","confidence":0.99}}})");
  BOOST_TEST(!invalid);

  const auto incomplete = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"candidate_1","probabilities":{"candidate_0":0.12,"candidate_1":0.76,"candidate_2":0.12}}}})");
  BOOST_TEST(!incomplete);

  const auto selected_is_not_maximum = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"candidate_0","probabilities":{"candidate_0":0.12,"candidate_1":0.76,"candidate_2":0.12,"raw_input":0.0}}}})");
  BOOST_TEST(!selected_is_not_maximum);

  const auto out_of_range = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"candidate_1","probabilities":{"candidate_0":0.0,"candidate_1":1.01,"candidate_2":0.0,"raw_input":0.0}}}})");
  BOOST_TEST(!out_of_range);

  const auto raw = ParseDecision(
      request,
      R"({"answers":{"intent":{"choice":"raw_input","probabilities":{"candidate_0":0.05,"candidate_1":0.10,"candidate_2":0.05,"raw_input":0.80}}}})");
  BOOST_TEST(raw.has_value());
  if (raw) {
    BOOST_TEST(static_cast<int>(raw->selected.kind) ==
               static_cast<int>(ChoiceKind::raw_input));
    BOOST_TEST(raw->selected.text == request.input);
    BOOST_TEST(MatchesSnapshot(*raw, 42, "wozhid", request.candidates));
  }

  RequestSnapshot malformed = request;
  malformed.choices[0].candidate_index = 99;
  BOOST_TEST(!ParseDecision(
      malformed,
      R"({"answers":{"intent":{"choice":"candidate_1","probabilities":{"candidate_0":0.12,"candidate_1":0.76,"candidate_2":0.12,"raw_input":0.0}}}})"));
}

int _tmain(int argc, _TCHAR* argv[]) {
  test_1();
  test_2();
  test_3();
  test_4();
  test_jev_choices();
  test_jev_ranking_and_validation();

  return boost::report_errors();
}
