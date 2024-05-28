// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOJOM_TEXT_EDIT_COMMAND_MOJOM_TRAITS_H_
#define UI_BASE_IME_MOJOM_TEXT_EDIT_COMMAND_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/enum_traits.h"
#include "ui/base/ime/mojom/text_edit_commands.mojom-shared.h"
#include "ui/base/ime/text_edit_commands.h"

namespace mojo {

template <>
class COMPONENT_EXPORT(IME_SHARED_MOJOM_TRAITS)
    EnumTraits<ui::mojom::TextEditCommand, ui::TextEditCommand> {
 public:
  static ui::mojom::TextEditCommand ToMojom(ui::TextEditCommand);
  static bool FromMojom(ui::mojom::TextEditCommand input,
                        ui::TextEditCommand* output);
};

}  // namespace mojo

#endif  // UI_BASE_IME_MOJOM_TEXT_EDIT_COMMAND_MOJOM_TRAITS_H_
