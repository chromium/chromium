// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>

#include "base/strings/strcat.h"
#include "base/strings/string_piece_rust.h"
#include "third_party/rust/fend_core/v1/wrapper/fend_core_ffi_glue.rs.h"

namespace fend_core {

std::optional<std::string> evaluate(std::string_view query,
                                    unsigned int timeout_in_ms) {
  static constexpr char kNoApproxPrefix[] = "@noapprox ";
  static constexpr char kDecimalPlacesSuffix[] = " in 2dp";

  rust::String rust_result;
  if (evaluate_using_rust(base::StringPieceToRustSlice(query), rust_result,
                          timeout_in_ms)) {
    std::string result(rust_result);
    if (result.ends_with(query) || result.starts_with("\\")) {
      return std::nullopt;
    }
    rust::String final_result;
    std::string full_query =
        base::StrCat({kNoApproxPrefix, query, kDecimalPlacesSuffix});
    if (evaluate_using_rust(base::StringPieceToRustSlice(full_query),
                            final_result, timeout_in_ms)) {
      return std::string(final_result);
    }
  }
  return std::nullopt;
}

} // namespace fend_core
