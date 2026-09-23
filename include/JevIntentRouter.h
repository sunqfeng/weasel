#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace weasel::jev {

enum class ChoiceKind { candidate, raw_input };

struct Choice {
  std::string key;
  std::string text;
  ChoiceKind kind = ChoiceKind::candidate;
  size_t candidate_index = 0;
};

struct RequestSnapshot {
  uint64_t generation = 0;
  uint32_t session = 0;
  std::string context;
  std::string input;
  std::vector<std::string> candidates;
  std::vector<Choice> choices;
};

struct RankedCandidate {
  size_t candidate_index = 0;
  double probability = 0;
};

struct Decision {
  RequestSnapshot request;
  Choice selected;
  double probability = 0;
  std::vector<RankedCandidate> ranking;
};

std::string ClassifyText(const std::string& text);
bool IsSafeRawInput(const std::string& input);
std::vector<Choice> BuildChoices(const std::vector<std::string>& candidates,
                                 const std::string& input,
                                 size_t limit = 10);
std::optional<Decision> ParseDecision(const RequestSnapshot& request,
                                      const std::string& json);
bool MatchesSnapshot(const Decision& decision,
                     uint32_t session,
                     const std::string& input,
                     const std::vector<std::string>& candidates,
                     double minimum_probability = 0.60);

}  // namespace weasel::jev
