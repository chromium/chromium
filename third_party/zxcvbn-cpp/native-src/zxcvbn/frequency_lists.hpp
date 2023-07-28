#ifndef __ZXCVBN__FREQUENCY_LISTS_HPP
#define __ZXCVBN__FREQUENCY_LISTS_HPP

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "base/files/memory_mapped_file.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

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
  // Abstraction layer for the binary blob of data that contains the contents
  // of the `RankedDicts`. The data can either be held directly in memory or
  // be obtained from a memory mapped file.
  // See `RankedDictEntryRef` and the rest of frequency_lists.cpp for
  // documentation of the data structure.
  class Datawrapper {
   public:
    explicit Datawrapper(std::vector<char> data);
    explicit Datawrapper(std::unique_ptr<base::MemoryMappedFile> map);
    Datawrapper() = default;
    Datawrapper(Datawrapper&&) = default;

    Datawrapper& operator=(Datawrapper&&) = default;

    size_t size() const { return size_; }
    // Returns a pointer to the data chunk belonging to the buffer. Returns a
    // non-null value only if `size()` is non-zero.
    const char* data() const { return data_; }

   private:
    size_t size_ = 0u;
    const char* data_ = nullptr;
    absl::variant<std::vector<char>, std::unique_ptr<base::MemoryMappedFile>>
        content_;
  };

  explicit RankedDicts(
      const std::vector<std::vector<std::string_view>>& ordered_dicts);
  explicit RankedDicts(std::unique_ptr<base::MemoryMappedFile>);
  RankedDicts() = default;
  RankedDicts(RankedDicts&&) = default;
  RankedDicts(const RankedDicts&) = delete;

  RankedDicts& operator=(RankedDicts&&) = default;
  RankedDicts& operator=(const RankedDicts&) = delete;

  absl::optional<rank_t> Find(std::string_view needle) const;

  std::string_view DataForTesting() const {
    return std::string_view(data_.data(), data_.size());
  }

 private:
  bool IsRealMarker(size_t offset) const;

  Datawrapper data_;
};

void SetRankedDicts(RankedDicts dicts);

RankedDicts& default_ranked_dicts();

} // namespace zxcvbn

#endif
