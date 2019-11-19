// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "third_party/leveldatabase/leveldb_features.h"

namespace leveldb {

const base::Feature kLevelDBRewriteFeature{"LevelDBPerformRewrite",
                                           base::FEATURE_ENABLED_BY_DEFAULT};
}  // namespace leveldb
