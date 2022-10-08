// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is Chromium specific. It's not rolled from the upstream project.

#include "base/strings/string_number_conversions.h"

namespace crdtp {
namespace json {
namespace platform {
// Parses |str| into |result|. Returns false iff there are
// leftover characters or parsing errors.
bool StrToD(const char* str, double* result) {
  return base::StringToDouble(str, result);
}

// Prints |value| in a format suitable for JSON.
std::string DToStr(double value) {
  return base::NumberToString(value);
}
}  // namespace platform
}  // namespace json
}  // namespace crdtp
