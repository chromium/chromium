// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_
#define WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_

#include "base/memory/weak_ptr.h"

namespace wolvic {

class WvrGraphicsDelegate {
 public:
  WvrGraphicsDelegate();

  WvrGraphicsDelegate(const WvrGraphicsDelegate&) = delete;
  WvrGraphicsDelegate& operator=(const WvrGraphicsDelegate&) = delete;

  ~WvrGraphicsDelegate();

  base::WeakPtr<WvrGraphicsDelegate> GetWeakPtr();

 private:
  base::WeakPtrFactory<WvrGraphicsDelegate> weak_ptr_factory_{this};
};

}  // namespace wolvic

#endif  // WOLVIC_BROWSER_VR_WVR_GRAPHICS_DELEGATE_H_
