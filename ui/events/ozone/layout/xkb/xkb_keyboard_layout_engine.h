// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback_forward.h"
#include "base/memory/free_deleter.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_runner.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/keycodes/scoped_xkb.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/xkb/xkb_key_code_converter.h"
#include "ui/events/ozone/layout/xkb/xkb_modifier_converter.h"

struct xkb_keymap;

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) XkbKeyboardLayoutEngine
    : public KeyboardLayoutEngine {
 public:
  explicit XkbKeyboardLayoutEngine(const XkbKeyCodeConverter& converter);
  ~XkbKeyboardLayoutEngine() override;

  // KeyboardLayoutEngine:
  std::string_view GetLayoutName() const override;
  bool CanSetCurrentLayout() const override;
  void SetCurrentLayoutByName(const std::string& layout_name,
                              base::OnceCallback<void(bool)> callback) override;
  // Required by Ozone/Wayland (at least) for non ChromeOS builds. See
  // http://xkbcommon.org/doc/current/md_doc_quick-guide.html for further info.
  bool SetCurrentLayoutFromBuffer(const char* keymap_string,
                                  size_t size) override;

  bool UsesISOLevel5Shift() const override;
  bool UsesAltGr() const override;

  bool Lookup(DomCode dom_code,
              int flags,
              DomKey* dom_key,
              KeyboardCode* key_code) const override;

  void SetInitCallbackForTest(base::OnceClosure closure) override;

  int UpdateModifiers(uint32_t depressed,
                      uint32_t latched,
                      uint32_t locked,
                      uint32_t group);

  // modifiers is optional for backward compatibility purpose.
  // This should be removed when we no longer need to support older platform,
  // specifically M101 or earlier of ash-chrome.
  DomCode GetDomCodeByKeysym(
      uint32_t keysym,
      const std::optional<std::vector<std::string_view>>& modifiers) const;

  static void ParseLayoutName(const std::string& layout_name,
                              std::string* layout_id,
                              std::string* layout_variant);

 protected:
  // Table for EventFlagsToXkbFlags().
  struct XkbFlagMapEntry {
    int ui_flag;
    xkb_mod_mask_t xkb_flag;
    xkb_mod_index_t xkb_index;
  };
  std::vector<XkbFlagMapEntry> xkb_flag_map_;

  // The data to reverse look up xkb_keycode/xkb_layout from xkb_keysym.
  // The data is sorted in the (xkb_keysym, xkb_keycode, xkb_layout) dictionary
  // order. Note that there can be multiple keycode/layout for a keysym, so
  // this is a multi map.
  // We can binary search on this vector by keysym as the key, and iterate from
  // the begin to the end of the range linearly. Then, on tie break, smaller
  // keycode wins.
  struct XkbKeysymMapEntry {
    xkb_keysym_t xkb_keysym;
    xkb_keycode_t xkb_keycode;
    xkb_layout_index_t xkb_layout;
  };
  std::vector<XkbKeysymMapEntry> xkb_keysym_map_;

  // Maps between ui::EventFlags and xkb_mod_mask_t.
  XkbModifierConverter xkb_modifier_converter_{{}};

  xkb_mod_mask_t shift_mod_mask_ = 0;
  xkb_mod_mask_t altgr_mod_mask_ = 0;

  // Determines the Windows-based KeyboardCode (VKEY) for a character key,
  // accounting for non-US layouts. May return VKEY_UNKNOWN, in which case the
  // caller should, as a last resort, obtain a KeyboardCode using
  // |DomCodeToUsLayoutDomKey()|.
  KeyboardCode DifficultKeyboardCode(DomCode dom_code,
                                     int ui_flags,
                                     xkb_keycode_t xkb_keycode,
                                     xkb_mod_mask_t xkb_flags,
                                     xkb_keysym_t xkb_keysym,
                                     char16_t character) const;

  // Sets a new XKB keymap. This updates xkb_state_ (which takes ownership
  // of the keymap), and updates xkb_flag_map_ for the new keymap.
  virtual void SetKeymap(xkb_keymap* keymap);

  // Maps DomCode to xkb_keycode_t.
  const raw_ref<const XkbKeyCodeConverter> key_code_converter_;

  // libxkbcommon uses explicit reference counting for its structures,
  // so we need to trigger its cleanup.
  std::unique_ptr<xkb_state, XkbStateDeleter> xkb_state_;

 private:
  struct XkbKeymapEntry {
    std::string layout_name;
    raw_ptr<xkb_keymap> keymap;
  };
  std::vector<XkbKeymapEntry> xkb_keymaps_;

  // Returns the XKB modifiers flags corresponding to the given EventFlags.
  xkb_mod_mask_t EventFlagsToXkbFlags(int ui_flags) const;

  // Determines the XKB KeySym and character associated with a key.
  // Returns true on success. This is virtual for testing.
  virtual bool XkbLookup(xkb_keycode_t xkb_keycode,
                         xkb_mod_mask_t xkb_flags,
                         xkb_keysym_t* xkb_keysym,
                         uint32_t* character) const;

  // Helper for difficult VKEY lookup. If |ui_flags| matches |base_flags|,
  // returns |base_character|; otherwise returns the XKB character for
  // the keycode and mapped |ui_flags|.
  char16_t XkbSubCharacter(xkb_keycode_t xkb_keycode,
                           xkb_mod_mask_t base_flags,
                           char16_t base_character,
                           xkb_mod_mask_t flags) const;

  // Callback when keymap file is loaded complete.
  void OnKeymapLoaded(base::OnceCallback<void(bool)> callback,
                      const std::string& layout_name,
                      std::unique_ptr<char, base::FreeDeleter> keymap_str);

  std::unique_ptr<xkb_context, XkbContextDeleter> xkb_context_;

  // Holds the keymap from xkb_keymap_new_from_buffer.
  std::unique_ptr<xkb_keymap, XkbKeymapDeleter> key_map_from_buffer_;

  std::string current_layout_name_;

  xkb_layout_index_t layout_index_ = 0;

  base::OnceClosure keymap_init_closure_for_test_;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<XkbKeyboardLayoutEngine> weak_ptr_factory_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
