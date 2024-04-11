// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_RUST_FEND_CORE_V1_WRAPPER_FEND_CORE_H_
#define THIRD_PARTY_RUST_FEND_CORE_V1_WRAPPER_FEND_CORE_H_

#include <optional>
#include <string>

namespace fend_core {

std::optional<std::string> evaluate(std::string_view query);

}  // namespace fend_core

#endif  // THIRD_PARTY_RUST_FEND_CORE_V1_WRAPPER_FEND_CORE_H_
