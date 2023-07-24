// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wolvic/browser/vr/wvr_graphics_delegate.h"

namespace wolvic {

WvrGraphicsDelegate::WvrGraphicsDelegate() {}

WvrGraphicsDelegate::~WvrGraphicsDelegate() {}

base::WeakPtr<WvrGraphicsDelegate> WvrGraphicsDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace wolvic
