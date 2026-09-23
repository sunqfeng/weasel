#include "stdafx.h"

#include <JevIntentRouter.h>

#include <algorithm>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <cmath>
#include <sstream>

namespace weasel::jev {
namespace {

bool IsContinuationByte(unsigned char value) {
  return (value & 0xc0) == 0x80;
}

bool ContainsHan(const std::string& text) {
  for (size_t i = 0; i < text.size();) {
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    if (lead < 0x80) {
      ++i;
      continue;
    }
    size_t count = 0;
    uint32_t codepoint = 0;
    if ((lead & 0xe0) == 0xc0) {
      count = 2;
      codepoint = lead & 0x1f;
    } else if ((lead & 0xf0) == 0xe0) {
      count = 3;
      codepoint = lead & 0x0f;
    } else if ((lead & 0xf8) == 0xf0) {
      count = 4;
      codepoint = lead & 0x07;
    } else {
      ++i;
      continue;
    }
    if (i + count > text.size())
      break;
    bool valid = true;
    for (size_t j = 1; j < count; ++j) {
      const unsigned char next = static_cast<unsigned char>(text[i + j]);
      if (!IsContinuationByte(next)) {
        valid = false;
        break;
      }
      codepoint = (codepoint << 6) | (next & 0x3f);
    }
    if (!valid) {
      ++i;
      continue;
    }
    if ((codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
        (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
        (codepoint >= 0x20000 && codepoint <= 0x2fa1f)) {
      return true;
    }
    i += count;
  }
  return false;
}

const Choice* FindChoice(const RequestSnapshot& request,
                         const std::string& key) {
  const auto found =
      std::find_if(request.choices.begin(), request.choices.end(),
                   [&key](const Choice& choice) { return choice.key == key; });
  return found == request.choices.end() ? nullptr : &*found;
}

}  // namespace

std::string ClassifyText(const std::string& text) {
  const bool han = ContainsHan(text);
  const bool latin = std::any_of(text.begin(), text.end(), [](char value) {
    const unsigned char c = static_cast<unsigned char>(value);
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  });
  if (han && !latin)
    return "han";
  if (latin && !han)
    return "latin";
  return "mixed";
}

bool IsSafeRawInput(const std::string& input) {
  if (input.empty() || input.size() > 128)
    return false;
  bool has_letter = false;
  for (unsigned char c : input) {
    if (c < 0x21 || c > 0x7e)
      return false;
    has_letter = has_letter || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
  }
  return has_letter;
}

std::vector<Choice> BuildChoices(const std::vector<std::string>& candidates,
                                 const std::string& input,
                                 size_t limit) {
  std::vector<Choice> choices;
  const size_t count = (std::min)(candidates.size(), limit);
  choices.reserve(count + 1);
  for (size_t i = 0; i < count; ++i) {
    if (!candidates[i].empty())
      choices.push_back({"candidate_" + std::to_string(i), candidates[i],
                         ChoiceKind::candidate, i});
  }
  if (IsSafeRawInput(input))
    choices.push_back({"raw_input", input, ChoiceKind::raw_input, 0});
  return choices;
}

std::optional<Decision> ParseDecision(const RequestSnapshot& request,
                                      const std::string& json) {
  try {
    boost::property_tree::ptree response;
    std::istringstream input(json);
    boost::property_tree::read_json(input, response);
    const auto& answer = response.get_child("answers.intent");
    const std::string selected_key = answer.get<std::string>("choice");
    const Choice* selected = FindChoice(request, selected_key);
    if (!selected)
      return std::nullopt;

    Decision decision;
    decision.request = request;
    decision.selected = *selected;
    const auto probabilities = answer.get_child_optional("probabilities");
    double maximum_probability = -1;
    for (const Choice& choice : request.choices) {
      double probability = 0;
      if (probabilities)
        probability = probabilities->get<double>(choice.key, 0);
      if (choice.key == selected_key && !probabilities)
        probability = answer.get<double>("confidence", 0);
      if (!std::isfinite(probability) || probability < 0 || probability > 1)
        return std::nullopt;
      if (choice.key == selected_key)
        decision.probability = probability;
      maximum_probability = (std::max)(maximum_probability, probability);
      if (choice.kind == ChoiceKind::candidate)
        decision.ranking.push_back({choice.candidate_index, probability});
    }
    std::stable_sort(
        decision.ranking.begin(), decision.ranking.end(),
        [](const RankedCandidate& left, const RankedCandidate& right) {
          return left.probability > right.probability;
        });
    if (probabilities && decision.probability < maximum_probability)
      return std::nullopt;
    return decision;
  } catch (...) {
    return std::nullopt;
  }
}

bool MatchesSnapshot(const Decision& decision,
                     uint32_t session,
                     const std::string& input,
                     const std::vector<std::string>& candidates,
                     double minimum_probability) {
  if (decision.request.session != session || decision.request.input != input ||
      decision.probability < minimum_probability ||
      decision.request.candidates.size() > candidates.size()) {
    return false;
  }
  for (size_t i = 0; i < decision.request.candidates.size(); ++i) {
    if (decision.request.candidates[i] != candidates[i])
      return false;
  }
  return true;
}

}  // namespace weasel::jev
