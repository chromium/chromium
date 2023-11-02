// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/constants.h"

namespace ui {

// Here, we define attributes of ui::Event::Properties objects
// kPropertyFromVK
const char kPropertyFromVK[] = "from_vk";

// Properties of the kPropertyFromVK attribute

// kFromVKIsMirroring is the index of the isMirrorring property on the
// kPropertyFromVK attribute. This is non-zero if mirroring and zero if not
// mirroring
const size_t kPropertyFromVKIsMirroringIndex = 0;
// kFromVKSize is the size of the kPropertyFromVK attribute
// It is equal to the number of kPropertyFromVK
const size_t kPropertyFromVKSize = 1;

}  // namespace ui
