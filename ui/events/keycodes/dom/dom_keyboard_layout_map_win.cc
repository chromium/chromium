// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <windows.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/dom_keyboard_layout_map_base.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

namespace ui {

namespace {

class DomKeyboardLayoutMapWin : public DomKeyboardLayoutMapBase {
 public:
  DomKeyboardLayoutMapWin();
  ~DomKeyboardLayoutMapWin() override;

 private:
  // ui::DomKeyboardLayoutMapBase implementation.
  uint32_t GetKeyboardLayoutCount() override;
  ui::DomKey GetDomKeyFromDomCodeForLayout(
      ui::DomCode dom_code,
      uint32_t keyboard_layout_index) override;

  // Set of keyboard layout handles provided by the operating system.
  // The handles stored do not need to be released when the vector is destroyed.
  std::vector<HKL> keyboard_layout_handles_;

  DISALLOW_COPY_AND_ASSIGN(DomKeyboardLayoutMapWin);
};

DomKeyboardLayoutMapWin::DomKeyboardLayoutMapWin() = default;

DomKeyboardLayoutMapWin::~DomKeyboardLayoutMapWin() = default;

uint32_t DomKeyboardLayoutMapWin::GetKeyboardLayoutCount() {
  keyboard_layout_handles_.clear();
  const size_t keyboard_layout_count = ::GetKeyboardLayoutList(0, nullptr);
  if (!keyboard_layout_count) {
    DPLOG(ERROR) << "GetKeyboardLayoutList failed: ";
    return false;
  }

  keyboard_layout_handles_.resize(keyboard_layout_count);
  const size_t copy_count = ::GetKeyboardLayoutList(
      keyboard_layout_handles_.size(), keyboard_layout_handles_.data());
  if (!copy_count) {
    DPLOG(ERROR) << "GetKeyboardLayoutList failed: ";
    return false;
  }
  DCHECK_EQ(keyboard_layout_count, copy_count);

  // The set of layouts returned from GetKeyboardLayoutList does not follow the
  // the order of the layouts in the control panel so we use GetKeyboardLayout
  // to retrieve the current layout and swap (if needed) to ensure it is always
  // evaluated first.
  auto iter = std::find(keyboard_layout_handles_.begin(),
                        keyboard_layout_handles_.end(), GetKeyboardLayout(0));
  if (iter != keyboard_layout_handles_.begin() &&
      iter != keyboard_layout_handles_.end())
    std::iter_swap(keyboard_layout_handles_.begin(), iter);

  return keyboard_layout_handles_.size();
}

ui::DomKey DomKeyboardLayoutMapWin::GetDomKeyFromDomCodeForLayout(
    ui::DomCode dom_code,
    uint32_t keyboard_layout_index) {
  DCHECK_NE(dom_code, ui::DomCode::NONE);
  DCHECK_LT(keyboard_layout_index, keyboard_layout_handles_.size());

  HKL keyboard_layout = keyboard_layout_handles_[keyboard_layout_index];
  int32_t scan_code = ui::KeycodeConverter::DomCodeToNativeKeycode(dom_code);
  uint32_t virtual_key_code =
      MapVirtualKeyEx(scan_code, MAPVK_VSC_TO_VK_EX, keyboard_layout);
  if (!virtual_key_code) {
    if (GetLastError() != 0)
      DPLOG(ERROR) << "MapVirtualKeyEx failed: ";
    return ui::DomKey::NONE;
  }

  // Represents a keyboard state with all keys up (i.e. no keys pressed).
  BYTE keyboard_state[256] = {0};

  // ToUnicodeEx() return value indicates the category for the scan code
  // passed in for the keyboard layout provided.
  // https://msdn.microsoft.com/en-us/library/windows/desktop/ms646322(v=vs.85).aspx
  wchar_t char_buffer[1] = {0};
  int key_type =
      ::ToUnicodeEx(virtual_key_code, scan_code, keyboard_state, char_buffer,
                    base::size(char_buffer), /*wFlags=*/0, keyboard_layout);

  // Handle special cases for Japanese keyboard layout.
  if (0x04110411 == reinterpret_cast<unsigned int>(keyboard_layout)) {
    // Fix value for Japanese yen currency symbol.
    // Windows returns '\' for both IntlRo and IntlYen, even though IntlYen
    // should be the yen symbol.
    if (dom_code == ui::DomCode::INTL_YEN)
      return ui::DomKey::FromCharacter(0x00a5);  // Japanese yen symbol.

    // Special case for Backquote.
    // Technically, this layout is not completely ASCII-capable because the
    // Backquote key is used as an IME function key (hankaku/zenkaku) and is
    // thus not a printable key. However, other than this key, it is a perfectly
    // usable ASCII-capable layout and it matches the values printed on the
    // keyboard, so we have special handling to allow this key.
    if (dom_code == ui::DomCode::BACKQUOTE)
      return ui::DomKey::ZENKAKU_HANKAKU;
  }

  // Handle special cases for Korean keyboard layout.
  if (0x04120412 == reinterpret_cast<unsigned int>(keyboard_layout)) {
    // Fix value for Korean won currency symbol.
    // Windows returns '\' for both Backslash and IntlBackslash, even though
    // IntlBackslash should be the won symbol.
    if (dom_code == ui::DomCode::INTL_BACKSLASH)
      return ui::DomKey::FromCharacter(0x20a9);  // Korean won symbol.
  }

  ui::DomKey key = ui::DomKey::NONE;
  if (key_type == 1)
    key = ui::DomKey::FromCharacter(char_buffer[0]);
  else if (key_type == -1) {
    key = ui::DomKey::DeadKeyFromCombiningCharacter(char_buffer[0]);

    // When we query info about dead keys, the system is left in a state
    // such that the next key queried is in the context of that dead key.
    // This causes ToUnicodeEx to return an incorrect result for the second
    // key. To fix this we query a Space key after any dead key to clear out
    // the dead key state. See crbug/977609 for details on how this problem
    // exhibits itself to users.
    ::ToUnicodeEx(0x0020, 0x0039, keyboard_state, char_buffer,
                  base::size(char_buffer), /*wFlags=*/0, keyboard_layout);
  }
  return key;
}

}  // namespace

// static
base::flat_map<std::string, std::string> GenerateDomKeyboardLayoutMap() {
  return DomKeyboardLayoutMapWin().Generate();
}

}  // namespace ui
