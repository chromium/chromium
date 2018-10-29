// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_MUS_MUS_TYPES_H_
#define UI_AURA_MUS_MUS_TYPES_H_

#include <stdint.h>

#include "services/ws/common/types.h"

// Typedefs for the transport types. These typedefs match that of the mojom
// file, see it for specifics.

namespace aura {

constexpr ws::Id kInvalidServerId = 0;

enum class WindowMusType {
  // The window is an embed root in the embedded client. That is, the client
  // received this window by way of another client calling Embed(). In other
  // words, this is the embedded side of an embedding.
  // NOTE: in the client that called Embed() the window type is LOCAL.
  EMBED,

  // The window was created by requesting a top level
  // (WindowTree::NewTopLevel()).
  TOP_LEVEL,

  // The window was created locally.
  LOCAL,

  // Not one of the above. This means the window is visible to the client and
  // not one of the above values. For example, if
  // |kEmbedFlagEmbedderInterceptsEvents| is used, then the embedder sees
  // Windows created by the embedded client.
  OTHER,
};

}  // namespace ui

#endif  // UI_AURA_MUS_MUS_TYPES_H_
