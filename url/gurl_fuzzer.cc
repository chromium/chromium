// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/check_op.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "url/gurl.h"

struct TestCase {
  TestCase() { CHECK(base::i18n::InitializeICU()); }

  // used by ICU integration.
  base::AtExitManager at_exit_manager;
};

TestCase* test_case = new TestCase();

// Checks that GURL's canonicalization is idempotent. This can help discover
// issues like https://crbug.com/1128999.
void CheckIdempotency(const GURL& url) {
  if (!url.is_valid())
    return;
  const std::string& spec = url.spec();
  GURL recanonicalized(spec);
  CHECK(recanonicalized.is_valid());
  CHECK_EQ(spec, recanonicalized.spec());
}

// Checks that |url.spec()| is preserved across a call to ReplaceComponents with
// zero replacements, which is effectively a copy. This can help discover issues
// like https://crbug.com/1075515.
void CheckReplaceComponentsPreservesSpec(const GURL& url) {
  static const base::NoDestructor<GURL::Replacements> no_op;
  GURL copy = url.ReplaceComponents(*no_op);
  CHECK_EQ(url.is_valid(), copy.is_valid());
  if (url.is_valid()) {
    CHECK_EQ(url.spec(), copy.spec());
  }
}

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size < 1)
    return 0;
  // SAFETY: libufzzer is responsible to pass valid `data` and `size`.
  auto bytes = UNSAFE_BUFFERS(base::span<const uint8_t>(data, size));
  {
    std::string_view string_piece_input(base::as_chars(bytes));
    const GURL url_from_string_piece(string_piece_input);
    CheckIdempotency(url_from_string_piece);
    CheckReplaceComponentsPreservesSpec(url_from_string_piece);
  }
  // Test for std::u16string_view if size is even.
  if (size % sizeof(char16_t) == 0) {
    std::u16string_view string_piece_input16(
        reinterpret_cast<const char16_t*>(data), size / sizeof(char16_t));
    const GURL url_from_string_piece16(string_piece_input16);
    CheckIdempotency(url_from_string_piece16);
    CheckReplaceComponentsPreservesSpec(url_from_string_piece16);
  }
  // Resolve relative url tests.
  {
    constexpr size_t kSizeTBytes = sizeof(size_t);
    if (size < kSizeTBytes + 1) {
      return 0;
    }
    // `bytes` is split into three spans; `size_bytes`, `relative_chars`, and
    // `part_chars`.
    auto [size_bytes, payload_bytes] = bytes.split_at(kSizeTBytes);
    size_t relative_size;
    base::byte_span_from_ref(relative_size).copy_from(size_bytes);
    relative_size = relative_size % payload_bytes.size();
    auto [relative_chars, part_chars] =
        base::as_chars(payload_bytes).split_at(relative_size);

    std::string_view relative_string(relative_chars);
    std::string_view string_piece_part_input(part_chars);
    const GURL url_from_string_piece_part(string_piece_part_input);
    CheckIdempotency(url_from_string_piece_part);
    CheckReplaceComponentsPreservesSpec(url_from_string_piece_part);

    std::ignore = url_from_string_piece_part.Resolve(relative_string);

    if (relative_size % sizeof(char16_t) == 0) {
      std::u16string relative_string16(
          reinterpret_cast<const char16_t*>(relative_chars.data()),
          relative_size / sizeof(char16_t));
      std::ignore = url_from_string_piece_part.Resolve(relative_string16);
    }
  }
  return 0;
}
