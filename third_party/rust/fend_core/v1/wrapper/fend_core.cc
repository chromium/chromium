// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/rust/fend_core/v1/wrapper/fend_core.h"

#include <optional>
#include <string>

#include "third_party/rust/fend_core/v1/wrapper/fend_core_ffi_glue.rs.h"
#include "base/strings/string_piece_rust.h"

namespace fend_core {

std::optional<std::string> evaluate(std::string_view query) {
  rust::String rust_result;
  if (evaluate_using_rust(base::StringPieceToRustSlice(query), rust_result)) {
    return std::string(rust_result);
  } else {
    return std::nullopt;
  }
}

}  // namespace fend_core
