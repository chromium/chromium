// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/accelerators/command.h"

namespace ui {

Command::Command(std::string_view command_name,
                 std::u16string_view description,
                 bool global)
    : command_name_(command_name), description_(description), global_(global) {}

}  // namespace ui
