#ifndef __ZXCVBN__FREQUENCY_LISTS_HPP
#define __ZXCVBN__FREQUENCY_LISTS_HPP

#include <cstdint>
#include <memory>
#include <vector>

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "base/strings/string_piece.h"

namespace zxcvbn {

using rank_t = std::size_t;

// Stores words from a set of dictionaries (originally ordered by word
// frequency) in a sorted flat array.
// Lookups run in roughly logarithmic time and, when a match is found, return
// the position of the word in the original dictionary.
// This data structure is optimized for memory efficiency over lookup speed.
// It does not contain any pointers and its format is target-independent, so it
// could theoretically directly be mapped from disk.
//
// Since this data structure sorts words alphabetically, the lookup code could
// be extended to also answer the question "are there any entries that start
// with the given prefix", which should permit speeding up dictionary_match().
// That isn't implemented yet though.
class RankedDicts {
 public:
  explicit RankedDicts(const std::vector<std::vector<base::StringPiece>> &ordered_dicts);
  RankedDicts() = default;
  RankedDicts(RankedDicts &&) = default;
  RankedDicts(RankedDicts &) = delete;

  RankedDicts& operator=(RankedDicts &&) = default;
  RankedDicts& operator=(const RankedDicts &) = delete;

  absl::optional<rank_t> Find(base::StringPiece needle) const;

 private:
  bool IsRealMarker(size_t offset) const;
  // Buffer storing the dictionaries, see RankedDictEntryRef and the rest of
  // frequency_lists.cpp for documentation of the data structure.
  std::vector<char> data_;
};

void SetRankedDicts(RankedDicts dicts);

RankedDicts &default_ranked_dicts();

} // namespace zxcvbn

#endif
