// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_ISOLATED_WORLD_IDS_H_
#define WEBLAYER_COMMON_ISOLATED_WORLD_IDS_H_

#include "content/public/common/isolated_world_ids.h"

namespace weblayer {

enum IsolatedWorldIDs {
  // Isolated world ID for internal WebLayer features.
  ISOLATED_WORLD_ID_WEBLAYER = content::ISOLATED_WORLD_ID_CONTENT_END + 1,

  // Isolated world ID for WebLayer translate.
  ISOLATED_WORLD_ID_TRANSLATE,
};

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_ISOLATED_WORLD_IDS_H_
