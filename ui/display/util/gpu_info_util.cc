// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/util/gpu_info_util.h"

#include "base/strings/string_piece.h"

namespace display {

base::Value BuildGpuInfoEntry(base::StringPiece description,
                              base::StringPiece value) {
  base::Value::Dict dict;
  dict.Set("description", description);
  dict.Set("value", value);
  return base::Value(std::move(dict));
}

base::Value::Dict BuildGpuInfoEntry(base::StringPiece description,
                                    base::Value value) {
  base::Value::Dict dict;
  dict.Set("description", description);
  dict.Set("value", std::move(value));
  return dict;
}

}  // namespace display
