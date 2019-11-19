// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_ACCELERATORS_MOJOM_ACCELERATOR_MOJOM_TRAITS_H_
#define UI_BASE_ACCELERATORS_MOJOM_ACCELERATOR_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/time_mojom_traits.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/accelerators/mojom/accelerator.mojom-shared.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace mojo {

template <>
struct EnumTraits<ui::mojom::AcceleratorKeyState, ui::Accelerator::KeyState> {
  static ui::mojom::AcceleratorKeyState ToMojom(
      ui::Accelerator::KeyState input) {
    switch (input) {
      case ui::Accelerator::KeyState::PRESSED:
        return ui::mojom::AcceleratorKeyState::PRESSED;
      case ui::Accelerator::KeyState::RELEASED:
        return ui::mojom::AcceleratorKeyState::RELEASED;
    }
    NOTREACHED();
    return ui::mojom::AcceleratorKeyState::PRESSED;
  }

  static bool FromMojom(ui::mojom::AcceleratorKeyState input,
                        ui::Accelerator::KeyState* out) {
    switch (input) {
      case ui::mojom::AcceleratorKeyState::PRESSED:
        *out = ui::Accelerator::KeyState::PRESSED;
        return true;
      case ui::mojom::AcceleratorKeyState::RELEASED:
        *out = ui::Accelerator::KeyState::RELEASED;
        return true;
    }
    NOTREACHED();
    return false;
  }
};

template <>
struct StructTraits<ui::mojom::AcceleratorDataView, ui::Accelerator> {
  static int32_t key_code(const ui::Accelerator& p) {
    return static_cast<int32_t>(p.key_code());
  }
  static ui::Accelerator::KeyState key_state(const ui::Accelerator& p) {
    return p.key_state();
  }
  static int32_t modifiers(const ui::Accelerator& p) { return p.modifiers(); }
  static base::TimeTicks time_stamp(const ui::Accelerator& p) {
    return p.time_stamp();
  }
  static bool Read(ui::mojom::AcceleratorDataView data, ui::Accelerator* out) {
    ui::Accelerator::KeyState key_state;
    if (!data.ReadKeyState(&key_state))
      return false;
    base::TimeTicks time_stamp;
    if (!data.ReadTimeStamp(&time_stamp))
      return false;
    *out = ui::Accelerator(static_cast<ui::KeyboardCode>(data.key_code()),
                           data.modifiers(), key_state, time_stamp);
    return true;
  }
};

}  // namespace mojo

#endif  // UI_BASE_ACCELERATORS_MOJOM_ACCELERATOR_MOJOM_TRAITS_H_
