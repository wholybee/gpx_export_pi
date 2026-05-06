/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Waypoint/route naming policy - implementation                         *
 **************************************************************************/

#include "name_policy.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <set>
#include <sstream>

namespace name_policy {

std::string ApplyPolicy(const std::string& original, const NamePolicy& policy,
                        int seq_index) {
  std::string result;

  if (policy.sequential_numbering) {
    // Generate a sequential number with zero-padding.
    std::ostringstream ss;
    if (!policy.prefix.empty()) {
      ss << policy.prefix;
    }
    ss << std::setw(policy.width) << std::setfill('0')
       << (policy.start_number + seq_index);
    result = ss.str();
  } else if (policy.preserve_names) {
    result = original;
    if (!policy.prefix.empty() && !result.empty()) {
      result = policy.prefix + result;
    } else if (!policy.prefix.empty()) {
      // Name is blank; use prefix + index as fallback.
      std::ostringstream ss;
      ss << policy.prefix << std::setw(policy.width) << std::setfill('0')
         << (policy.start_number + seq_index);
      result = ss.str();
    }
  }

  // If name is still empty, generate a fallback.
  if (result.empty()) {
    std::ostringstream ss;
    ss << "WPT" << std::setw(policy.width) << std::setfill('0')
       << (policy.start_number + seq_index);
    result = ss.str();
  }

  // Apply strip_special_chars before strip_spaces so spaces can be
  // removed in the next step if desired.
  if (policy.strip_special_chars) {
    std::string cleaned;
    cleaned.reserve(result.size());
    for (char c : result) {
      if (std::isalnum(static_cast<unsigned char>(c)) || c == ' ' ||
          c == '-' || c == '_') {
        cleaned += c;
      }
    }
    result = cleaned;
  }

  if (policy.strip_spaces) {
    result.erase(std::remove(result.begin(), result.end(), ' '), result.end());
  }

  if (policy.uppercase) {
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return std::toupper(c); });
  }

  if (policy.max_length.has_value() && result.size() > *policy.max_length) {
    result = result.substr(0, *policy.max_length);
  }

  return result;
}

std::vector<std::string> ApplyPolicyBatch(
    const std::vector<std::string>& originals, const NamePolicy& policy,
    bool require_unique) {
  std::vector<std::string> results;
  results.reserve(originals.size());

  for (size_t i = 0; i < originals.size(); ++i) {
    results.push_back(ApplyPolicy(originals[i], policy, static_cast<int>(i)));
  }

  if (!require_unique) return results;

  // Ensure uniqueness by appending a suffix to duplicates.
  std::set<std::string> seen;
  for (auto& name : results) {
    std::string candidate = name;
    int suffix = 2;
    while (seen.count(candidate)) {
      std::ostringstream ss;
      ss << name << "_" << suffix++;
      candidate = ss.str();
      // Respect max_length if set.
      if (policy.max_length.has_value() &&
          candidate.size() > *policy.max_length) {
        // Shorten the base to make room for the suffix.
        std::string sfx = "_" + std::to_string(suffix - 1);
        size_t base_len = *policy.max_length - sfx.size();
        if (base_len > 0 && base_len < name.size()) {
          candidate = name.substr(0, base_len) + sfx;
        }
      }
    }
    name = candidate;
    seen.insert(name);
  }

  return results;
}

}  // namespace name_policy
