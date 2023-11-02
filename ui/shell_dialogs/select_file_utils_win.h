// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_SHELL_DIALOGS_SELECT_FILE_UTILS_WIN_H_
#define UI_SHELL_DIALOGS_SELECT_FILE_UTILS_WIN_H_

#include "base/strings/string_tokenizer.h"

namespace ui {

// Given a file name, return the sanitized version by removing substrings that
// are embedded in double '%' characters as those are reserved for environment
// variables. Implementation detail exported for unit tests.
template <typename T>
std::basic_string<T> RemoveEnvVarFromFileName(
    const std::basic_string<T>& file_name,
    const std::basic_string<T>& env_delimit) {
  base::StringTokenizerT<std::basic_string<T>,
                         typename std::basic_string<T>::const_iterator>
      t(file_name, env_delimit);
  t.set_options(base::StringTokenizer::RETURN_EMPTY_TOKENS);
  std::basic_string<T> result;
  bool token_valid = t.GetNext();
  while (token_valid) {
    // Append substring before the first "%".
    result.append(t.token());
    // Done if we are reaching the end delimiter,
    if (!t.GetNext()) {
      break;
    }
    std::basic_string<T> string_after_first_percent = t.token();
    token_valid = t.GetNext();
    // If there are no other "%", append the string after
    // the first "%". Otherwise, remove the string between
    // the "%" and continue handing the remaining string.
    if (!token_valid) {
      result.append(env_delimit);
      result.append(string_after_first_percent);
      break;
    }
  }
  return result;
}

}  // namespace ui

#endif  // UI_SHELL_DIALOGS_SELECT_FILE_UTILS_WIN_H_
