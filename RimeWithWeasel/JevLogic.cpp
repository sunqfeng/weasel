#include "JevLogic.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <boost/property_tree/json_parser.hpp>

namespace jev {

std::string LowerAscii(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  });
  return value;
}

std::set<std::string> ParseAllowedApps(const std::string& value) {
  std::set<std::string> apps;
  std::string app;
  const auto add = [&]() {
    app.erase(0, app.find_first_not_of(" \t"));
    const auto end = app.find_last_not_of(" \t");
    if (end != std::string::npos)
      app.erase(end + 1);
    if (!app.empty())
      apps.insert(LowerAscii(app));
    app.clear();
  };
  for (char c : value) {
    if (c == ',' || c == ';')
      add();
    else
      app.push_back(c);
  }
  add();
  return apps;
}

std::optional<Result> ParseJevResponse(const Request& request,
                                       const std::string& json) {
  try {
    boost::property_tree::ptree response;
    std::istringstream input(json);
    boost::property_tree::read_json(input, response);
    const std::string choice =
        response.get<std::string>("answers.candidate.choice");
    if (choice.size() < 2 || choice[0] != 'c' ||
        !std::all_of(choice.begin() + 1, choice.end(), [](char c) {
          return std::isdigit(static_cast<unsigned char>(c));
        }))
      return std::nullopt;
    const size_t candidate = std::stoul(choice.substr(1));
    if (candidate >= request.candidates.size())
      return std::nullopt;

    Result result;
    static_cast<Request&>(result) = request;
    result.candidate = candidate;
    result.probability = response.get<double>(
        "answers.candidate.probabilities." + choice,
        response.get<double>("answers.candidate.confidence", 0));
    for (size_t i = 0; i < request.candidates.size(); ++i) {
      if (i == candidate)
        continue;
      const auto probability = response.get_optional<double>(
          "answers.candidate.probabilities.c" + std::to_string(i));
      if (probability && *probability > result.runner_up_probability)
        result.runner_up_probability = *probability;
    }
    return result;
  } catch (...) {
    return std::nullopt;
  }
}

bool IsRecommendationConfident(const Result& result) {
  constexpr double kHighConfidence = 0.60;
  constexpr double kMinimumConfidence = 0.40;
  constexpr double kMinimumLead = 0.10;
  if (result.probability >= kHighConfidence)
    return true;
  return result.runner_up_probability >= 0 &&
         result.probability >= kMinimumConfidence &&
         result.probability - result.runner_up_probability >= kMinimumLead;
}

std::optional<ScheduleDecision> TryScheduleJev(
    int page_no,
    int num_candidates,
    bool has_preedit,
    const std::string& client_app,
    const std::set<std::string>& allowed_apps,
    const std::string& context,
    const std::string& preedit,
    const std::vector<std::string>& page_candidates,
    const std::string& previous_signature) {
  if (page_no != 0 || num_candidates < 2 || !has_preedit)
    return std::nullopt;

  // 原实现依赖调用方（_ReadClientInfo）已经把 client_app 转成小写；
  // 这里额外做一次防御性折叠，避免调用方大小写处理被改动时静默失效。
  if (allowed_apps.find(LowerAscii(client_app)) == allowed_apps.end())
    return std::nullopt;

  ScheduleDecision decision;
  decision.request.context = context;
  decision.request.preedit = preedit;

  const size_t available_candidates =
      std::min<size_t>(page_candidates.size(), num_candidates);
  const size_t candidate_count = std::min<size_t>(available_candidates, 10);
  decision.request.candidates.reserve(candidate_count);
  std::string signature = context + "\n" + preedit;
  for (size_t i = 0; i < candidate_count; ++i) {
    decision.request.candidates.push_back(page_candidates[i]);
    signature.append("\n").append(page_candidates[i]);
  }

  if (signature == previous_signature)
    return std::nullopt;

  decision.new_signature = std::move(signature);
  return decision;
}

}  // namespace jev
