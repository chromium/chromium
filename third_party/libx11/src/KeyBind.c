/*

Copyright 1985, 1987, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

*/

uint32_t KeycodeColumnToKeysym(KeyCode keycode,
                               int column,
                               const GetKeyboardMappingReply& keyboard_mapping,
                               uint8_t min_keycode,
                               uint8_t max_keycode);

// Ported from ResetModMap:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/KeyBind.c#L160
void UpdateMappingImpl(Connection* connection,
                       GetKeyboardMappingReply* keyboard_mapping,
                       uint16_t* lock_meaning,
                       uint8_t* mode_switch,
                       uint8_t* num_lock) {
  auto min_keycode = static_cast<uint8_t>(connection->setup().min_keycode);
  auto max_keycode = static_cast<uint8_t>(connection->setup().max_keycode);

  uint8_t count = max_keycode - min_keycode + 1;
  auto keyboard_future =
      connection->GetKeyboardMapping({connection->setup().min_keycode, count});
  auto modifier_future = connection->GetModifierMapping();
  GetModifierMappingReply modifier_mapping;
  connection->Flush();
  if (auto reply = keyboard_future.Sync())
    *keyboard_mapping = std::move(*reply.reply);
  if (auto reply = modifier_future.Sync())
    modifier_mapping = std::move(*reply.reply);

  for (uint8_t i = 0; i < modifier_mapping.keycodes_per_modifier; i++) {
    // Lock modifiers are in the second row of the matrix
    size_t index = 2 * modifier_mapping.keycodes_per_modifier + i;
    for (uint8_t j = 0; j < keyboard_mapping->keysyms_per_keycode; j++) {
      auto sym =
          KeycodeColumnToKeysym(modifier_mapping.keycodes[index], j,
                                *keyboard_mapping, min_keycode, max_keycode);
      if (sym == XK_Caps_Lock || sym == XK_ISO_Lock) {
        *lock_meaning = XK_Caps_Lock;
        break;
      }
      if (sym == XK_Shift_Lock)
        *lock_meaning = XK_Shift_Lock;
    }
  }

  // Mod<n> is at row (n + 2) of the matrix.  This iterates from Mod1 to Mod5.
  for (int mod = 3; mod < 8; mod++) {
    for (size_t i = 0; i < modifier_mapping.keycodes_per_modifier; i++) {
      size_t index = mod * modifier_mapping.keycodes_per_modifier + i;
      for (uint8_t j = 0; j < keyboard_mapping->keysyms_per_keycode; j++) {
        auto sym =
            KeycodeColumnToKeysym(modifier_mapping.keycodes[index], j,
                                  *keyboard_mapping, min_keycode, max_keycode);
        if (sym == XK_Mode_switch)
          *mode_switch |= 1 << mod;
        if (sym == XK_Num_Lock)
          *num_lock |= 1 << mod;
      }
    }
  }
}

// Ported from XConvertCase:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/KeyBind.c#L645
void ConvertCaseImpl(uint32_t sym, uint32_t* lower, uint32_t* upper) {
  // Unicode keysym
  if ((sym & 0xff000000) == 0x01000000) {
    std::u16string string({static_cast<char16_t>(sym)});
    auto lower_string = base::i18n::ToLower(string);
    auto upper_string = base::i18n::ToUpper(string);
    *lower = lower_string[0] | 0x01000000;
    *upper = upper_string[0] | 0x01000000;
    return;
  }

  *lower = sym;
  *upper = sym;

  switch (sym >> 8) {
    // Latin 1
    case 0:
      if ((sym >= XK_A) && (sym <= XK_Z))
        *lower += (XK_a - XK_A);
      else if ((sym >= XK_a) && (sym <= XK_z))
        *upper -= (XK_a - XK_A);
      else if ((sym >= XK_Agrave) && (sym <= XK_Odiaeresis))
        *lower += (XK_agrave - XK_Agrave);
      else if ((sym >= XK_agrave) && (sym <= XK_odiaeresis))
        *upper -= (XK_agrave - XK_Agrave);
      else if ((sym >= XK_Ooblique) && (sym <= XK_Thorn))
        *lower += (XK_oslash - XK_Ooblique);
      else if ((sym >= XK_oslash) && (sym <= XK_thorn))
        *upper -= (XK_oslash - XK_Ooblique);
      break;
    // Latin 2
    case 1:
      if (sym == XK_Aogonek)
        *lower = XK_aogonek;
      else if (sym >= XK_Lstroke && sym <= XK_Sacute)
        *lower += (XK_lstroke - XK_Lstroke);
      else if (sym >= XK_Scaron && sym <= XK_Zacute)
        *lower += (XK_scaron - XK_Scaron);
      else if (sym >= XK_Zcaron && sym <= XK_Zabovedot)
        *lower += (XK_zcaron - XK_Zcaron);
      else if (sym == XK_aogonek)
        *upper = XK_Aogonek;
      else if (sym >= XK_lstroke && sym <= XK_sacute)
        *upper -= (XK_lstroke - XK_Lstroke);
      else if (sym >= XK_scaron && sym <= XK_zacute)
        *upper -= (XK_scaron - XK_Scaron);
      else if (sym >= XK_zcaron && sym <= XK_zabovedot)
        *upper -= (XK_zcaron - XK_Zcaron);
      else if (sym >= XK_Racute && sym <= XK_Tcedilla)
        *lower += (XK_racute - XK_Racute);
      else if (sym >= XK_racute && sym <= XK_tcedilla)
        *upper -= (XK_racute - XK_Racute);
      break;
    // Latin 3
    case 2:
      if (sym >= XK_Hstroke && sym <= XK_Hcircumflex)
        *lower += (XK_hstroke - XK_Hstroke);
      else if (sym >= XK_Gbreve && sym <= XK_Jcircumflex)
        *lower += (XK_gbreve - XK_Gbreve);
      else if (sym >= XK_hstroke && sym <= XK_hcircumflex)
        *upper -= (XK_hstroke - XK_Hstroke);
      else if (sym >= XK_gbreve && sym <= XK_jcircumflex)
        *upper -= (XK_gbreve - XK_Gbreve);
      else if (sym >= XK_Cabovedot && sym <= XK_Scircumflex)
        *lower += (XK_cabovedot - XK_Cabovedot);
      else if (sym >= XK_cabovedot && sym <= XK_scircumflex)
        *upper -= (XK_cabovedot - XK_Cabovedot);
      break;
    // Latin 4
    case 3:
      if (sym >= XK_Rcedilla && sym <= XK_Tslash)
        *lower += (XK_rcedilla - XK_Rcedilla);
      else if (sym >= XK_rcedilla && sym <= XK_tslash)
        *upper -= (XK_rcedilla - XK_Rcedilla);
      else if (sym == XK_ENG)
        *lower = XK_eng;
      else if (sym == XK_eng)
        *upper = XK_ENG;
      else if (sym >= XK_Amacron && sym <= XK_Umacron)
        *lower += (XK_amacron - XK_Amacron);
      else if (sym >= XK_amacron && sym <= XK_umacron)
        *upper -= (XK_amacron - XK_Amacron);
      break;
    // Cyrillic
    case 6:
      if (sym >= XK_Serbian_DJE && sym <= XK_Serbian_DZE)
        *lower -= (XK_Serbian_DJE - XK_Serbian_dje);
      else if (sym >= XK_Serbian_dje && sym <= XK_Serbian_dze)
        *upper += (XK_Serbian_DJE - XK_Serbian_dje);
      else if (sym >= XK_Cyrillic_YU && sym <= XK_Cyrillic_HARDSIGN)
        *lower -= (XK_Cyrillic_YU - XK_Cyrillic_yu);
      else if (sym >= XK_Cyrillic_yu && sym <= XK_Cyrillic_hardsign)
        *upper += (XK_Cyrillic_YU - XK_Cyrillic_yu);
      break;
    // Greek
    case 7:
      if (sym >= XK_Greek_ALPHAaccent && sym <= XK_Greek_OMEGAaccent)
        *lower += (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
      else if (sym >= XK_Greek_alphaaccent && sym <= XK_Greek_omegaaccent &&
               sym != XK_Greek_iotaaccentdieresis &&
               sym != XK_Greek_upsilonaccentdieresis)
        *upper -= (XK_Greek_alphaaccent - XK_Greek_ALPHAaccent);
      else if (sym >= XK_Greek_ALPHA && sym <= XK_Greek_OMEGA)
        *lower += (XK_Greek_alpha - XK_Greek_ALPHA);
      else if (sym >= XK_Greek_alpha && sym <= XK_Greek_omega &&
               sym != XK_Greek_finalsmallsigma)
        *upper -= (XK_Greek_alpha - XK_Greek_ALPHA);
      break;
    // Latin 9
    case 0x13:
      if (sym == XK_OE)
        *lower = XK_oe;
      else if (sym == XK_oe)
        *upper = XK_OE;
      else if (sym == XK_Ydiaeresis)
        *lower = XK_ydiaeresis;
      break;
  }
}

// Ported from _XTranslateKey:
// https://gitlab.freedesktop.org/xorg/lib/libx11/-/blob/2b7598221d87049d03e9a95fcb541c37c8728184/src/KeyBind.c#L761
KeySym KeycodeToKeysymCoreImpl(KeyCode key,
                               uint32_t modifiers,
                               Connection* connection,
                               const GetKeyboardMappingReply& keyboard_mapping,
                               uint16_t lock_meaning,
                               uint8_t mode_switch,
                               uint8_t num_lock) {
  constexpr auto kShiftMask = static_cast<unsigned int>(x11::ModMask::Shift);
  constexpr auto kLockMask = static_cast<unsigned int>(x11::ModMask::Lock);

  uint8_t keycode = static_cast<uint8_t>(key);
  uint8_t min_key = static_cast<uint8_t>(connection->setup().min_keycode);
  uint8_t max_key = static_cast<uint8_t>(connection->setup().max_keycode);
  if (keycode < min_key || keycode > max_key)
    return kNoSymbol;

  uint8_t n_keysyms = keyboard_mapping.keysyms_per_keycode;
  if (!n_keysyms)
    return {};
  const auto* syms = &keyboard_mapping.keysyms[(keycode - min_key) * n_keysyms];
  while ((n_keysyms > 2) && (syms[n_keysyms - 1] == kNoSymbol))
    n_keysyms--;
  if ((n_keysyms > 2) && (modifiers & mode_switch)) {
    syms += 2;
    n_keysyms -= 2;
  }

  if ((modifiers & num_lock) &&
      (n_keysyms > 1 && (IsPublicOrPrivateKeypadKey(syms[1])))) {
    if ((modifiers & kShiftMask) ||
        ((modifiers & kLockMask) && (lock_meaning == XK_Shift_Lock))) {
      return syms[0];
    }
    return syms[1];
  }

  KeySym lower;
  KeySym upper;
  if (!(modifiers & kShiftMask) &&
      (!(modifiers & kLockMask) ||
       (static_cast<x11::KeySym>(lock_meaning) == kNoSymbol))) {
    if ((n_keysyms == 1) || (syms[1] == kNoSymbol)) {
      ConvertCase(syms[0], &lower, &upper);
      return lower;
    }
    return syms[0];
  }

  if (!(modifiers & kLockMask) || (lock_meaning != XK_Caps_Lock)) {
    if ((n_keysyms == 1) || ((upper = syms[1]) == kNoSymbol))
      ConvertCase(syms[0], &lower, &upper);
    return upper;
  }

  KeySym sym;
  if ((n_keysyms == 1) || ((sym = syms[1]) == kNoSymbol))
    sym = syms[0];
  ConvertCase(sym, &lower, &upper);
  if (!(modifiers & kShiftMask) && (sym != syms[0]) &&
      ((sym != upper) || (lower == upper)))
    ConvertCase(syms[0], &lower, &upper);
  return upper;
}
