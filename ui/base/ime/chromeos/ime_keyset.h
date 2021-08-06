// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_CHROMEOS_IME_KEYSET_H_
#define UI_BASE_IME_CHROMEOS_IME_KEYSET_H_

namespace chromeos {
namespace input_method {

// Used by the virtual keyboard to represent different key layouts for
// different purposes. 'kNone' represents the default key layout.
// Used in UMA, so this enum should not be reordered.
enum class ImeKeyset {
  kNone = 0,
  kEmoji = 1,
  kHandwriting = 2,
  kVoice = 3,
};

}  // namespace input_method
}  // namespace chromeos

// TODO(https://crbug.com/1164001): remove when moved to ash.
namespace ash {
namespace input_method {
using ::chromeos::input_method::ImeKeyset;
}  // namespace input_method
}  // namespace ash

#endif  // UI_BASE_IME_CHROMEOS_IME_KEYSET_H_
