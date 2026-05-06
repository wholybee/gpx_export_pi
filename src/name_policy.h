/**************************************************************************
 *   GPX Export Plugin for OpenCPN                                         *
 *   Waypoint/route naming policy                                          *
 **************************************************************************/

#ifndef _NAME_POLICY_H_
#define _NAME_POLICY_H_

#include <optional>
#include <string>
#include <vector>

/** Controls how waypoint and route names are transformed during export. */
struct NamePolicy {
  bool preserve_names = true;
  bool sequential_numbering = false;
  std::string prefix;
  int start_number = 1;
  int width = 3;  // zero-padded width for sequential numbers
  bool uppercase = false;
  bool strip_spaces = false;
  bool strip_special_chars = false;
  std::optional<size_t> max_length;
};

namespace name_policy {

/**
 * Apply the name policy to a single name.
 *
 * @param original   The original waypoint/route name.
 * @param policy     The naming policy to apply.
 * @param seq_index  Sequential index (0-based), used when sequential_numbering
 *                   is true.
 * @return The transformed name.
 */
std::string ApplyPolicy(const std::string& original, const NamePolicy& policy,
                        int seq_index);

/**
 * Apply the name policy to a list of names, ensuring uniqueness.
 *
 * When require_unique is true and duplicates exist after policy application,
 * a numeric suffix is appended to make each name unique.
 *
 * @param originals       The list of original names.
 * @param policy          The naming policy to apply.
 * @param require_unique  If true, ensure all output names are unique.
 * @return Vector of transformed names in the same order as originals.
 */
std::vector<std::string> ApplyPolicyBatch(
    const std::vector<std::string>& originals, const NamePolicy& policy,
    bool require_unique);

}  // namespace name_policy

#endif  // _NAME_POLICY_H_
