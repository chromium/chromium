// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has no header guard because it is explicitly intended
// to be included more than once with different definitions of the
// macros USB_KEYMAP and USB_KEYMAP_DECLARATION.

// Data in this file was created by referencing:
//  [0] USB HID Usage Tables,
//      http://www.usb.org/developers/hidpage/Hut1_12v2.pdf
//  [1] DOM Level 3 KeyboardEvent code Values,
//      http://www.w3.org/TR/uievents-code/
//  [2] OS X <HIToolbox/Events.h>
//  [3] Linux <linux/input.h> and hid-input.c
//  [4] USB HID to PS/2 Scan Code Translation Table
//      http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/translate.pdf
//  [5] Keyboard Scan Code Specification
//      http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/scancode.doc

// General notes:
//
//  This table provides the definition of ui::DomCode (UI Events |code|) values
//  as well as mapping between scan codes and DomCode. Some entries have no
//  defined scan codes; these are present only to allow those UI Events |code|
//  strings to be represented by DomCode. A few have a null code; these define
//  mappings with a DomCode:: value but no |code| string, typically because
//  they end up used in shortcuts but not standardized in UI Events; e.g.
//  DomCode::BRIGHTNESS_UP. Commented-out entries document USB codes that are
//  potentially interesting but not currently used.

// Linux notes:
//
//  All USB codes that are listed here and that are supported by the kernel
//  (as of 4.2) have their evdev/xkb scan codes recorded; if an evdev/xkb
//  code is 0, it is because the kernel USB driver does not handle that key.
//
//  Some Linux kernel mappings for USB keys may seem counterintuitive:
//
//  [L1] Although evdev 0x163 KEY_CLEAR exists, Linux does not use it
//       for any USB keys. Linux maps USB 0x07009c [Keyboard Clear] and
//       0x0700d8 [Keypad Clear] to KEY_DELETE "Delete", so those codes are
//       not distinguishable by applications, and UI Events "NumpadClear"
//       is therefore not supported. USB 0x0700A2 [Keyboard Clear/Again]
//       is not mapped by the kernel at all.
//
//  [L2] 'Menu' and 'Props' naming differs between evdev and USB / UI Events.
//        USB 0x010085 [System Main Menu] and USB 0x0C0040 [Menu Mode] both
//        map to evdev 0x8B KEY_MENU (which has no corresponding UI Events
//        |code|). USB 0x070076 [Keyboard Menu] does not map to KEY_MENU;
//        it maps to evdev 0x82 KEY_PROPS, which is not the same as USB and
//        UI Events "Props". USB 0x0700A3 [Props], which does correspond to
//        UI Events "Props", is not mapped by the kernel. (And all of these
//        are distinct from UI Events' "ContextMenu", which corresponds to
//        USB 0x070065 [Keyboard Application] via evdev 0x7F KEY_COMPOSE,
//        following Windows convention.)
//
//  [L3]  Linux flattens both USB 0x070048 [Keyboard Pause] and 0x0C00B1
//        [Media Pause] to 0x77 KEY_PAUSE. We map the former, since [1]
//        defines a 'Pause' code but no 'MediaPause' code.

// Windows notes:
//
//  The set of scan codes supported here may not be complete.
//
//  [W1] Windows maps both USB 0x070094 [Lang5] and USB 0x070073 [F24] to the
//       same scan code, 0x76. (Microsoft's defined scan codes for F13 - F24
//       appear to be the result of accidentally mapping an IBM Set 3 terminal
//       keyboard, rather than an IBM Set 2 PC keyboard, through the BIOS
//       2-to-1 table.)  We map 0x76 to F24 here, since Lang5 appears unused
//       in practice (its declared function, Zenkaku/Hankaku switch, is
//       conventionally placed on Backquote by Japanese keyboards).

// Macintosh notes:
//
//  The set of scan codes supported here may not be complete.
//
//  [M1] OS X maps USB 0x070049 [Insert] as well as USB 0x070075 [Help] to
//       scan code 0x72 kVK_Help. We map this to UI Events 'Insert', since
//       Apple keyboards with USB 0x070049 [Insert] labelled "Help" have not
//       been made since 2007.

USB_KEYMAP_DECLARATION {

  //            USB     evdev    XKB     Win     Mac   Code
  USB_KEYMAP(0x000000, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NONE), // Invalid

#if defined(OS_ANDROID)
#define USB_KEYMAP_ANDROID
#include "ui/events/keycodes/dom/keycode_conversion_data_android_generated.inc"
#undef USB_KEYMAP_ANDROID
#endif

  // =========================================
  // Non-USB codes
  // =========================================

  //            USB     evdev    XKB     Win     Mac   Code
  USB_KEYMAP(0x000010, 0x0000, 0x0000, 0x0000, 0xffff, "Hyper", HYPER),
  USB_KEYMAP(0x000011, 0x0000, 0x0000, 0x0000, 0xffff, "Super", SUPER),
  USB_KEYMAP(0x000012, 0x0000, 0x0000, 0x0000, 0xffff, "Fn", FN),
  USB_KEYMAP(0x000013, 0x0000, 0x0000, 0x0000, 0xffff, "FnLock", FN_LOCK),
  USB_KEYMAP(0x000014, 0x0000, 0x0000, 0x0000, 0xffff, "Suspend", SUSPEND),
  USB_KEYMAP(0x000015, 0x0000, 0x0000, 0x0000, 0xffff, "Resume", RESUME),
  USB_KEYMAP(0x000016, 0x0000, 0x0000, 0x0000, 0xffff, "Turbo", TURBO),

  // =========================================
  // USB Usage Page 0x01: Generic Desktop Page
  // =========================================

  // Sleep could be encoded as USB#0c0032, but there's no corresponding WakeUp
  // in the 0x0c USB page.
  //            USB     evdev    XKB     Win     Mac
  USB_KEYMAP(0x010082, 0x008e, 0x0096, 0xe05f, 0xffff, "Sleep", SLEEP), // SystemSleep
  USB_KEYMAP(0x010083, 0x008f, 0x0097, 0xe063, 0xffff, "WakeUp", WAKE_UP),
  USB_KEYMAP(0x0100b5, 0x00e3, 0x00eb, 0x0000, 0xffff, "DisplayToggleIntExt",
             DISPLAY_TOGGLE_INT_EXT),  // System Display Toggle Int/Ext


  // =========================================
  // USB Usage Page 0x07: Keyboard/Keypad Page
  // =========================================

  // TODO(garykac):
  // XKB#005c ISO Level3 Shift (AltGr)
  // XKB#005e <>||
  // XKB#006d Linefeed
  // XKB#008a SunProps cf. USB#0700a3 CrSel/Props
  // XKB#008e SunOpen
  // Mac#003f kVK_Function
  // Mac#000a kVK_ISO_Section (ISO keyboards only)
  // Mac#0066 kVK_JIS_Eisu (USB#07008a Henkan?)

  //            USB     evdev    XKB     Win     Mac
  USB_KEYMAP(0x070000, 0x0000, 0x0000, 0x0000, 0xffff, NULL, USB_RESERVED),
  USB_KEYMAP(0x070001, 0x0000, 0x0000, 0x00ff, 0xffff, NULL, USB_ERROR_ROLL_OVER),
  USB_KEYMAP(0x070002, 0x0000, 0x0000, 0x00fc, 0xffff, NULL, USB_POST_FAIL),
  USB_KEYMAP(0x070003, 0x0000, 0x0000, 0x0000, 0xffff, NULL, USB_ERROR_UNDEFINED),
  USB_KEYMAP(0x070004, 0x001e, 0x0026, 0x001e, 0x0000, "KeyA", US_A), // aA
  USB_KEYMAP(0x070005, 0x0030, 0x0038, 0x0030, 0x000b, "KeyB", US_B), // bB
  USB_KEYMAP(0x070006, 0x002e, 0x0036, 0x002e, 0x0008, "KeyC", US_C), // cC
  USB_KEYMAP(0x070007, 0x0020, 0x0028, 0x0020, 0x0002, "KeyD", US_D), // dD

  USB_KEYMAP(0x070008, 0x0012, 0x001a, 0x0012, 0x000e, "KeyE", US_E), // eE
  USB_KEYMAP(0x070009, 0x0021, 0x0029, 0x0021, 0x0003, "KeyF", US_F), // fF
  USB_KEYMAP(0x07000a, 0x0022, 0x002a, 0x0022, 0x0005, "KeyG", US_G), // gG
  USB_KEYMAP(0x07000b, 0x0023, 0x002b, 0x0023, 0x0004, "KeyH", US_H), // hH
  USB_KEYMAP(0x07000c, 0x0017, 0x001f, 0x0017, 0x0022, "KeyI", US_I), // iI
  USB_KEYMAP(0x07000d, 0x0024, 0x002c, 0x0024, 0x0026, "KeyJ", US_J), // jJ
  USB_KEYMAP(0x07000e, 0x0025, 0x002d, 0x0025, 0x0028, "KeyK", US_K), // kK
  USB_KEYMAP(0x07000f, 0x0026, 0x002e, 0x0026, 0x0025, "KeyL", US_L), // lL

  USB_KEYMAP(0x070010, 0x0032, 0x003a, 0x0032, 0x002e, "KeyM", US_M), // mM
  USB_KEYMAP(0x070011, 0x0031, 0x0039, 0x0031, 0x002d, "KeyN", US_N), // nN
  USB_KEYMAP(0x070012, 0x0018, 0x0020, 0x0018, 0x001f, "KeyO", US_O), // oO
  USB_KEYMAP(0x070013, 0x0019, 0x0021, 0x0019, 0x0023, "KeyP", US_P), // pP
  USB_KEYMAP(0x070014, 0x0010, 0x0018, 0x0010, 0x000c, "KeyQ", US_Q), // qQ
  USB_KEYMAP(0x070015, 0x0013, 0x001b, 0x0013, 0x000f, "KeyR", US_R), // rR
  USB_KEYMAP(0x070016, 0x001f, 0x0027, 0x001f, 0x0001, "KeyS", US_S), // sS
  USB_KEYMAP(0x070017, 0x0014, 0x001c, 0x0014, 0x0011, "KeyT", US_T), // tT

  USB_KEYMAP(0x070018, 0x0016, 0x001e, 0x0016, 0x0020, "KeyU", US_U), // uU
  USB_KEYMAP(0x070019, 0x002f, 0x0037, 0x002f, 0x0009, "KeyV", US_V), // vV
  USB_KEYMAP(0x07001a, 0x0011, 0x0019, 0x0011, 0x000d, "KeyW", US_W), // wW
  USB_KEYMAP(0x07001b, 0x002d, 0x0035, 0x002d, 0x0007, "KeyX", US_X), // xX
  USB_KEYMAP(0x07001c, 0x0015, 0x001d, 0x0015, 0x0010, "KeyY", US_Y), // yY
  USB_KEYMAP(0x07001d, 0x002c, 0x0034, 0x002c, 0x0006, "KeyZ", US_Z), // zZ
  USB_KEYMAP(0x07001e, 0x0002, 0x000a, 0x0002, 0x0012, "Digit1", DIGIT1), // 1!
  USB_KEYMAP(0x07001f, 0x0003, 0x000b, 0x0003, 0x0013, "Digit2", DIGIT2), // 2@

  USB_KEYMAP(0x070020, 0x0004, 0x000c, 0x0004, 0x0014, "Digit3", DIGIT3), // 3#
  USB_KEYMAP(0x070021, 0x0005, 0x000d, 0x0005, 0x0015, "Digit4", DIGIT4), // 4$
  USB_KEYMAP(0x070022, 0x0006, 0x000e, 0x0006, 0x0017, "Digit5", DIGIT5), // 5%
  USB_KEYMAP(0x070023, 0x0007, 0x000f, 0x0007, 0x0016, "Digit6", DIGIT6), // 6^
  USB_KEYMAP(0x070024, 0x0008, 0x0010, 0x0008, 0x001a, "Digit7", DIGIT7), // 7&
  USB_KEYMAP(0x070025, 0x0009, 0x0011, 0x0009, 0x001c, "Digit8", DIGIT8), // 8*
  USB_KEYMAP(0x070026, 0x000a, 0x0012, 0x000a, 0x0019, "Digit9", DIGIT9), // 9(
  USB_KEYMAP(0x070027, 0x000b, 0x0013, 0x000b, 0x001d, "Digit0", DIGIT0), // 0)

  USB_KEYMAP(0x070028, 0x001c, 0x0024, 0x001c, 0x0024, "Enter", ENTER),
  USB_KEYMAP(0x070029, 0x0001, 0x0009, 0x0001, 0x0035, "Escape", ESCAPE),
  USB_KEYMAP(0x07002a, 0x000e, 0x0016, 0x000e, 0x0033, "Backspace", BACKSPACE),
  USB_KEYMAP(0x07002b, 0x000f, 0x0017, 0x000f, 0x0030, "Tab", TAB),
  USB_KEYMAP(0x07002c, 0x0039, 0x0041, 0x0039, 0x0031, "Space", SPACE), // Spacebar
  USB_KEYMAP(0x07002d, 0x000c, 0x0014, 0x000c, 0x001b, "Minus", MINUS), // -_
  USB_KEYMAP(0x07002e, 0x000d, 0x0015, 0x000d, 0x0018, "Equal", EQUAL), // =+
  USB_KEYMAP(0x07002f, 0x001a, 0x0022, 0x001a, 0x0021, "BracketLeft", BRACKET_LEFT),

  USB_KEYMAP(0x070030, 0x001b, 0x0023, 0x001b, 0x001e, "BracketRight", BRACKET_RIGHT),
  USB_KEYMAP(0x070031, 0x002b, 0x0033, 0x002b, 0x002a, "Backslash", BACKSLASH), // \|
  // USB#070032 never appears on keyboards that have USB#070031.
  // Platforms use the same scancode as for the two keys.
  // Hence this code can only be generated synthetically
  // (e.g. in a DOM Level 3 KeyboardEvent).
  // The keycap varies on international keyboards:
  //   Dan: '*  Dutch: <>  Ger: #'  UK: #~
  // TODO(garykac): Verify Mac intl keyboard.
  USB_KEYMAP(0x070032, 0x0000, 0x0000, 0x0000, 0xffff, "IntlHash", INTL_HASH),
  USB_KEYMAP(0x070033, 0x0027, 0x002f, 0x0027, 0x0029, "Semicolon", SEMICOLON), // ;:
  USB_KEYMAP(0x070034, 0x0028, 0x0030, 0x0028, 0x0027, "Quote", QUOTE), // '"
  USB_KEYMAP(0x070035, 0x0029, 0x0031, 0x0029, 0x0032, "Backquote", BACKQUOTE), // `~
  USB_KEYMAP(0x070036, 0x0033, 0x003b, 0x0033, 0x002b, "Comma", COMMA), // ,<
  USB_KEYMAP(0x070037, 0x0034, 0x003c, 0x0034, 0x002f, "Period", PERIOD), // .>

  USB_KEYMAP(0x070038, 0x0035, 0x003d, 0x0035, 0x002c, "Slash", SLASH), // /?
  // TODO(garykac): CapsLock requires special handling for each platform.
  USB_KEYMAP(0x070039, 0x003a, 0x0042, 0x003a, 0x0039, "CapsLock", CAPS_LOCK),
  USB_KEYMAP(0x07003a, 0x003b, 0x0043, 0x003b, 0x007a, "F1", F1),
  USB_KEYMAP(0x07003b, 0x003c, 0x0044, 0x003c, 0x0078, "F2", F2),
  USB_KEYMAP(0x07003c, 0x003d, 0x0045, 0x003d, 0x0063, "F3", F3),
  USB_KEYMAP(0x07003d, 0x003e, 0x0046, 0x003e, 0x0076, "F4", F4),
  USB_KEYMAP(0x07003e, 0x003f, 0x0047, 0x003f, 0x0060, "F5", F5),
  USB_KEYMAP(0x07003f, 0x0040, 0x0048, 0x0040, 0x0061, "F6", F6),

  USB_KEYMAP(0x070040, 0x0041, 0x0049, 0x0041, 0x0062, "F7", F7),
  USB_KEYMAP(0x070041, 0x0042, 0x004a, 0x0042, 0x0064, "F8", F8),
  USB_KEYMAP(0x070042, 0x0043, 0x004b, 0x0043, 0x0065, "F9", F9),
  USB_KEYMAP(0x070043, 0x0044, 0x004c, 0x0044, 0x006d, "F10", F10),
  USB_KEYMAP(0x070044, 0x0057, 0x005f, 0x0057, 0x0067, "F11", F11),
  USB_KEYMAP(0x070045, 0x0058, 0x0060, 0x0058, 0x006f, "F12", F12),
  // PrintScreen is effectively F13 on Mac OS X.
  USB_KEYMAP(0x070046, 0x0063, 0x006b, 0xe037, 0xffff, "PrintScreen", PRINT_SCREEN),
  USB_KEYMAP(0x070047, 0x0046, 0x004e, 0x0046, 0xffff, "ScrollLock", SCROLL_LOCK),

  USB_KEYMAP(0x070048, 0x0077, 0x007f, 0x0045, 0xffff, "Pause", PAUSE),
  // USB#0x070049 Insert, labeled "Help/Insert" on Mac -- see note M1 at top.
  USB_KEYMAP(0x070049, 0x006e, 0x0076, 0xe052, 0x0072, "Insert", INSERT),
  USB_KEYMAP(0x07004a, 0x0066, 0x006e, 0xe047, 0x0073, "Home", HOME),
  USB_KEYMAP(0x07004b, 0x0068, 0x0070, 0xe049, 0x0074, "PageUp", PAGE_UP),
  // Delete (Forward Delete) named DEL because DELETE conflicts with <windows.h>
  USB_KEYMAP(0x07004c, 0x006f, 0x0077, 0xe053, 0x0075, "Delete", DEL),
  USB_KEYMAP(0x07004d, 0x006b, 0x0073, 0xe04f, 0x0077, "End", END),
  USB_KEYMAP(0x07004e, 0x006d, 0x0075, 0xe051, 0x0079, "PageDown", PAGE_DOWN),
  USB_KEYMAP(0x07004f, 0x006a, 0x0072, 0xe04d, 0x007c, "ArrowRight", ARROW_RIGHT),

  USB_KEYMAP(0x070050, 0x0069, 0x0071, 0xe04b, 0x007b, "ArrowLeft", ARROW_LEFT),
  USB_KEYMAP(0x070051, 0x006c, 0x0074, 0xe050, 0x007d, "ArrowDown", ARROW_DOWN),
  USB_KEYMAP(0x070052, 0x0067, 0x006f, 0xe048, 0x007e, "ArrowUp", ARROW_UP),
  USB_KEYMAP(0x070053, 0x0045, 0x004d, 0xe045, 0x0047, "NumLock", NUM_LOCK),
  USB_KEYMAP(0x070054, 0x0062, 0x006a, 0xe035, 0x004b, "NumpadDivide", NUMPAD_DIVIDE),
  USB_KEYMAP(0x070055, 0x0037, 0x003f, 0x0037, 0x0043, "NumpadMultiply",
             NUMPAD_MULTIPLY),  // Keypad_*
  USB_KEYMAP(0x070056, 0x004a, 0x0052, 0x004a, 0x004e, "NumpadSubtract",
             NUMPAD_SUBTRACT),  // Keypad_-
  USB_KEYMAP(0x070057, 0x004e, 0x0056, 0x004e, 0x0045, "NumpadAdd", NUMPAD_ADD),

  USB_KEYMAP(0x070058, 0x0060, 0x0068, 0xe01c, 0x004c, "NumpadEnter", NUMPAD_ENTER),
  USB_KEYMAP(0x070059, 0x004f, 0x0057, 0x004f, 0x0053, "Numpad1", NUMPAD1), // +End
  USB_KEYMAP(0x07005a, 0x0050, 0x0058, 0x0050, 0x0054, "Numpad2", NUMPAD2), // +Down
  USB_KEYMAP(0x07005b, 0x0051, 0x0059, 0x0051, 0x0055, "Numpad3", NUMPAD3), // +PageDn
  USB_KEYMAP(0x07005c, 0x004b, 0x0053, 0x004b, 0x0056, "Numpad4", NUMPAD4), // +Left
  USB_KEYMAP(0x07005d, 0x004c, 0x0054, 0x004c, 0x0057, "Numpad5", NUMPAD5), //
  USB_KEYMAP(0x07005e, 0x004d, 0x0055, 0x004d, 0x0058, "Numpad6", NUMPAD6), // +Right
  USB_KEYMAP(0x07005f, 0x0047, 0x004f, 0x0047, 0x0059, "Numpad7", NUMPAD7), // +Home

  USB_KEYMAP(0x070060, 0x0048, 0x0050, 0x0048, 0x005b, "Numpad8", NUMPAD8), // +Up
  USB_KEYMAP(0x070061, 0x0049, 0x0051, 0x0049, 0x005c, "Numpad9", NUMPAD9), // +PageUp
  USB_KEYMAP(0x070062, 0x0052, 0x005a, 0x0052, 0x0052, "Numpad0", NUMPAD0), // +Insert
  USB_KEYMAP(0x070063, 0x0053, 0x005b, 0x0053, 0x0041, "NumpadDecimal",
             NUMPAD_DECIMAL),  // Keypad_. Delete
  // USB#070064 is not present on US keyboard.
  // This key is typically located near LeftShift key.
  // The keycap varies on international keyboards:
  //   Dan: <> Dutch: ][ Ger: <> UK: \|
  USB_KEYMAP(0x070064, 0x0056, 0x005e, 0x0056, 0x000a, "IntlBackslash", INTL_BACKSLASH),
  // USB#0x070065 Application Menu (next to RWin key) -- see note L2 at top.
  USB_KEYMAP(0x070065, 0x007f, 0x0087, 0xe05d, 0x006e, "ContextMenu", CONTEXT_MENU),
  USB_KEYMAP(0x070066, 0x0074, 0x007c, 0xe05e, 0xffff, "Power", POWER),
  USB_KEYMAP(0x070067, 0x0075, 0x007d, 0x0059, 0x0051, "NumpadEqual", NUMPAD_EQUAL),

  USB_KEYMAP(0x070068, 0x00b7, 0x00bf, 0x0064, 0x0069, "F13", F13),
  USB_KEYMAP(0x070069, 0x00b8, 0x00c0, 0x0065, 0x006b, "F14", F14),
  USB_KEYMAP(0x07006a, 0x00b9, 0x00c1, 0x0066, 0x0071, "F15", F15),
  USB_KEYMAP(0x07006b, 0x00ba, 0x00c2, 0x0067, 0x006a, "F16", F16),
  USB_KEYMAP(0x07006c, 0x00bb, 0x00c3, 0x0068, 0x0040, "F17", F17),
  USB_KEYMAP(0x07006d, 0x00bc, 0x00c4, 0x0069, 0x004f, "F18", F18),
  USB_KEYMAP(0x07006e, 0x00bd, 0x00c5, 0x006a, 0x0050, "F19", F19),
  USB_KEYMAP(0x07006f, 0x00be, 0x00c6, 0x006b, 0x005a, "F20", F20),

  USB_KEYMAP(0x070070, 0x00bf, 0x00c7, 0x006c, 0xffff, "F21", F21),
  USB_KEYMAP(0x070071, 0x00c0, 0x00c8, 0x006d, 0xffff, "F22", F22),
  USB_KEYMAP(0x070072, 0x00c1, 0x00c9, 0x006e, 0xffff, "F23", F23),
  // USB#0x070073 -- see note W1 at top.
  USB_KEYMAP(0x070073, 0x00c2, 0x00ca, 0x0076, 0xffff, "F24", F24),
  USB_KEYMAP(0x070074, 0x0086, 0x008e, 0x0000, 0xffff, "Open", OPEN), // Execute
  // USB#0x070075 Help -- see note M1 at top.
  USB_KEYMAP(0x070075, 0x008a, 0x0092, 0xe03b, 0xffff, "Help", HELP),
  // USB#0x070076 Keyboard Menu -- see note L2 at top.
  //USB_KEYMAP(0x070076, 0x0000, 0x0000, 0x0000, 0xffff, NULL, MENU), // Menu
  USB_KEYMAP(0x070077, 0x0084, 0x008c, 0x0000, 0xffff, "Select", SELECT), // Select

  //USB_KEYMAP(0x070078, 0x0080, 0x0088, 0x0000, 0xffff, NULL, STOP), // Stop
  USB_KEYMAP(0x070079, 0x0081, 0x0089, 0x0000, 0xffff, "Again", AGAIN), // Again
  USB_KEYMAP(0x07007a, 0x0083, 0x008b, 0xe008, 0xffff, "Undo", UNDO),
  USB_KEYMAP(0x07007b, 0x0089, 0x0091, 0xe017, 0xffff, "Cut", CUT),
  USB_KEYMAP(0x07007c, 0x0085, 0x008d, 0xe018, 0xffff, "Copy", COPY),
  USB_KEYMAP(0x07007d, 0x0087, 0x008f, 0xe00a, 0xffff, "Paste", PASTE),
  USB_KEYMAP(0x07007e, 0x0088, 0x0090, 0x0000, 0xffff, "Find", FIND), // Find
  USB_KEYMAP(0x07007f, 0x0071, 0x0079, 0xe020, 0x004a, "AudioVolumeMute", VOLUME_MUTE),

  USB_KEYMAP(0x070080, 0x0073, 0x007b, 0xe030, 0x0048, "AudioVolumeUp", VOLUME_UP),
  USB_KEYMAP(0x070081, 0x0072, 0x007a, 0xe02e, 0x0049, "AudioVolumeDown", VOLUME_DOWN),
  //USB_KEYMAP(0x070082, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LOCKING_CAPS_LOCK),
  //USB_KEYMAP(0x070083, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LOCKING_NUM_LOCK),
  //USB_KEYMAP(0x070084, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LOCKING_SCROLL_LOCK),
  USB_KEYMAP(0x070085, 0x0079, 0x0081, 0x007e, 0x005f, "NumpadComma", NUMPAD_COMMA),

  // International1
  // USB#070086 is used on AS/400 keyboards. Standard Keypad_= is USB#070067.
  //USB_KEYMAP(0x070086, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_EQUAL),
  // USB#070087 is used for Brazilian /? and Japanese _ 'ro'.
  USB_KEYMAP(0x070087, 0x0059, 0x0061, 0x0073, 0x005e, "IntlRo", INTL_RO),
  // International2
  // USB#070088 is used as Japanese Hiragana/Katakana key.
  USB_KEYMAP(0x070088, 0x005d, 0x0065, 0x0070, 0x0068, "KanaMode", KANA_MODE),
  // International3
  // USB#070089 is used as Japanese Yen key.
  USB_KEYMAP(0x070089, 0x007c, 0x0084, 0x007d, 0x005d, "IntlYen", INTL_YEN),
  // International4
  // USB#07008a is used as Japanese Henkan (Convert) key.
  USB_KEYMAP(0x07008a, 0x005c, 0x0064, 0x0079, 0xffff, "Convert", CONVERT),
  // International5
  // USB#07008b is used as Japanese Muhenkan (No-convert) key.
  USB_KEYMAP(0x07008b, 0x005e, 0x0066, 0x007b, 0xffff, "NonConvert", NON_CONVERT),
  //USB_KEYMAP(0x07008c, 0x005f, 0x0067, 0x005c, 0xffff, NULL, INTERNATIONAL6),
  //USB_KEYMAP(0x07008d, 0x0000, 0x0000, 0x0000, 0xffff, NULL, INTERNATIONAL7),
  //USB_KEYMAP(0x07008e, 0x0000, 0x0000, 0x0000, 0xffff, NULL, INTERNATIONAL8),
  //USB_KEYMAP(0x07008f, 0x0000, 0x0000, 0x0000, 0xffff, NULL, INTERNATIONAL9),

  // LANG1
  // USB#070090 is used as Korean Hangul/English toggle key.
  USB_KEYMAP(0x070090, 0x007a, 0x0082, 0x0072, 0xffff, "Lang1", LANG1),
  // LANG2
  // USB#070091 is used as Korean Hanja conversion key.
  USB_KEYMAP(0x070091, 0x007b, 0x0083, 0x0071, 0xffff, "Lang2", LANG2),
  // LANG3
  // USB#070092 is used as Japanese Katakana key.
  USB_KEYMAP(0x070092, 0x005a, 0x0062, 0x0078, 0xffff, "Lang3", LANG3),
  // LANG4
  // USB#070093 is used as Japanese Hiragana key.
  USB_KEYMAP(0x070093, 0x005b, 0x0063, 0x0077, 0xffff, "Lang4", LANG4),
  // LANG5
  // USB#070094 is used as Japanese Zenkaku/Hankaku (Fullwidth/halfwidth) key.
  // Not mapped on Windows -- see note W1 at top.
  USB_KEYMAP(0x070094, 0x0055, 0x005d, 0x0000, 0xffff, "Lang5", LANG5),
  //USB_KEYMAP(0x070095, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LANG6), // LANG6
  //USB_KEYMAP(0x070096, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LANG7), // LANG7
  //USB_KEYMAP(0x070097, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LANG8), // LANG8
  //USB_KEYMAP(0x070098, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LANG9), // LANG9

  //USB_KEYMAP(0x070099, 0x0000, 0x0000, 0x0000, 0xffff, NULL, ALTERNATE_ERASE),
  //USB_KEYMAP(0x07009a, 0x0000, 0x0000, 0x0000, 0xffff, NULL, SYS_REQ), // /Attention
  USB_KEYMAP(0x07009b, 0x0000, 0x0000, 0x0000, 0xffff, "Abort", ABORT), // Cancel
  // USB#0x07009c Keyboard Clear -- see note L1 at top.
  //USB_KEYMAP(0x07009c, 0x0000, 0x0000, 0x0000, 0xffff, NULL, CLEAR), // Clear
  //USB_KEYMAP(0x07009d, 0x0000, 0x0000, 0x0000, 0xffff, NULL, PRIOR), // Prior
  //USB_KEYMAP(0x07009e, 0x0000, 0x0000, 0x0000, 0xffff, NULL, RETURN), // Return
  //USB_KEYMAP(0x07009f, 0x0000, 0x0000, 0x0000, 0xffff, NULL, SEPARATOR), // Separator

  //USB_KEYMAP(0x0700a0, 0x0000, 0x0000, 0x0000, 0xffff, NULL, OUT), // Out
  //USB_KEYMAP(0x0700a1, 0x0000, 0x0000, 0x0000, 0xffff, NULL, OPER), // Oper
  //USB_KEYMAP(0x0700a2, 0x0000, 0x0000, 0x0000, 0xffff, NULL, CLEAR_AGAIN),
  // USB#0x0700a3 Props -- see note L2 at top.
  USB_KEYMAP(0x0700a3, 0x0000, 0x0000, 0x0000, 0xffff, "Props", PROPS), // CrSel/Props
  //USB_KEYMAP(0x0700a4, 0x0000, 0x0000, 0x0000, 0xffff, NULL, EX_SEL), // ExSel

  //USB_KEYMAP(0x0700b0, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_00),
  //USB_KEYMAP(0x0700b1, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_000),
  //USB_KEYMAP(0x0700b2, 0x0000, 0x0000, 0x0000, 0xffff, NULL, THOUSANDS_SEPARATOR),
  //USB_KEYMAP(0x0700b3, 0x0000, 0x0000, 0x0000, 0xffff, NULL, DECIMAL_SEPARATOR),
  //USB_KEYMAP(0x0700b4, 0x0000, 0x0000, 0x0000, 0xffff, NULL, CURRENCY_UNIT),
  //USB_KEYMAP(0x0700b5, 0x0000, 0x0000, 0x0000, 0xffff, NULL, CURRENCY_SUBUNIT),
  USB_KEYMAP(0x0700b6, 0x00b3, 0x00bb, 0x0000, 0xffff, "NumpadParenLeft",
             NUMPAD_PAREN_LEFT),   // Keypad_(
  USB_KEYMAP(0x0700b7, 0x00b4, 0x00bc, 0x0000, 0xffff, "NumpadParenRight",
             NUMPAD_PAREN_RIGHT),  // Keypad_)

  //USB_KEYMAP(0x0700b8, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_BRACE_LEFT),
  //USB_KEYMAP(0x0700b9, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_BRACE_RIGHT),
  //USB_KEYMAP(0x0700ba, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_TAB),
  USB_KEYMAP(0x0700bb, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadBackspace",
             NUMPAD_BACKSPACE),  // Keypad_Backspace
  //USB_KEYMAP(0x0700bc, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_A),
  //USB_KEYMAP(0x0700bd, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_B),
  //USB_KEYMAP(0x0700be, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_C),
  //USB_KEYMAP(0x0700bf, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_D),

  //USB_KEYMAP(0x0700c0, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_E),
  //USB_KEYMAP(0x0700c1, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_F),
  //USB_KEYMAP(0x0700c2, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_XOR),
  //USB_KEYMAP(0x0700c3, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_CARAT),
  //USB_KEYMAP(0x0700c4, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_PERCENT),
  //USB_KEYMAP(0x0700c5, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_LESS_THAN),
  //USB_KEYMAP(0x0700c6, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_GREATER_THAN),
  //USB_KEYMAP(0x0700c7, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_AMERSAND),

  //USB_KEYMAP(0x0700c8, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_DOUBLE_AMPERSAND),
  //USB_KEYMAP(0x0700c9, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_VERTICAL_BAR),
  //USB_KEYMAP(0x0700ca, 0x0000, 0x0000, 0x0000, 0xffff, NULL,
  //           NUMPAD_DOUBLE_VERTICAL_BAR),  // Keypad_||
  //USB_KEYMAP(0x0700cb, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_COLON),
  //USB_KEYMAP(0x0700cc, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_NUMBER),
  //USB_KEYMAP(0x0700cd, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_SPACE),
  //USB_KEYMAP(0x0700ce, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_AT),
  //USB_KEYMAP(0x0700cf, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_EXCLAMATION),

  USB_KEYMAP(0x0700d0, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadMemoryStore",
             NUMPAD_MEMORY_STORE),  // Keypad_MemoryStore
  USB_KEYMAP(0x0700d1, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadMemoryRecall",
             NUMPAD_MEMORY_RECALL),  // Keypad_MemoryRecall
  USB_KEYMAP(0x0700d2, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadMemoryClear",
             NUMPAD_MEMORY_CLEAR),  // Keypad_MemoryClear
  USB_KEYMAP(0x0700d3, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadMemoryAdd",
             NUMPAD_MEMORY_ADD),  // Keypad_MemoryAdd
  USB_KEYMAP(0x0700d4, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadMemorySubtract",
             NUMPAD_MEMORY_SUBTRACT),  // Keypad_MemorySubtract
  //USB_KEYMAP(0x0700d5, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_MEMORY_MULTIPLE),
  //USB_KEYMAP(0x0700d6, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_MEMORY_DIVIDE),
  USB_KEYMAP(0x0700d7, 0x0076, 0x007e, 0x0000, 0xffff, NULL, NUMPAD_SIGN_CHANGE), // +/-
  // USB#0x0700d8 Keypad Clear -- see note L1 at top.
  USB_KEYMAP(0x0700d8, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadClear", NUMPAD_CLEAR),
  USB_KEYMAP(0x0700d9, 0x0000, 0x0000, 0x0000, 0xffff, "NumpadClearEntry",
             NUMPAD_CLEAR_ENTRY),  // Keypad_ClearEntry
  //USB_KEYMAP(0x0700da, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_BINARY),
  //USB_KEYMAP(0x0700db, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_OCTAL),
  //USB_KEYMAP(0x0700dc, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_DECIMAL),
  //USB_KEYMAP(0x0700dd, 0x0000, 0x0000, 0x0000, 0xffff, NULL, NUMPAD_HEXADECIMAL),

  // USB#0700de - #0700df are reserved.
  USB_KEYMAP(0x0700e0, 0x001d, 0x0025, 0x001d, 0x003b, "ControlLeft", CONTROL_LEFT),
  USB_KEYMAP(0x0700e1, 0x002a, 0x0032, 0x002a, 0x0038, "ShiftLeft", SHIFT_LEFT),
  // USB#0700e2: left Alt key (Mac left Option key).
  USB_KEYMAP(0x0700e2, 0x0038, 0x0040, 0x0038, 0x003a, "AltLeft", ALT_LEFT),
  // USB#0700e3: left GUI key, e.g. Windows, Mac Command, ChromeOS Search.
  USB_KEYMAP(0x0700e3, 0x007d, 0x0085, 0xe05b, 0x0037, "MetaLeft", META_LEFT),
  USB_KEYMAP(0x0700e4, 0x0061, 0x0069, 0xe01d, 0x003e, "ControlRight", CONTROL_RIGHT),
  USB_KEYMAP(0x0700e5, 0x0036, 0x003e, 0x0036, 0x003c, "ShiftRight", SHIFT_RIGHT),
  // USB#0700e6: right Alt key (Mac right Option key).
  USB_KEYMAP(0x0700e6, 0x0064, 0x006c, 0xe038, 0x003d, "AltRight", ALT_RIGHT),
  // USB#0700e7: right GUI key, e.g. Windows, Mac Command, ChromeOS Search.
  USB_KEYMAP(0x0700e7, 0x007e, 0x0086, 0xe05c, 0x0036, "MetaRight", META_RIGHT),

  // USB#0700e8 - #07ffff are reserved

  // ==================================
  // USB Usage Page 0x0c: Consumer Page
  // ==================================
  // AL = Application Launch
  // AC = Application Control

  // TODO(garykac): Many XF86 keys have multiple scancodes mapping to them.
  // We need to map all of these into a canonical USB scancode without
  // confusing the reverse-lookup - most likely by simply returning the first
  // found match.

  // TODO(garykac): Find appropriate mappings for:
  // Win#e03c Music - USB#0c0193 is AL_AVCapturePlayback
  // Win#e064 Pictures
  // XKB#0080 XF86LaunchA
  // XKB#0099 XF86Send
  // XKB#009b XF86Xfer
  // XKB#009c XF86Launch1
  // XKB#009d XF86Launch2
  // XKB... remaining XF86 keys

  // KEY_BRIGHTNESS* added in Linux 3.16
  // http://www.usb.org/developers/hidpage/HUTRR41.pdf
  //            USB     evdev    XKB     Win     Mac   Code
  USB_KEYMAP(0x0c0060, 0x0166, 0x016e, 0x0000, 0xffff, NULL, INFO),
  USB_KEYMAP(0x0c0061, 0x0172, 0x017a, 0x0000, 0xffff, NULL, CLOSED_CAPTION_TOGGLE),
  USB_KEYMAP(0x0c006f, 0x00e1, 0x00e9, 0x0000, 0xffff, "BrightnessUp", BRIGHTNESS_UP),
  USB_KEYMAP(0x0c0070, 0x00e0, 0x00e8, 0x0000, 0xffff, "BrightnessDown",
             BRIGHTNESS_DOWN),  // Display Brightness Decrement
  USB_KEYMAP(0x0c0072, 0x01af, 0x01b7, 0x0000, 0xffff, NULL, BRIGHTNESS_TOGGLE),
  USB_KEYMAP(0x0c0073, 0x0250, 0x0258, 0x0000, 0xffff, NULL, BRIGHTNESS_MINIMIUM),
  USB_KEYMAP(0x0c0074, 0x0251, 0x0259, 0x0000, 0xffff, NULL, BRIGHTNESS_MAXIMUM),
  USB_KEYMAP(0x0c0075, 0x00f4, 0x00fc, 0x0000, 0xffff, NULL, BRIGHTNESS_AUTO),
  USB_KEYMAP(0x0c0083, 0x0195, 0x019d, 0x0000, 0xffff, NULL, MEDIA_LAST),
  USB_KEYMAP(0x0c008c, 0x00a9, 0x00b1, 0x0000, 0xffff, NULL, LAUNCH_PHONE),
  USB_KEYMAP(0x0c008d, 0x016a, 0x0172, 0x0000, 0xffff, NULL, PROGRAM_GUIDE),
  USB_KEYMAP(0x0c0094, 0x00ae, 0x00b6, 0x0000, 0xffff, NULL, EXIT),
  USB_KEYMAP(0x0c009c, 0x019a, 0x01a2, 0x0000, 0xffff, NULL, CHANNEL_UP),
  USB_KEYMAP(0x0c009d, 0x019b, 0x01a3, 0x0000, 0xffff, NULL, CHANNEL_DOWN),

  //              USB     evdev    XKB     Win     Mac
  USB_KEYMAP(0x0c00b0, 0x00cf, 0x00d7, 0x0000, 0xffff, "MediaPlay", MEDIA_PLAY),
  //USB_KEYMAP(0x0c00b1, 0x0077, 0x007f, 0x0000, 0xffff, "MediaPause", MEDIA_PAUSE),
  USB_KEYMAP(0x0c00b2, 0x00a7, 0x00af, 0x0000, 0xffff, "MediaRecord", MEDIA_RECORD),
  USB_KEYMAP(0x0c00b3, 0x00d0, 0x00d8, 0x0000, 0xffff, "MediaFastForward", MEDIA_FAST_FORWARD),
  USB_KEYMAP(0x0c00b4, 0x00a8, 0x00b0, 0x0000, 0xffff, "MediaRewind", MEDIA_REWIND),
  USB_KEYMAP(0x0c00b5, 0x00a3, 0x00ab, 0xe019, 0xffff, "MediaTrackNext",
             MEDIA_TRACK_NEXT),
  USB_KEYMAP(0x0c00b6, 0x00a5, 0x00ad, 0xe010, 0xffff, "MediaTrackPrevious",
             MEDIA_TRACK_PREVIOUS),
  USB_KEYMAP(0x0c00b7, 0x00a6, 0x00ae, 0xe024, 0xffff, "MediaStop", MEDIA_STOP),
  USB_KEYMAP(0x0c00b8, 0x00a1, 0x00a9, 0xe02c, 0xffff, "Eject", EJECT),
  USB_KEYMAP(0x0c00cd, 0x00a4, 0x00ac, 0xe022, 0xffff, "MediaPlayPause",
             MEDIA_PLAY_PAUSE),
  USB_KEYMAP(0x0c00cf, 0x0246, 0x024e, 0x0000, 0xffff, NULL, SPEECH_INPUT_TOGGLE),
  USB_KEYMAP(0x0c00e5, 0x00d1, 0x00d9, 0x0000, 0xffff, NULL, BASS_BOOST),
  //USB_KEYMAP(0x0c00e6, 0x0000, 0x0000, 0x0000, 0xffff, NULL, SURROUND_MODE),
  //USB_KEYMAP(0x0c0150, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_BALANCE_RIGHT),
  //USB_KEYMAP(0x0c0151, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_BALANCE_LEFT ),
  //USB_KEYMAP(0x0c0152, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_BASS_INCREMENT),
  //USB_KEYMAP(0x0c0153, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_BASS_DECREMENT),
  //USB_KEYMAP(0x0c0154, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_TREBLE_INCREMENT),
  //USB_KEYMAP(0x0c0155, 0x0000, 0x0000, 0x0000, 0xffff, NULL, AUDIO_TREBLE_DECREMENT),
  // USB#0c0183: AL Consumer Control Configuration
  USB_KEYMAP(0x0c0183, 0x00ab, 0x00b3, 0xe06d, 0xffff, "MediaSelect", MEDIA_SELECT),
  USB_KEYMAP(0x0c0184, 0x01a5, 0x01ad, 0x0000, 0xffff, NULL, LAUNCH_WORD_PROCESSOR),
  USB_KEYMAP(0x0c0186, 0x01a7, 0x01af, 0x0000, 0xffff, NULL, LAUNCH_SPREADSHEET),
  // USB#0x0c018a AL_EmailReader
  USB_KEYMAP(0x0c018a, 0x009b, 0x00a3, 0xe06c, 0xffff, "LaunchMail", LAUNCH_MAIL),
  // USB#0x0c018d: AL Contacts/Address Book
  USB_KEYMAP(0x0c018d, 0x01ad, 0x01b5, 0x0000, 0xffff, NULL, LAUNCH_CONTACTS),
  // USB#0x0c018e: AL Calendar/Schedule
  USB_KEYMAP(0x0c018e, 0x018d, 0x0195, 0x0000, 0xffff, NULL, LAUNCH_CALENDAR),
  // USB#0x0c018f AL Task/Project Manager
  //USB_KEYMAP(0x0c018f, 0x0241, 0x0249, 0x0000, 0xffff, NULL, LAUNCH_TASK_MANAGER),
  // USB#0x0c0190: AL Log/Journal/Timecard
  //USB_KEYMAP(0x0c0190, 0x0242, 0x024a, 0x0000, 0xffff, NULL, LAUNCH_LOG),
  // USB#0x0c0192: AL_Calculator
  USB_KEYMAP(0x0c0192, 0x008c, 0x0094, 0xe021, 0xffff, "LaunchApp2", LAUNCH_APP2),
  // USB#0c0194: My Computer (AL_LocalMachineBrowser)
  USB_KEYMAP(0x0c0194, 0x0090, 0x0098, 0xe06b, 0xffff, "LaunchApp1", LAUNCH_APP1),
  USB_KEYMAP(0x0c0196, 0x0096, 0x009e, 0x0000, 0xffff, NULL, LAUNCH_INTERNET_BROWSER),
  USB_KEYMAP(0x0c019C, 0x01b1, 0x01b9, 0x0000, 0xffff, NULL, LOG_OFF),
  // USB#0x0c019e: AL Terminal Lock/Screensaver
  USB_KEYMAP(0x0c019e, 0x0098, 0x00a0, 0x0000, 0xffff, NULL, LOCK_SCREEN),
  // USB#0x0c019f AL Control Panel
  USB_KEYMAP(0x0c019f, 0x0243, 0x024b, 0x0000, 0xffff, "LaunchControlPanel",
             LAUNCH_CONTROL_PANEL),
  // USB#0x0c01a2: AL Select Task/Application
  USB_KEYMAP(0x0c01a2, 0x0244, 0x024c, 0x0000, 0xffff, "SelectTask", SELECT_TASK),
  // USB#0x0c01a7: AL_Documents
  USB_KEYMAP(0x0c01a7, 0x00eb, 0x00f3, 0x0000, 0xffff, NULL, LAUNCH_DOCUMENTS),
  USB_KEYMAP(0x0c01ab, 0x01b0, 0x01b8, 0x0000, 0xffff, NULL, SPELL_CHECK),
  // USB#0x0c01ae: AL Keyboard Layout
  USB_KEYMAP(0x0c01ae, 0x0176, 0x017e, 0x0000, 0xffff, NULL, LAUNCH_KEYBOARD_LAYOUT),
  USB_KEYMAP(0x0c01b1, 0x0245, 0x024d, 0x0000, 0xffff, "LaunchScreenSaver",
             LAUNCH_SCREEN_SAVER),  // AL Screen Saver
  USB_KEYMAP(0x0c01cb, 0x0247, 0x024f, 0x0000, 0xffff, "LaunchAssistant",
             LAUNCH_ASSISTANT),  // AL Context-aware desktop assistant
   // USB#0c01b4: Home Directory (AL_FileBrowser) (Explorer)
  //USB_KEYMAP(0x0c01b4, 0x0000, 0x0000, 0x0000, 0xffff, NULL, LAUNCH_FILE_BROWSER),
  // USB#0x0c01b7: AL Audio Browser
  USB_KEYMAP(0x0c01b7, 0x0188, 0x0190, 0x0000, 0xffff, NULL, LAUNCH_AUDIO_BROWSER),
  // USB#0x0c0201: AC New
  USB_KEYMAP(0x0c0201, 0x00b5, 0x00bd, 0x0000, 0xffff, NULL, NEW),
  // USB#0x0c0203: AC Close
  USB_KEYMAP(0x0c0203, 0x00ce, 0x00d6, 0x0000, 0xffff, NULL, CLOSE),
  // USB#0x0c0207: AC Close
  USB_KEYMAP(0x0c0207, 0x00ea, 0x00f2, 0x0000, 0xffff, NULL, SAVE),
  // USB#0x0c0208: AC Print
  USB_KEYMAP(0x0c0208, 0x00d2, 0x00da, 0x0000, 0xffff, NULL, PRINT),
  // USB#0x0c0221:  AC_Search
  USB_KEYMAP(0x0c0221, 0x00d9, 0x00e1, 0xe065, 0xffff, "BrowserSearch", BROWSER_SEARCH),
  // USB#0x0c0223:  AC_Home
  USB_KEYMAP(0x0c0223, 0x00ac, 0x00b4, 0xe032, 0xffff, "BrowserHome", BROWSER_HOME),
  // USB#0x0c0224:  AC_Back
  USB_KEYMAP(0x0c0224, 0x009e, 0x00a6, 0xe06a, 0xffff, "BrowserBack", BROWSER_BACK),
  // USB#0x0c0225:  AC_Forward
  USB_KEYMAP(0x0c0225, 0x009f, 0x00a7, 0xe069, 0xffff, "BrowserForward",
             BROWSER_FORWARD),
  // USB#0x0c0226:  AC_Stop
  USB_KEYMAP(0x0c0226, 0x0080, 0x0088, 0xe068, 0xffff, "BrowserStop", BROWSER_STOP),
  // USB#0x0c0227:  AC_Refresh (Reload)
  USB_KEYMAP(0x0c0227, 0x00ad, 0x00b5, 0xe067, 0xffff, "BrowserRefresh",
             BROWSER_REFRESH),
  // USB#0x0c022a:  AC_Bookmarks (Favorites)
  USB_KEYMAP(0x0c022a, 0x009c, 0x00a4, 0xe066, 0xffff, "BrowserFavorites",
             BROWSER_FAVORITES),
  USB_KEYMAP(0x0c022d, 0x01a2, 0x01aa, 0x0000, 0xffff, NULL, ZOOM_IN),
  USB_KEYMAP(0x0c022e, 0x01a3, 0x01ab, 0x0000, 0xffff, NULL, ZOOM_OUT),
  // USB#0x0c0230:  AC Full Screen View
  //USB_KEYMAP(0x0c0230, 0x0000, 0x0000, 0x0000, 0xffff, NULL, ZOOM_FULL),
  // USB#0x0c0231:  AC Normal View
  //USB_KEYMAP(0x0c0231, 0x0000, 0x0000, 0x0000, 0xffff, NULL, ZOOM_NORMAL),
  USB_KEYMAP(0x0c0232, 0x0174, 0x017c, 0x0000, 0xffff, "ZoomToggle",
             ZOOM_TOGGLE),  // AC View Toggle
  // USB#0x0c0279:  AC Redo/Repeat
  USB_KEYMAP(0x0c0279, 0x00b6, 0x00be, 0x0000, 0xffff, NULL, REDO),
  // USB#0x0c0289:  AC_Reply
  USB_KEYMAP(0x0c0289, 0x00e8, 0x00f0, 0x0000, 0xffff, "MailReply", MAIL_REPLY),
  // USB#0x0c028b:  AC_ForwardMsg (MailForward)
  USB_KEYMAP(0x0c028b, 0x00e9, 0x00f1, 0x0000, 0xffff, "MailForward", MAIL_FORWARD),
  // USB#0x0c028c:  AC_Send
  USB_KEYMAP(0x0c028c, 0x00e7, 0x00ef, 0x0000, 0xffff, "MailSend", MAIL_SEND),
  // USB#0x0c029d:  AC Next Keyboard Layout Select
  USB_KEYMAP(0x0c029d, 0x0248, 0x0250, 0x0000, 0xffff, "KeyboardLayoutSelect",
             KEYBOARD_LAYOUT_SELECT),
  USB_KEYMAP(0x0c029f, 0x0078, 0x0080, 0x0000, 0xffff, "ShowAllWindows",
             SHOW_ALL_WINDOWS),  // AC Desktop Show All Windows
};
