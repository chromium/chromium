// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_composer.h"

#include <locale.h>
#include <xkbcommon/xkbcommon-compose.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversion_utils.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/thread_pool.h"
#include "base/third_party/icu/icu_utf.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/scoped_xkb.h"

namespace ui {

namespace {

constexpr int kShortcutModifiers =
    EF_CONTROL_DOWN | EF_ALT_DOWN | EF_COMMAND_DOWN;

struct ComposeTableUnref {
  void operator()(xkb_compose_table* table) const {
    xkb_compose_table_unref(table);
  }
};
using ScopedComposeTable =
    std::unique_ptr<xkb_compose_table, ComposeTableUnref>;

// The process-wide default compose table. Only accessed on the UI thread; the
// table is compiled on the thread pool and handed over in
// OnDefaultComposeTableCreated().
struct DefaultComposeTable {
  enum class State { kNotStarted, kLoading, kLoaded };
  State state = State::kNotStarted;
  ScopedComposeTable table;
};

DefaultComposeTable& GetDefaultComposeTable() {
  static base::NoDestructor<DefaultComposeTable> default_table;
  return *default_table;
}

// Runs on the thread pool, since it parses the locale's Compose file (about
// 450 KB for en_US.UTF-8) and the user's XCompose files. The context is local
// to this task; the returned table holds the only other reference to it.
ScopedComposeTable CreateDefaultComposeTable() {
  std::unique_ptr<xkb_context, XkbContextDeleter> context(
      xkb_context_new(XKB_CONTEXT_NO_FLAGS));
  if (!context) {
    return nullptr;
  }

  const char* raw_locale = setlocale(LC_CTYPE, nullptr);
  std::string locale;
  if (!raw_locale || !*raw_locale || std::string_view(raw_locale) == "C" ||
      std::string_view(raw_locale) == "POSIX") {
    // On libxkbcommon < 1.7 (e.g. Ubuntu 22.04), "C" and "POSIX" resolve to
    // iso8859-1/Compose instead of en_US.UTF-8/Compose. Map them to
    // en_US.UTF-8 to match libxkbcommon >= 1.7 while still honouring
    // XCOMPOSEFILE, $XDG_CONFIG_HOME/XCompose, and ~/.XCompose.
    locale = "en_US.UTF-8";
  } else {
    locale = raw_locale;
  }

  ScopedComposeTable table(xkb_compose_table_new_from_locale(
      context.get(), locale.c_str(), XKB_COMPOSE_COMPILE_NO_FLAGS));
  if (!table && locale != "en_US.UTF-8") {
    // Retry en_US.UTF-8 for locales missing from compose.dir (such as
    // es_419.UTF-8 on libxkbcommon < 1.12).
    table.reset(xkb_compose_table_new_from_locale(
        context.get(), "en_US.UTF-8", XKB_COMPOSE_COMPILE_NO_FLAGS));
  }
  return table;
}

void OnDefaultComposeTableCreated(ScopedComposeTable table) {
  DefaultComposeTable& default_table = GetDefaultComposeTable();
  // A reset for testing may have happened while the task was running.
  if (default_table.state != DefaultComposeTable::State::kLoading) {
    return;
  }
  default_table.state = DefaultComposeTable::State::kLoaded;
  default_table.table = std::move(table);
}

xkb_context* GetSharedXkbContext() {
  static base::NoDestructor<std::unique_ptr<xkb_context, XkbContextDeleter>>
      context(xkb_context_new(XKB_CONTEXT_NO_FLAGS));
  return context->get();
}

bool IsDeadKeySym(xkb_keysym_t keysym) {
  return keysym >= XKB_KEY_dead_grave &&
         keysym <= XKB_KEY_dead_longsolidusoverlay;
}

bool IsModifierKeySym(xkb_keysym_t keysym) {
  return (keysym >= XKB_KEY_Shift_L && keysym <= XKB_KEY_Hyper_R) ||
         (keysym >= XKB_KEY_ISO_Lock && keysym <= XKB_KEY_ISO_Level5_Lock) ||
         keysym == XKB_KEY_Mode_switch || keysym == XKB_KEY_Num_Lock;
}

std::optional<char16_t> DeadKeySymToPreeditChar(xkb_keysym_t keysym) {
  switch (keysym) {
    case XKB_KEY_dead_grave:
      return u'`';
    case XKB_KEY_dead_acute:
      return u'´';
    case XKB_KEY_dead_circumflex:
      return u'^';
    case XKB_KEY_dead_tilde:
      return u'~';
    case XKB_KEY_dead_macron:
      return u'¯';
    case XKB_KEY_dead_breve:
      return u'˘';
    case XKB_KEY_dead_abovedot:
      return u'˙';
    case XKB_KEY_dead_diaeresis:
      return u'¨';
    case XKB_KEY_dead_abovering:
      return u'˚';
    case XKB_KEY_dead_doubleacute:
      return u'˝';
    case XKB_KEY_dead_caron:
      return u'ˇ';
    case XKB_KEY_dead_cedilla:
      return u'¸';
    case XKB_KEY_dead_ogonek:
      return u'˛';
    case XKB_KEY_dead_iota:
      return u'ͺ';
    case XKB_KEY_dead_voiced_sound:
      return 0x309B;  // KATAKANA-HIRAGANA VOICED SOUND MARK
    case XKB_KEY_dead_semivoiced_sound:
      return 0x309C;  // KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK
    case XKB_KEY_dead_stroke:
    case XKB_KEY_dead_longsolidusoverlay:
      return u'/';
    case XKB_KEY_dead_currency:
      return 0x00A4;  // CURRENCY SIGN
    case XKB_KEY_dead_lowline:
      return u'_';
    default:
      return std::nullopt;
  }
}

std::u16string GetComposedUtf16(xkb_compose_state* state) {
  std::array<char, 64> stack_buf{};
  int len =
      xkb_compose_state_get_utf8(state, stack_buf.data(), stack_buf.size());
  if (len <= 0) {
    return std::u16string();
  }
  const size_t byte_len = static_cast<size_t>(len);
  if (byte_len < stack_buf.size()) {
    return base::UTF8ToUTF16(std::string_view(stack_buf.data(), byte_len));
  }
  std::string heap_buf(byte_len + 1, '\0');
  xkb_compose_state_get_utf8(state, heap_buf.data(), heap_buf.size());
  heap_buf.resize(byte_len);
  return base::UTF8ToUTF16(heap_buf);
}

}  // namespace

void XkbComposer::ComposeTableDeleter::operator()(
    xkb_compose_table* table) const {
  xkb_compose_table_unref(table);
}

void XkbComposer::ComposeStateDeleter::operator()(
    xkb_compose_state* state) const {
  xkb_compose_state_unref(state);
}

XkbComposer::XkbComposer() = default;

XkbComposer::~XkbComposer() = default;

// static
void XkbComposer::LoadDefaultComposeTable() {
  DefaultComposeTable& default_table = GetDefaultComposeTable();
  if (default_table.state != DefaultComposeTable::State::kNotStarted) {
    return;
  }
  default_table.state = DefaultComposeTable::State::kLoading;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&CreateDefaultComposeTable),
      base::BindOnce(&OnDefaultComposeTableCreated));
}

// static
void XkbComposer::ResetDefaultComposeTableForTesting() {
  DefaultComposeTable& default_table = GetDefaultComposeTable();
  default_table.state = DefaultComposeTable::State::kNotStarted;
  default_table.table.reset();
}

bool XkbComposer::EnsureInitialized() {
  if (initialized_) {
    return state_ != nullptr;
  }

  const DefaultComposeTable& default_table = GetDefaultComposeTable();
  if (default_table.state != DefaultComposeTable::State::kLoaded) {
    // CharacterComposer handles keys until the table is ready.
    LoadDefaultComposeTable();
    return false;
  }
  initialized_ = true;
  if (!default_table.table) {
    return false;
  }

  table_.reset(xkb_compose_table_ref(default_table.table.get()));
  state_.reset(xkb_compose_state_new(table_.get(), XKB_COMPOSE_STATE_NO_FLAGS));
  if (!state_) {
    table_.reset();
    return false;
  }
  return true;
}

bool XkbComposer::SetComposeTableFromBuffer(std::string_view buffer,
                                            const char* locale) {
  initialized_ = true;
  Reset();
  state_.reset();
  table_.reset();

  xkb_context* context = GetSharedXkbContext();
  if (!context) {
    return false;
  }

  table_.reset(xkb_compose_table_new_from_buffer(
      context, buffer.data(), buffer.size(), locale ? locale : "C",
      XKB_COMPOSE_FORMAT_TEXT_V1, XKB_COMPOSE_COMPILE_NO_FLAGS));
  if (!table_) {
    return false;
  }

  state_.reset(xkb_compose_state_new(table_.get(), XKB_COMPOSE_STATE_NO_FLAGS));
  if (!state_) {
    table_.reset();
    return false;
  }
  return true;
}

void XkbComposer::DisableComposeTableForTesting() {
  initialized_ = true;
  Reset();
  state_.reset();
  table_.reset();
}

bool XkbComposer::FilterKeyPress(xkb_keysym_t keysym, int event_flags) {
  const bool has_shortcut_modifier = (event_flags & kShortcutModifiers) != 0;
  const bool is_compose_or_dead =
      keysym == XKB_KEY_Multi_key || IsDeadKeySym(keysym);

  // Do not intercept Ctrl/Alt/Super shortcuts when idle.
  if (!is_composing() && has_shortcut_modifier && !is_compose_or_dead) {
    composed_character_.clear();
    return false;
  }

  if (!EnsureInitialized()) {
    return false;
  }

  // Feed keysym to libxkbcommon. If a shortcut modifier is held mid-sequence on
  // a non-modifier key (e.g. Ctrl+C), feed XKB_KEY_NoSymbol so the pending
  // sequence cancels instead of matching the modified character; feed `keysym`
  // first only if libxkbcommon ignores it (modifier keysyms like Control_L).
  xkb_compose_feed_result feed_result = XKB_COMPOSE_FEED_IGNORED;
  if (is_composing() && has_shortcut_modifier && !is_compose_or_dead) {
    // Modifier presses themselves (e.g. Shift_L while Control is held) are
    // ignored by libxkbcommon without altering state; non-modifier keysyms
    // (including keys in 0xff00..0xffe0 such as KP_* or navigation keys) cancel
    // the sequence via NoSymbol.
    if (IsModifierKeySym(keysym)) {
      feed_result = xkb_compose_state_feed(state_.get(), keysym);
    } else {
      feed_result = xkb_compose_state_feed(state_.get(), XKB_KEY_NoSymbol);
    }
  } else {
    feed_result = xkb_compose_state_feed(state_.get(), keysym);
  }

  if (feed_result == XKB_COMPOSE_FEED_IGNORED) {
    composed_character_.clear();
    return false;
  }

  switch (xkb_compose_state_get_status(state_.get())) {
    case XKB_COMPOSE_NOTHING:
      composed_character_.clear();
      preedit_string_.clear();
      sequence_.clear();
      return false;

    case XKB_COMPOSE_COMPOSING:
      composed_character_.clear();
      sequence_.push_back(keysym);
      UpdatePreeditString();
      return true;

    case XKB_COMPOSE_COMPOSED:
      composed_character_ = GetComposedUtf16(state_.get());
      if (dead_acute_c_produces_cedilla_ && sequence_.size() == 1 &&
          sequence_[0] == XKB_KEY_dead_acute) {
        if (composed_character_ == u"\u0107") {
          composed_character_ = u"\u00E7";
        } else if (composed_character_ == u"\u0106") {
          composed_character_ = u"\u00C7";
        }
      }
      xkb_compose_state_reset(state_.get());
      sequence_.clear();
      preedit_string_.clear();
      return true;

    case XKB_COMPOSE_CANCELLED:
      xkb_compose_state_reset(state_.get());
      composed_character_.clear();
      sequence_.clear();
      preedit_string_.clear();
      return true;
  }
  return false;
}

void XkbComposer::Reset() {
  if (state_) {
    xkb_compose_state_reset(state_.get());
  }
  sequence_.clear();
  composed_character_.clear();
  preedit_string_.clear();
}

void XkbComposer::UpdatePreeditString() {
  preedit_string_.clear();
  if (sequence_.empty()) {
    return;
  }
  for (xkb_keysym_t keysym : sequence_) {
    if (keysym == XKB_KEY_Multi_key) {
      if (sequence_.size() == 1) {
        preedit_string_.push_back(kPreeditComposeSymbol);
      }
      continue;
    }
    if (std::optional<char16_t> dead_char = DeadKeySymToPreeditChar(keysym)) {
      preedit_string_.push_back(*dead_char);
      continue;
    }
    uint32_t codepoint = xkb_keysym_to_utf32(keysym);
    if (codepoint >= 0x20 && !(codepoint >= 0x7f && codepoint <= 0x9f) &&
        CBU_IS_UNICODE_CHAR(codepoint)) {
      base::WriteUnicodeCharacter(codepoint, &preedit_string_);
    }
  }
  if (preedit_string_.empty()) {
    preedit_string_.push_back(kPreeditComposeSymbol);
  }
}

}  // namespace ui
