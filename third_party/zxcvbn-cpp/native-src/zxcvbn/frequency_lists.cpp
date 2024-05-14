#include <zxcvbn/frequency_lists.hpp>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/files/memory_mapped_file.h"
#include "base/logging.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
  std::string_view value;
};

// A reference to an entry inside a dictionary.
// The entry consists of a MarkedBigEndianU15 representing the word's rank
// (the position at which the word appears in the original wordlist) and an
// inline string (ASCII, terminated with a byte that has the MARKER_BIT set)
// that stores the actual word.
class RankedDictEntryRef {
 public:
  explicit RankedDictEntryRef(const RankedDicts::Datawrapper& wrapper,
                              size_t offset) {
    size_t size = wrapper.size();
    const char* data = wrapper.data();

    CHECK_LT(offset + sizeof(MarkedBigEndianU15), size);
    const char* raw_rank = data + offset;
    rank_ = reinterpret_cast<const MarkedBigEndianU15*>(raw_rank)->get();

    size_t value_start = offset + sizeof(MarkedBigEndianU15);
    size_t value_end = value_start;
    while (true) {
      CHECK_LT(value_end, size);
      if (MarkedBigEndianU15::IsPossibleMarkerByte(data[value_end])) {
        break;
      }
      value_end++;
    }
    value_ = std::string_view(data + value_start, value_end - value_start);
  }
  RankedDictEntryRef(RankedDictEntryRef&) = delete;
  RankedDictEntryRef& operator=(const RankedDictEntryRef&) = delete;

  uint16_t rank() const { return rank_; }
  std::string_view value() const { return value_; }

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
  std::string_view value_;
};

// Helper function that does nothing with the RankedDicts apart from letting
// it destruct as it goes out of scope. This is called on the ThreadPool to
// allow for potentially blocking behavior of `RankedDicts` destructor.
void DoNothing(RankedDicts dicts) {}

}  // namespace

RankedDicts::Datawrapper::Datawrapper(std::vector<char> data)
    : size_(data.size()), data_(data.data()), content_(std::move(data)) {}

RankedDicts::Datawrapper::Datawrapper(
    std::unique_ptr<base::MemoryMappedFile> map)
    : size_((map && map->IsValid()) ? map->length() : 0u),
      data_(map && map->IsValid() ? reinterpret_cast<const char*>(map->data())
                                  : nullptr),
      content_(std::move(map)) {}

RankedDicts::RankedDicts(
    const std::vector<std::vector<std::string_view>>& ordered_dicts) {
  std::vector<MergedEntry> merged_dicts;
  for (const std::vector<std::string_view>& strings : ordered_dicts) {
    size_t rank = 1;
    for (const std::string_view& s : strings) {
      bool clean_string = true;
      for (char c : s) {
        if (MarkedBigEndianU15::IsPossibleMarkerByte(c)) {
          NOTREACHED_IN_MIGRATION()
              << "RankedDicts bad character " << static_cast<unsigned char>(c);
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
  std::vector<char> vec;
  vec.reserve(dict_size + 1);

  // second pass: place elements in allocated array
  for (MergedEntry& entry : merged_dicts)
    RankedDictEntryRef::AppendToVector(entry, vec);
  CHECK_EQ(vec.size(), dict_size);
  vec.push_back(MarkedBigEndianU15::MARKER_BIT);
  data_ = Datawrapper(std::move(vec));
}

RankedDicts::RankedDicts(std::unique_ptr<base::MemoryMappedFile> map)
    : data_(std::move(map)) {}

// Performs a binary search over an array of variable-size elements.
// To find an element in the middle between two others, we first locate the
// *byte* in the middle, then seek forward until we hit a marker byte that
// will only appear at the start of an allocation.
absl::optional<rank_t> RankedDicts::Find(std::string_view needle) const {
  // Special case for empty dictionary.
  size_t size = data_.size();
  if (size == 0) {
    return absl::nullopt;
  }
  CHECK_GE(size, 3u);  // 2 bytes header, 1 byte trailing marker

  // Create a range whose start and end point to marker bytes.
  size_t range_start = 0;
  size_t range_last = size - 2u;
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
    std::string_view mid_value = mid_entry.value();
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
  const char* data = data_.data();
  if (MarkedBigEndianU15::IsPossibleMarkerByte(data[offset])) {
    if (offset == 0)
      return true;
    if (!MarkedBigEndianU15::IsPossibleMarkerByte(data[offset - 1])) {
      return true;
    }
  }
  return false;
}

void SetRankedDictsImplementation(RankedDicts dicts) {
  default_ranked_dicts() = std::move(dicts);
}

void SetRankedDicts(RankedDicts dicts) {
  // Destroying a `RankedDict` may block if it is based on a `MemoryMappedFile`.
  // Therefore this helper moves the task of doing it to a thread pool.
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DoNothing, std::move(default_ranked_dicts())));
  default_ranked_dicts() = std::move(dicts);
}

RankedDicts& default_ranked_dicts() {
  static base::NoDestructor<RankedDicts> default_dicts;
  return *default_dicts;
}

}  // namespace zxcvbn
