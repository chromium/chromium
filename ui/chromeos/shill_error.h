// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_SHILL_ERROR_H_
#define UI_CHROMEOS_SHILL_ERROR_H_

#include <string>

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace shill_error {

UI_CHROMEOS_EXPORT std::u16string GetShillErrorString(
    const std::string& error,
    const std::string& network_id);

}  // namespace shill_error
}  // namespace ui

#endif  // UI_CHROMEOS_SHILL_ERROR_H_
