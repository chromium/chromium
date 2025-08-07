// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CONSTANTS_H_
#define UI_BASE_IME_CONSTANTS_H_

#include "base/component_export.h"
#include "stddef.h"

namespace ui {

// The name of the property that is attach to the key event and indicates
// whether it was from the virtual keyboard.
//
// This is used where the key event is simulated by the virtual keyboard
// (e.g. IME extension API) as well as the input field implementation (e.g.
// Textfield).
inline constexpr char kPropertyFromVK[] = "from_vk";

// Properties of the kPropertyFromVK attribute

// kFromVKIsMirroring is the index of the isMirroring property on the
// kPropertyFromVK attribute. This is non-zero if mirroring and zero if not
// mirroring.
inline constexpr size_t kPropertyFromVKIsMirroringIndex = 0;
// kFromVKSize is the size of the kPropertyFromVK attribute
// It is equal to the number of kPropertyFromVK
inline constexpr size_t kPropertyFromVKSize = 1;

}  // namespace ui

#endif  // UI_BASE_IME_CONSTANTS_H_
