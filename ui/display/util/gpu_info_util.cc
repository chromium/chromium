// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/gpu_info_util.h"

#include "base/strings/string_piece.h"
#include "base/values.h"

namespace display {

base::Value BuildGpuInfoEntry(base::StringPiece description,
                              base::StringPiece value) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("description", description);
  dict.SetStringKey("value", value);
  return dict;
}

base::Value BuildGpuInfoEntry(base::StringPiece description,
                              base::Value value) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("description", description);
  dict.SetKey("value", std::move(value));
  return dict;
}

}  // namespace display
