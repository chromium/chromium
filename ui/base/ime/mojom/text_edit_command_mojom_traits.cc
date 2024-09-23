// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/mojom/text_edit_command_mojom_traits.h"

namespace mojo {

// These functions use text_edit_commands.inc to generate the code required to
// convert to and from ui::TextEditCommand and
// ui::mojom::TextEditCommand.

// static
#define TEXT_EDIT_COMMAND(UI, MOJOM) \
  case ui::TextEditCommand::UI:      \
    return ui::mojom::TextEditCommand::MOJOM;

ui::mojom::TextEditCommand
EnumTraits<ui::mojom::TextEditCommand, ui::TextEditCommand>::ToMojom(
    ui::TextEditCommand input) {
  switch (input) {
#include "ui/base/ime/text_edit_commands.inc"
  }
#undef TEXT_EDIT_COMMAND

  // Failure to convert should never occur.
  NOTREACHED();
}

// static
#define TEXT_EDIT_COMMAND(UI, MOJOM)      \
  case ui::mojom::TextEditCommand::MOJOM: \
    *output = ui::TextEditCommand::UI;    \
    return true;

bool EnumTraits<ui::mojom::TextEditCommand, ui::TextEditCommand>::FromMojom(
    ui::mojom::TextEditCommand input,
    ui::TextEditCommand* output) {
  switch (input) {
#include "ui/base/ime/text_edit_commands.inc"
  }
#undef TEXT_EDIT_COMMAND

  // Return `false` to indicate the conversion was not successful.
  return false;
}

}  // namespace mojo
