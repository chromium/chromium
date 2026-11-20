// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_OZONE_LAYOUT_XKB_XKB_COMPOSER_H_
#define UI_EVENTS_OZONE_LAYOUT_XKB_XKB_COMPOSER_H_

#include <xkbcommon/xkbcommon.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/component_export.h"

struct xkb_compose_state;
struct xkb_compose_table;

namespace ui {

// Composes dead-key and Multi_key (Compose) sequences using libxkbcommon's
// compose state machine, honouring XCOMPOSEFILE, $XDG_CONFIG_HOME/XCompose,
// ~/.XCompose, and the active locale's system Compose table.
class COMPONENT_EXPORT(EVENTS_OZONE_LAYOUT) XkbComposer {
 public:
  // U+00B7 MIDDLE DOT, matching CharacterComposer and GTK for pending
  // Multi_key preedit.
  static constexpr char16_t kPreeditComposeSymbol = 0x00B7;

  XkbComposer();
  XkbComposer(const XkbComposer&) = delete;
  XkbComposer& operator=(const XkbComposer&) = delete;
  ~XkbComposer();

  // Starts compiling the compose table for the current LC_CTYPE locale (and
  // the user's XCompose files) on the thread pool. Only the first call per
  // process has an effect. Must be called on the UI thread.
  static void LoadDefaultComposeTable();

  // Forgets the default compose table so that the next
  // LoadDefaultComposeTable() compiles it again.
  static void ResetDefaultComposeTableForTesting();

  // Attaches the default compose table once it has loaded. Returns true if a
  // compose table is ready, or false while it is still loading or if none
  // could be loaded.
  bool EnsureInitialized();

  // Compiles and installs a compose table from an in-memory Compose file
  // buffer (for testing). Marks initialization complete; clears any existing
  // state and returns false on compilation failure.
  bool SetComposeTableFromBuffer(std::string_view buffer,
                                 const char* locale = "en_US.UTF-8");

  // Marks initialization complete with no table loaded (for testing fallback
  // paths when no compose table is available).
  void DisableComposeTableForTesting();

  bool HasComposeTable() const { return state_ != nullptr; }
  bool is_composing() const { return !sequence_.empty(); }

  // Feeds `keysym` (with `event_flags` from ui::EventFlags) into the compose
  // state machine. Returns true if the key press was consumed as part of a
  // compose sequence (advancing, completing, or cancelling it).
  bool FilterKeyPress(xkb_keysym_t keysym, int event_flags);

  void Reset();

  const std::u16string& composed_character() const {
    return composed_character_;
  }
  const std::u16string& preedit_string() const { return preedit_string_; }

  // When enabled, Dead<acute> followed by c/C composes U+00E7/U+00C7 (c with
  // cedilla) where the compose table gives U+0107/U+0106 (c with acute), as
  // GTK's "cedilla" input method does.
  void set_dead_acute_c_produces_cedilla(bool enabled) {
    dead_acute_c_produces_cedilla_ = enabled;
  }

 private:
  struct ComposeTableDeleter {
    void operator()(xkb_compose_table* table) const;
  };
  struct ComposeStateDeleter {
    void operator()(xkb_compose_state* state) const;
  };

  void UpdatePreeditString();

  bool initialized_ = false;
  bool dead_acute_c_produces_cedilla_ = false;
  std::unique_ptr<xkb_compose_table, ComposeTableDeleter> table_;
  std::unique_ptr<xkb_compose_state, ComposeStateDeleter> state_;
  std::vector<xkb_keysym_t> sequence_;
  std::u16string composed_character_;
  std::u16string preedit_string_;
};

}  // namespace ui

#endif  // UI_EVENTS_OZONE_LAYOUT_XKB_XKB_COMPOSER_H_
