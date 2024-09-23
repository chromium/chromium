// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>

#include "base/strings/string_view_rust.h"
#include "third_party/rust/fend_core/v1/wrapper/fend_core_ffi_glue.rs.h"

namespace fend_core {

std::optional<std::string> evaluate(std::string_view query,
                                    unsigned int timeout_in_ms) {
  rust::String rust_result;
  if (evaluate_using_rust(base::StringViewToRustSlice(query), rust_result,
                          timeout_in_ms)) {
    return std::string(rust_result);
  }
  return std::nullopt;
}

} // namespace fend_core
