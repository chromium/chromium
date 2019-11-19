// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include <fuzzer/FuzzedDataProvider.h>

#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/hunspell/fuzz/hunspell_fuzzer_hunspell_dictionary.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  // Generate and use a range of dictionary sizes. Large dictionaries might have
  // more coverage and uncover more interesting code paths, however, it is
  // extremely slow. Smaller dictionaries are _much_ faster, but may result in
  // less coverage.
  FuzzedDataProvider data_provider(data, size);
  size_t shift = data_provider.ConsumeIntegralInRange(0, 16);
  static Hunspell* hunspell =
      new Hunspell(kHunspellDictionary, sizeof(kHunspellDictionary) >> shift);

  std::string data_string(reinterpret_cast<const char*>(data), size);

  // hunspell is not handling invalid UTF8. To avoid that, do the same thing
  // Chromium does - convert to UTF16, and back to UTF8. Valid UTF8 guaranteed.
  base::string16 utf16_string = base::UTF8ToUTF16(data_string);
  data_string = base::UTF16ToUTF8(utf16_string);

  std::vector<std::string> suggestions = hunspell->suggest(data_string);

  return 0;
}
