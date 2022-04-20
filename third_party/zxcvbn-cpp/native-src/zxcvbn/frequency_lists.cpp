#include <zxcvbn/frequency_lists.hpp>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"

namespace zxcvbn {

namespace {

// A big-endian 16-bit value, consisting of a 15-bit number and a marker bit in
// the most significant position (in the first byte).
// No alignment requirements.
// This is used to store a "rank", which is the position at which a word
// occurred in a wordlist.
class MarkedBigEndianU15 {
 public:
  static constexpr size_t MAX_VALUE = (1 << 15) - 1;
  static constexpr uint8_t MARKER_BIT = 0x80;
  uint16_t get() const {
    return (encoded_value[0] & ~MARKER_BIT) * 256 + encoded_value[1];
  }
  static void AppendToVector(uint16_t value, std::vector<char>& vec) {
    CHECK(value <= MAX_VALUE);
    vec.push_back((value >> 8) | MARKER_BIT);
    vec.push_back(value & 0xff);
  }
  // Check whether the given byte has the high bit set.
  // This always returns true for the first byte of a MarkedBigEndianU15, but
  // may also be false-positive for the second byte.
  // To reliably determine whether a given byte really is the start of a
  // MarkedBigEndianU15, you need to also check the preceding byte if this
  // returns true.
  static bool IsPossibleMarkerByte(uint8_t c) { return (c & MARKER_BIT) != 0; }

 private:
  uint8_t encoded_value[2];
};
static_assert(
    sizeof(MarkedBigEndianU15) == 2,
    "object layout must fit with assumptions in the rest of this file");

struct MergedEntry {
  size_t rank;
  base::StringPiece value;
};

// A reference to an entry inside a dictionary.
// The entry consists of a MarkedBigEndianU15 representing the word's rank
// (the position at which the word appears in the original wordlist) and an
// inline string (ASCII, terminated with a byte that has the MARKER_BIT set)
// that stores the actual word.
class RankedDictEntryRef {
 public:
  explicit RankedDictEntryRef(const std::vector<char>& vec, size_t offset) {
    CHECK_LT(offset + sizeof(MarkedBigEndianU15), vec.size());
    const char* raw_rank = vec.data() + offset;
    rank_ = reinterpret_cast<const MarkedBigEndianU15*>(raw_rank)->get();

    size_t value_start = offset + sizeof(MarkedBigEndianU15);
    size_t value_end = value_start;
    while (true) {
      CHECK_LT(value_end, vec.size());
      if (MarkedBigEndianU15::IsPossibleMarkerByte(vec[value_end]))
        break;
      value_end++;
    }
    value_ =
        base::StringPiece(vec.data() + value_start, value_end - value_start);
  }
  RankedDictEntryRef(RankedDictEntryRef&) = delete;
  RankedDictEntryRef& operator=(const RankedDictEntryRef&) = delete;

  uint16_t rank() const { return rank_; }
  base::StringPiece value() const { return value_; }

  static void AppendToVector(MergedEntry entry, std::vector<char>& vec) {
    if (entry.rank > MarkedBigEndianU15::MAX_VALUE) {
      LOG(ERROR) << "MarkedBigEndianU15 clamping";
      entry.rank = MarkedBigEndianU15::MAX_VALUE;
    }
    MarkedBigEndianU15::AppendToVector(entry.rank, vec);
    vec.insert(vec.end(), entry.value.begin(), entry.value.end());
  }

 private:
  size_t rank_;
  base::StringPiece value_;
};
}  // namespace

RankedDicts::RankedDicts(
    const std::vector<std::vector<base::StringPiece>>& ordered_dicts) {
  std::vector<MergedEntry> merged_dicts;
  for (const std::vector<base::StringPiece>& strings : ordered_dicts) {
    size_t rank = 1;
    for (const base::StringPiece& s : strings) {
      bool clean_string = true;
      for (char c : s) {
        if (MarkedBigEndianU15::IsPossibleMarkerByte(c)) {
          NOTREACHED() << "RankedDicts bad character "
                       << static_cast<unsigned char>(c);
          clean_string = false;
        }
      }
      if (clean_string) {
        merged_dicts.push_back({rank++, s});
      }
    }
  }
  std::sort(merged_dicts.begin(), merged_dicts.end(),
            [](MergedEntry& a, MergedEntry& b) { return a.value < b.value; });

  if (merged_dicts.size() == 0)
    return;

  // first pass: calculate required total size
  size_t dict_size = sizeof(MarkedBigEndianU15) * merged_dicts.size();
  for (MergedEntry& entry : merged_dicts)
    dict_size += entry.value.size();

  // 1 byte at the end for trailing marker byte (for finding last string size)
  data_.reserve(dict_size + 1);

  // second pass: place elements in allocated array
  for (MergedEntry& entry : merged_dicts)
    RankedDictEntryRef::AppendToVector(entry, data_);
  CHECK_EQ(data_.size(), dict_size);
  data_.push_back(MarkedBigEndianU15::MARKER_BIT);
}

// Performs a binary search over an array of variable-size elements.
// To find an element in the middle between two others, we first locate the
// *byte* in the middle, then seek forward until we hit a marker byte that
// will only appear at the start of an allocation.
absl::optional<rank_t> RankedDicts::Find(base::StringPiece needle) const {
  // special case for empty dictionary
  if (data_.size() == 0)
    return absl::nullopt;
  CHECK_GE(data_.size(), 3UL);  // 2 bytes header, 1 byte trailing marker

  // Create a range whose start and end point to marker bytes.
  size_t range_start = 0;
  size_t range_last = data_.size() - 2;
  CHECK(IsRealMarker(0));
  while (!IsRealMarker(range_last))
    range_last--;

  while (true) {
    size_t midpoint = range_start + (range_last - range_start) / 2;
    // Find a marker byte from the midpoint onwards. (There must be one, since
    // there is one at range_last.)
    size_t adjusted_midpoint = midpoint;
    while (!IsRealMarker(adjusted_midpoint))
      adjusted_midpoint++;

    // Perform the actual comparison.
    RankedDictEntryRef mid_entry(data_, adjusted_midpoint);
    base::StringPiece mid_value = mid_entry.value();
    int cmp_result = mid_value.compare(needle);
    if (cmp_result == 0)
      return mid_entry.rank();
    if (cmp_result < 0) {
      if (adjusted_midpoint == range_last)
        return absl::nullopt;
      range_start = adjusted_midpoint + 1;
      while (!IsRealMarker(range_start))
        range_start++;
    } else {
      if (adjusted_midpoint == range_start)
        return absl::nullopt;
      range_last = adjusted_midpoint - 1;
      while (!IsRealMarker(range_last))
        range_last--;
    }
  }
}

// Determine whether an entry starts at the given offset; in other words,
// determine whether a MarkedBigEndianU15 starts there.
bool RankedDicts::IsRealMarker(size_t offset) const {
  CHECK_LT(offset, data_.size());
  if (MarkedBigEndianU15::IsPossibleMarkerByte(data_[offset])) {
    if (offset == 0)
      return true;
    if (!MarkedBigEndianU15::IsPossibleMarkerByte(data_[offset - 1]))
      return true;
  }
  return false;
}

void SetRankedDicts(RankedDicts dicts) {
  default_ranked_dicts() = std::move(dicts);
}

RankedDicts& default_ranked_dicts() {
  static base::NoDestructor<RankedDicts> default_dicts;
  return *default_dicts;
}

}  // namespace zxcvbn
