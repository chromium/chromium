// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "base/containers/span.h"
#include "usr/include/linux/netlink.h"

// NOTE: this exists as a regression test for crbug/357433195, where before the
// associated patch that added this, this would fail an assertion in
// extract_edits.py. However after fixing the assertion the code below doesn't
// get rewritten so original.cc == expected.cc, we're just asserting that the
// script doesn't fail on it.
namespace internal {
namespace {
using std::size_t;
// No expected rewrite
bool GetAddress(const struct nlmsghdr* header,
                int header_length,
                bool* really_deprecated) {
  if (really_deprecated) {
    *really_deprecated = false;
  }

  // No expected rewrite
  const struct nlmsghdr* msg =
      reinterpret_cast<const struct nlmsghdr*>(NLMSG_DATA(header));
  return true;
}

// No expected rewrite
template <typename T>
T* SafelyCastNetlinkMsgData(const struct nlmsghdr* header, int length) {
  if (length <= 0 || static_cast<size_t>(length) < NLMSG_HDRLEN + sizeof(T)) {
    return nullptr;
  }
  // no expected rewrite
  return reinterpret_cast<const T*>(NLMSG_DATA(header));
}
}  // namespace
}  // namespace internal

// While fixing the above, the updating sometimes caused the rewrite to occur in
// the middle of the MACRO rather than inside the macro. This catches that case.

// Expected rewrite:
// dict.data()
#define CAST_FUN reinterpret_cast<std::size_t**>(dict.data());
#define CAST_FUN_2(x) reinterpret_cast<std::size_t**>(x);

// Expected rewrite:
// base::span<int*> dict
std::size_t check(base::span<int*> dict, std::size_t N) {
  if (dict) {
    for (int i = 0; i < N; ++i) {
      int* ptr = dict[i];
      // No expected rewrite
      std::size_t** new_ptr = CAST_FUN;
      // Expected rewrite:
      // dict.data()
      std::size_t** new_ptr_2 = CAST_FUN_2(dict.data());
      if (**new_ptr > 10) {
        return **new_ptr;
      } else {
        return **new_ptr_2;
      }
    }
  }
  return 10;
}

void checkMacroInFile() {
  int array[2][1] = {{int(2)}, {int(3)}};
  // Expected rewrite:
  // base::span<int*>
  base::span<int*> front = reinterpret_cast<int**>(array);
  check(front, 2);
}

// Finally after fixing the initial multiple neighbors there was a new one and
// this one catches that. Again this used to crash and showcases the situations
// where the assertion would fail.
struct Entry {
  explicit Entry(std::string n) : name(n) {}
  std::string name;
  std::unordered_map<std::string, int> metrics;
};

class Recorder {
 public:
  // No expected rewrite.
  bool EntryHasMetricNoRewrites(const Entry* entry, std::string metric_name) {
    const int* val = nullptr;
    if (entry->metrics.find(metric_name) != entry->metrics.end()) {
      val = &entry->metrics.find(metric_name)->second;
    }
    return val != nullptr;
  }

  // No expected rewrite
  bool EntryHasMetricRhsRewrite(const Entry* const* entries,
                                size_t len,
                                std::string metric_name) {
    // Can't use entries like a buffer to avoid the rewrite.
    const int* val = nullptr;
    if (!entries || metric_name == "hello" || len > 3) {
      return false;
    }
    return true;
  }

  // Expected rewrite:
  // base::span<const Entry* const>
  bool EntryHasMetricFullRewrite(base::span<const Entry* const> entries,
                                 size_t len,
                                 std::string metric_name) {
    const int* val = nullptr;
    for (size_t i = 0; i < len; ++i) {
      const Entry* entry = entries[i];
      if (entry->metrics.find(metric_name) != entry->metrics.end()) {
        val = &entry->metrics.find(metric_name)->second;
      }
    }
    return val != nullptr;
  }

  // No expected rewrite.
  std::vector<const Entry*> GetEntriesByName(std::string name) const {
    std::vector<const Entry*> result;
    for (const auto& entry : entries_) {
      if (entry->name == name) {
        result.push_back(entry.get());
      }
    }
    return result;
  }
  std::vector<std::unique_ptr<Entry>> entries_;
};

// No expected rewrite.
#define EXPECT_HAS_UKM_NO_REWRITE(name) \
  assert(test_recorder_->EntryHasMetricNoRewrites(entry, name));

// Expected rewrite:
// test_entries.data()
#define EXPECT_HAS_UKM_RHS_REWRITE(name)                               \
  assert(test_recorder_->EntryHasMetricRhsRewrite(test_entries.data(), \
                                                  entries.size(), name));

// No expected rewrite.
#define EXPECT_HAS_UKM_FULL_REWRITE(name)                        \
  assert(test_recorder_->EntryHasMetricFullRewrite(test_entries, \
                                                   entries.size(), name));

void MediaMetricsProviderTestTestUkm() {
  Recorder recorder_;
  Recorder* test_recorder_ = &recorder_;
  test_recorder_->entries_.push_back(std::make_unique<Entry>("foo"));
  // Test when neither lhs nor rhs gets rewritten this is the common case in the
  // code base.
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    assert(1u == entries.size());
    // No expected rewrite.
    for (const Entry* entry : entries) {
      // This macro references |entry| and thus would need to rewrite it as
      // .data() if we changed the for loop to a base::span (which we don't).
      EXPECT_HAS_UKM_NO_REWRITE("bar");
    }
  }
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    assert(1u == entries.size());
    // No expected rewrite.
    for (const Entry* entry : entries) {
      // This macro references |entry| and thus would need to rewrite it as
      // .data() if we changed the for loop to a base::span (which we don't).
      EXPECT_HAS_UKM_NO_REWRITE("bar");
    }
  }

  // Test how we handle macros when the function (lhs) doesn't get rewritten but
  // the rhs does.
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    // Expected rewrite:
    // base::span<const Entry*> test_entries = entries;
    base::span<const Entry* const> test_entries = entries;
    const_cast<const Entry**>(test_entries)[0] = nullptr;
    EXPECT_HAS_UKM_RHS_REWRITE("bar");
  }
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    // Expected rewrite:
    // base::span<const Entry*> test_entries = entries;
    base::span<const Entry* const> test_entries = entries;
    const_cast<const Entry**>(test_entries)[0] = nullptr;
    EXPECT_HAS_UKM_RHS_REWRITE("bar");
  }

  // Test how we handle macros when the function and the rhs gets rewritten.
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    // Expected rewrite:
    // base::span<const Entry* const> test_entries = entries;
    base::span<const Entry* const> test_entries = entries;
    EXPECT_HAS_UKM_FULL_REWRITE("bar");
  }
  {
    const auto& entries = test_recorder_->GetEntriesByName("foo");
    // Expected rewrite:
    // base::span<const Entry* const> test_entries = entries;
    base::span<const Entry* const> test_entries = entries;
    EXPECT_HAS_UKM_FULL_REWRITE("bar");
  }
}
