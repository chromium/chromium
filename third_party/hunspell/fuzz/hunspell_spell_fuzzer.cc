// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>
#include <string>

#include "base/strings/utf_string_conversions.h"
#include "third_party/hunspell/fuzz/hunspell_fuzzer_hunspell_dictionary.h"
#include "third_party/hunspell/src/hunspell/hunspell.hxx"

// Entry point for LibFuzzer.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (!size)
    return 0;

  static Hunspell* hunspell = new Hunspell(kHunspellDictionary);

  std::string data_string(reinterpret_cast<const char*>(data), size);

  // hunspell is not handling invalid UTF8. To avoid that, do the same thing
  // Chromium does - convert to UTF16, and back to UTF8. Valid UTF8 guaranteed.
  std::u16string utf16_string = base::UTF8ToUTF16(data_string);
  data_string = base::UTF16ToUTF8(utf16_string);

  hunspell->spell(data_string);

  return 0;
}
