#include "JevLogic.h"

#include <algorithm>
#include <cctype>
#include <sstream>

#include <boost/property_tree/json_parser.hpp>

namespace jev {

const char* ScoreSourceName(ScoreSource source) {
  switch (source) {
    case ScoreSource::kProbabilities:
      return "probabilities";
    case ScoreSource::kConfidence:
      return "confidence";
    case ScoreSource::kMissing:
      return "missing";
  }
  return "missing";
}

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
    const auto choice_probability = response.get_optional<double>(
        "answers.candidate.probabilities." + choice);
    if (choice_probability) {
      result.probability = *choice_probability;
      result.score_source = ScoreSource::kProbabilities;
      for (size_t i = 0; i < request.candidates.size(); ++i) {
        if (i == candidate)
          continue;
        const auto probability = response.get_optional<double>(
            "answers.candidate.probabilities.c" + std::to_string(i));
        if (probability && *probability > result.runner_up_probability)
          result.runner_up_probability = *probability;
      }
    } else if (const auto confidence = response.get_optional<double>(
                   "answers.candidate.confidence")) {
      result.probability = *confidence;
      result.score_source = ScoreSource::kConfidence;
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
  if (result.score_source == ScoreSource::kMissing)
    return false;
  if (result.probability >= kHighConfidence)
    return true;
  if (result.score_source != ScoreSource::kProbabilities)
    return false;
  return result.runner_up_probability >= 0 &&
         result.probability >= kMinimumConfidence &&
         result.probability - result.runner_up_probability >= kMinimumLead;
}

std::optional<size_t> RemapCandidate(
    const Result& result,
    const std::string& current_preedit,
    const std::vector<std::string>& current_candidates) {
  if (result.preedit != current_preedit ||
      result.candidate >= result.candidates.size())
    return std::nullopt;

  std::vector<bool> matched(current_candidates.size(), false);
  for (const auto& requested : result.candidates) {
    size_t current_index = 0;
    while (current_index < current_candidates.size() &&
           (matched[current_index] ||
            current_candidates[current_index] != requested))
      ++current_index;
    if (current_index == current_candidates.size())
      return std::nullopt;
    matched[current_index] = true;
  }

  const std::string& selected = result.candidates[result.candidate];
  std::optional<size_t> mapped;
  for (size_t i = 0; i < current_candidates.size(); ++i) {
    if (current_candidates[i] != selected)
      continue;
    if (mapped)
      return std::nullopt;
    mapped = i;
  }
  return mapped;
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
