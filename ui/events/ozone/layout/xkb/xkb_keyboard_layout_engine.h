// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_

#include <stdint.h>
#include <xkbcommon/xkbcommon.h>

#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/containers/flat_map.h"
#include "base/memory/free_deleter.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/task_runner.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/keycodes/scoped_xkb.h"
#include "ui/events/ozone/layout/keyboard_layout_engine.h"
#include "ui/events/ozone/layout/xkb/xkb_key_code_converter.h"

struct xkb_keymap;

namespace ui {

class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) XkbKeyboardLayoutEngine
    : public KeyboardLayoutEngine {
 public:
  explicit XkbKeyboardLayoutEngine(const XkbKeyCodeConverter& converter);
  ~XkbKeyboardLayoutEngine() override;

  // KeyboardLayoutEngine:
  bool CanSetCurrentLayout() const override;
  bool SetCurrentLayoutByName(const std::string& layout_name) override;
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

  int UpdateModifiers(uint32_t depressed,
                      uint32_t latched,
                      uint32_t locked,
                      uint32_t group);

  DomCode GetDomCodeByKeysym(uint32_t keysym) const;

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

  // Table from xkb_keysym to xkb_keycode on the current keymap.
  // Note that there could be multiple keycodes mapped to the same
  // keysym. In the case, the first one (smallest keycode) will be
  // kept.
  base::flat_map<uint32_t, uint32_t> xkb_keysym_map_;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Flag mask for num lock, which is always considered enabled in ChromeOS.
  xkb_mod_mask_t num_lock_mod_mask_ = 0;
#endif
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
                                     base::char16 character) const;

  // Sets a new XKB keymap. This updates xkb_state_ (which takes ownership
  // of the keymap), and updates xkb_flag_map_ for the new keymap.
  virtual void SetKeymap(xkb_keymap* keymap);

  // Maps DomCode to xkb_keycode_t.
  const XkbKeyCodeConverter& key_code_converter_;

  // libxkbcommon uses explicit reference counting for its structures,
  // so we need to trigger its cleanup.
  std::unique_ptr<xkb_state, XkbStateDeleter> xkb_state_;

 private:
  struct XkbKeymapEntry {
    std::string layout_name;
    xkb_keymap* keymap;
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
  base::char16 XkbSubCharacter(xkb_keycode_t xkb_keycode,
                               xkb_mod_mask_t base_flags,
                               base::char16 base_character,
                               xkb_mod_mask_t flags) const;

  // Callback when keymap file is loaded complete.
  void OnKeymapLoaded(const std::string& layout_name,
                      std::unique_ptr<char, base::FreeDeleter> keymap_str);

  std::unique_ptr<xkb_context, XkbContextDeleter> xkb_context_;

  // Holds the keymap from xkb_keymap_new_from_buffer.
  std::unique_ptr<xkb_keymap, XkbKeymapDeleter> key_map_from_buffer_;

  std::string current_layout_name_;

  xkb_layout_index_t layout_index_ = 0;

  // Support weak pointers for attach & detach callbacks.
  base::WeakPtrFactory<XkbKeyboardLayoutEngine> weak_ptr_factory_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_KEYBOARD_LAYOUT_ENGINE_H_
