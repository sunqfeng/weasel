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

struct Utf8Unit {
  size_t offset = 0;
  uint32_t codepoint = 0;
};

std::vector<Utf8Unit> DecodeUtf8Units(const std::string& text) {
  std::vector<Utf8Unit> units;
  units.reserve(text.size());
  for (size_t i = 0; i < text.size();) {
    const size_t offset = i;
    const unsigned char lead = static_cast<unsigned char>(text[i]);
    size_t count = 1;
    uint32_t codepoint = lead;
    if ((lead & 0xe0) == 0xc0) {
      count = 2;
      codepoint = lead & 0x1f;
    } else if ((lead & 0xf0) == 0xe0) {
      count = 3;
      codepoint = lead & 0x0f;
    } else if ((lead & 0xf8) == 0xf0) {
      count = 4;
      codepoint = lead & 0x07;
    }
    if (i + count > text.size())
      count = 1;
    for (size_t j = 1; j < count; ++j) {
      const unsigned char next = static_cast<unsigned char>(text[i + j]);
      if (!IsContinuationByte(next)) {
        count = 1;
        codepoint = lead;
        break;
      }
      codepoint = (codepoint << 6) | (next & 0x3f);
    }
    units.push_back({offset, codepoint});
    i += count;
  }
  return units;
}

bool IsContextBoundary(uint32_t codepoint) {
  switch (codepoint) {
    case '\n':
    case '\r':
    case ',':
    case '.':
    case ';':
    case '!':
    case '?':
    case 0x3002:  // 。
    case 0xff0c:  // ，
    case 0xff1b:  // ；
    case 0xff01:  // ！
    case 0xff1f:  // ？
      return true;
    default:
      return false;
  }
}

bool ContainsHan(const std::string& text) {
  for (const auto& unit : DecodeUtf8Units(text)) {
    const uint32_t codepoint = unit.codepoint;
    if ((codepoint >= 0x3400 && codepoint <= 0x4dbf) ||
        (codepoint >= 0x4e00 && codepoint <= 0x9fff) ||
        (codepoint >= 0x20000 && codepoint <= 0x2fa1f)) {
      return true;
    }
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

bool HasValidChoices(const RequestSnapshot& request) {
  bool has_raw_input = false;
  for (size_t i = 0; i < request.choices.size(); ++i) {
    const Choice& choice = request.choices[i];
    if (choice.key.empty() || choice.text.empty())
      return false;
    if (choice.kind == ChoiceKind::candidate) {
      if (choice.candidate_index >= request.candidates.size() ||
          choice.key != "candidate_" + std::to_string(choice.candidate_index) ||
          choice.text != request.candidates[choice.candidate_index]) {
        return false;
      }
    } else if (choice.kind == ChoiceKind::raw_input) {
      if (has_raw_input || choice.key != "raw_input" ||
          choice.text != request.input || !IsSafeRawInput(choice.text)) {
        return false;
      }
      has_raw_input = true;
    } else {
      return false;
    }
    for (size_t j = i + 1; j < request.choices.size(); ++j) {
      if (choice.key == request.choices[j].key ||
          (choice.kind == ChoiceKind::candidate &&
           request.choices[j].kind == ChoiceKind::candidate &&
           choice.candidate_index == request.choices[j].candidate_index)) {
        return false;
      }
    }
  }
  return !request.choices.empty();
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

std::string UpdateContextWindow(const std::string& context,
                                const std::string& committed_text,
                                size_t maximum_codepoints) {
  std::string combined = context + committed_text;
  if (!maximum_codepoints)
    return {};
  const auto units = DecodeUtf8Units(combined);
  if (units.size() <= maximum_codepoints)
    return combined;

  const size_t hard_start = units.size() - maximum_codepoints;
  std::vector<size_t> boundaries;
  for (size_t i = hard_start; i < units.size(); ++i) {
    const bool ends_boundary_run =
        IsContextBoundary(units[i].codepoint) &&
        (i + 1 == units.size() || !IsContextBoundary(units[i + 1].codepoint));
    if (ends_boundary_run)
      boundaries.push_back(i);
  }

  size_t start = hard_start;
  if (boundaries.size() >= 2) {
    start = boundaries[boundaries.size() - 2] + 1;
  } else if (boundaries.size() == 1 && boundaries.front() + 1 < units.size()) {
    start = boundaries.front() + 1;
  }
  return combined.substr(units[start].offset);
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
    if (!HasValidChoices(request))
      return std::nullopt;
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
    const auto confidence = answer.get_optional<double>("confidence");
    if (!probabilities && !confidence)
      return std::nullopt;
    if (probabilities) {
      std::vector<std::string> probability_keys;
      probability_keys.reserve(probabilities->size());
      for (const auto& probability : *probabilities) {
        if (!FindChoice(request, probability.first) ||
            std::find(probability_keys.begin(), probability_keys.end(),
                      probability.first) != probability_keys.end()) {
          return std::nullopt;
        }
        probability_keys.push_back(probability.first);
      }
      if (probability_keys.size() != request.choices.size())
        return std::nullopt;
    }
    double maximum_probability = -1;
    const Choice* maximum_choice = nullptr;
    for (const Choice& choice : request.choices) {
      double probability = 0;
      if (probabilities) {
        const auto value = probabilities->get_optional<double>(choice.key);
        if (!value)
          return std::nullopt;
        probability = *value;
      } else if (choice.key == selected_key) {
        probability = *confidence;
      }
      if (!std::isfinite(probability) || probability < 0 || probability > 1)
        return std::nullopt;
      if (choice.key == selected_key)
        decision.probability = probability;
      if (probability > maximum_probability) {
        maximum_probability = probability;
        maximum_choice = &choice;
      }
      if (choice.kind == ChoiceKind::candidate)
        decision.ranking.push_back({choice.candidate_index, probability});
    }
    std::stable_sort(
        decision.ranking.begin(), decision.ranking.end(),
        [](const RankedCandidate& left, const RankedCandidate& right) {
          return left.probability > right.probability;
        });
    if (!maximum_choice || maximum_choice->key != selected_key)
      return std::nullopt;
    if (selected->kind == ChoiceKind::candidate &&
        (decision.ranking.empty() || decision.ranking.front().candidate_index !=
                                         selected->candidate_index)) {
      return std::nullopt;
    }
    for (size_t i = 0; i < request.candidates.size(); ++i) {
      const auto ranked =
          std::find_if(decision.ranking.begin(), decision.ranking.end(),
                       [i](const RankedCandidate& candidate) {
                         return candidate.candidate_index == i;
                       });
      if (ranked == decision.ranking.end())
        decision.ranking.push_back({i, 0});
    }
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
