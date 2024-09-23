// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_EVENTS_KEYCODES_DOM_US_LAYOUT_DATA_H_
#define UI_EVENTS_KEYCODES_DOM_US_LAYOUT_DATA_H_

#include <array>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace ui {

// This table maps a DomCode to a printable character, assuming US layout.
// It is used by DomCodeToUsLayoutDomKey(), which provides a fallback
// interpretation when there is no other way to map a physical key.
const struct PrintableCodeEntry {
  DomCode dom_code;
  std::array<char16_t, 2> character;  // normal, shifted
} kPrintableCodeMap[] = {
    {DomCode::US_A, {'a', 'A'}},
    {DomCode::US_B, {'b', 'B'}},
    {DomCode::US_C, {'c', 'C'}},
    {DomCode::US_D, {'d', 'D'}},
    {DomCode::US_E, {'e', 'E'}},
    {DomCode::US_F, {'f', 'F'}},
    {DomCode::US_G, {'g', 'G'}},
    {DomCode::US_H, {'h', 'H'}},
    {DomCode::US_I, {'i', 'I'}},
    {DomCode::US_J, {'j', 'J'}},
    {DomCode::US_K, {'k', 'K'}},
    {DomCode::US_L, {'l', 'L'}},
    {DomCode::US_M, {'m', 'M'}},
    {DomCode::US_N, {'n', 'N'}},
    {DomCode::US_O, {'o', 'O'}},
    {DomCode::US_P, {'p', 'P'}},
    {DomCode::US_Q, {'q', 'Q'}},
    {DomCode::US_R, {'r', 'R'}},
    {DomCode::US_S, {'s', 'S'}},
    {DomCode::US_T, {'t', 'T'}},
    {DomCode::US_U, {'u', 'U'}},
    {DomCode::US_V, {'v', 'V'}},
    {DomCode::US_W, {'w', 'W'}},
    {DomCode::US_X, {'x', 'X'}},
    {DomCode::US_Y, {'y', 'Y'}},
    {DomCode::US_Z, {'z', 'Z'}},
    {DomCode::DIGIT1, {'1', '!'}},
    {DomCode::DIGIT2, {'2', '@'}},
    {DomCode::DIGIT3, {'3', '#'}},
    {DomCode::DIGIT4, {'4', '$'}},
    {DomCode::DIGIT5, {'5', '%'}},
    {DomCode::DIGIT6, {'6', '^'}},
    {DomCode::DIGIT7, {'7', '&'}},
    {DomCode::DIGIT8, {'8', '*'}},
    {DomCode::DIGIT9, {'9', '('}},
    {DomCode::DIGIT0, {'0', ')'}},
    {DomCode::SPACE, {' ', ' '}},
    {DomCode::MINUS, {'-', '_'}},
    {DomCode::EQUAL, {'=', '+'}},
    {DomCode::BRACKET_LEFT, {'[', '{'}},
    {DomCode::BRACKET_RIGHT, {']', '}'}},
    {DomCode::BACKSLASH, {'\\', '|'}},
    {DomCode::SEMICOLON, {';', ':'}},
    {DomCode::QUOTE, {'\'', '"'}},
    {DomCode::BACKQUOTE, {'`', '~'}},
    {DomCode::COMMA, {',', '<'}},
    {DomCode::PERIOD, {'.', '>'}},
    {DomCode::SLASH, {'/', '?'}},
    {DomCode::INTL_BACKSLASH, {'<', '>'}},
    {DomCode::INTL_YEN, {0x00A5, '|'}},
    {DomCode::NUMPAD_DIVIDE, {'/', '/'}},
    {DomCode::NUMPAD_MULTIPLY, {'*', '*'}},
    {DomCode::NUMPAD_SUBTRACT, {'-', '-'}},
    {DomCode::NUMPAD_ADD, {'+', '+'}},
    {DomCode::NUMPAD1, {'1', '1'}},
    {DomCode::NUMPAD2, {'2', '2'}},
    {DomCode::NUMPAD3, {'3', '3'}},
    {DomCode::NUMPAD4, {'4', '4'}},
    {DomCode::NUMPAD5, {'5', '5'}},
    {DomCode::NUMPAD6, {'6', '6'}},
    {DomCode::NUMPAD7, {'7', '7'}},
    {DomCode::NUMPAD8, {'8', '8'}},
    {DomCode::NUMPAD9, {'9', '9'}},
    {DomCode::NUMPAD0, {'0', '0'}},
    {DomCode::NUMPAD_DECIMAL, {'.', '.'}},
    {DomCode::NUMPAD_EQUAL, {'=', '='}},
    {DomCode::NUMPAD_COMMA, {',', ','}},
    {DomCode::NUMPAD_PAREN_LEFT, {'(', '('}},
    {DomCode::NUMPAD_PAREN_RIGHT, {')', ')'}},
    {DomCode::NUMPAD_SIGN_CHANGE, {0x00B1, 0x00B1}},
};

// This table maps a DomCode to a DomKey, assuming US keyboard layout.
// It is used by DomCodeToUsLayoutDomKey(), which provides a fallback
// interpretation when there is no other way to map a physical key.
const struct NonPrintableCodeEntry {
  DomCode dom_code;
  DomKey::Base dom_key;
} kNonPrintableCodeMap[] = {
    {DomCode::ABORT, DomKey::CANCEL},
    {DomCode::AGAIN, DomKey::AGAIN},
    {DomCode::ALT_LEFT, DomKey::ALT},
    {DomCode::ALT_RIGHT, DomKey::ALT},
    {DomCode::ARROW_DOWN, DomKey::ARROW_DOWN},
    {DomCode::ARROW_LEFT, DomKey::ARROW_LEFT},
    {DomCode::ARROW_RIGHT, DomKey::ARROW_RIGHT},
    {DomCode::ARROW_UP, DomKey::ARROW_UP},
    {DomCode::BACKSPACE, DomKey::BACKSPACE},
    {DomCode::BASS_BOOST, DomKey::AUDIO_BASS_BOOST_TOGGLE},
    {DomCode::BRIGHTNESS_DOWN, DomKey::BRIGHTNESS_DOWN},
    {DomCode::BRIGHTNESS_UP, DomKey::BRIGHTNESS_UP},
    // {DomCode::BRIGHTNESS_AUTO, DomKey::_}
    // {DomCode::BRIGHTNESS_MAXIMUM, DomKey::_}
    // {DomCode::BRIGHTNESS_MINIMIUM, DomKey::_}
    // {DomCode::BRIGHTNESS_TOGGLE, DomKey::_}
    {DomCode::BROWSER_BACK, DomKey::BROWSER_BACK},
    {DomCode::BROWSER_FAVORITES, DomKey::BROWSER_FAVORITES},
    {DomCode::BROWSER_FORWARD, DomKey::BROWSER_FORWARD},
    {DomCode::BROWSER_HOME, DomKey::BROWSER_HOME},
    {DomCode::BROWSER_REFRESH, DomKey::BROWSER_REFRESH},
    {DomCode::BROWSER_SEARCH, DomKey::BROWSER_SEARCH},
    {DomCode::BROWSER_STOP, DomKey::BROWSER_STOP},
    {DomCode::CAPS_LOCK, DomKey::CAPS_LOCK},
    {DomCode::CHANNEL_DOWN, DomKey::CHANNEL_DOWN},
    {DomCode::CHANNEL_UP, DomKey::CHANNEL_UP},
    {DomCode::CLOSE, DomKey::CLOSE},
    {DomCode::CLOSED_CAPTION_TOGGLE, DomKey::CLOSED_CAPTION_TOGGLE},
    {DomCode::CONTEXT_MENU, DomKey::CONTEXT_MENU},
    {DomCode::CONTROL_LEFT, DomKey::CONTROL},
    {DomCode::CONTROL_RIGHT, DomKey::CONTROL},
    {DomCode::CONVERT, DomKey::CONVERT},
    {DomCode::COPY, DomKey::COPY},
    {DomCode::CUT, DomKey::CUT},
    {DomCode::DEL, DomKey::DEL},
    {DomCode::EJECT, DomKey::EJECT},
    {DomCode::END, DomKey::END},
    {DomCode::ENTER, DomKey::ENTER},
    {DomCode::ESCAPE, DomKey::ESCAPE},
    {DomCode::EXIT, DomKey::EXIT},
    {DomCode::F1, DomKey::F1},
    {DomCode::F2, DomKey::F2},
    {DomCode::F3, DomKey::F3},
    {DomCode::F4, DomKey::F4},
    {DomCode::F5, DomKey::F5},
    {DomCode::F6, DomKey::F6},
    {DomCode::F7, DomKey::F7},
    {DomCode::F8, DomKey::F8},
    {DomCode::F9, DomKey::F9},
    {DomCode::F10, DomKey::F10},
    {DomCode::F11, DomKey::F11},
    {DomCode::F12, DomKey::F12},
    {DomCode::F13, DomKey::F13},
    {DomCode::F14, DomKey::F14},
    {DomCode::F15, DomKey::F15},
    {DomCode::F16, DomKey::F16},
    {DomCode::F17, DomKey::F17},
    {DomCode::F18, DomKey::F18},
    {DomCode::F19, DomKey::F19},
    {DomCode::F20, DomKey::F20},
    {DomCode::F21, DomKey::F21},
    {DomCode::F22, DomKey::F22},
    {DomCode::F23, DomKey::F23},
    {DomCode::F24, DomKey::F24},
    {DomCode::FIND, DomKey::FIND},
    {DomCode::FN, DomKey::FN},
    {DomCode::FN_LOCK, DomKey::FN_LOCK},
    {DomCode::HELP, DomKey::HELP},
    {DomCode::HOME, DomKey::HOME},
    {DomCode::HYPER, DomKey::HYPER},
    {DomCode::INFO, DomKey::INFO},
    {DomCode::INSERT, DomKey::INSERT},
    // {DomCode::INTL_RO, DomKey::_}
    {DomCode::KANA_MODE, DomKey::KANA_MODE},
    {DomCode::KEYBOARD_LAYOUT_SELECT, DomKey::MODE_CHANGE},
    {DomCode::LANG1, DomKey::HANGUL_MODE},
    {DomCode::LANG2, DomKey::HANJA_MODE},
    {DomCode::LANG3, DomKey::KATAKANA},
    {DomCode::LANG4, DomKey::HIRAGANA},
    {DomCode::LANG5, DomKey::ZENKAKU_HANKAKU},
    {DomCode::LAUNCH_APP1, DomKey::LAUNCH_MY_COMPUTER},
    {DomCode::LAUNCH_APP2, DomKey::LAUNCH_CALCULATOR},
    {DomCode::LAUNCH_ASSISTANT, DomKey::LAUNCH_ASSISTANT},
    {DomCode::LAUNCH_AUDIO_BROWSER, DomKey::LAUNCH_MUSIC_PLAYER},
    {DomCode::LAUNCH_CALENDAR, DomKey::LAUNCH_CALENDAR},
    {DomCode::LAUNCH_CONTACTS, DomKey::LAUNCH_CONTACTS},
    {DomCode::LAUNCH_CONTROL_PANEL, DomKey::SETTINGS},
    {DomCode::LAUNCH_INTERNET_BROWSER, DomKey::LAUNCH_WEB_BROWSER},
    {DomCode::LAUNCH_MAIL, DomKey::LAUNCH_MAIL},
    {DomCode::LAUNCH_PHONE, DomKey::LAUNCH_PHONE},
    {DomCode::LAUNCH_SCREEN_SAVER, DomKey::LAUNCH_SCREEN_SAVER},
    {DomCode::LAUNCH_SPREADSHEET, DomKey::LAUNCH_SPREADSHEET},
    // {DomCode::LAUNCH_DOCUMENTS, DomKey::_}
    // {DomCode::LAUNCH_FILE_BROWSER, DomKey::_}
    // {DomCode::LAUNCH_KEYBOARD_LAYOUT, DomKey::_}
    {DomCode::LOCK_SCREEN, DomKey::LAUNCH_SCREEN_SAVER},
    {DomCode::LOG_OFF, DomKey::LOG_OFF},
    {DomCode::MAIL_FORWARD, DomKey::MAIL_FORWARD},
    {DomCode::MAIL_REPLY, DomKey::MAIL_REPLY},
    {DomCode::MAIL_SEND, DomKey::MAIL_SEND},
    {DomCode::MEDIA_FAST_FORWARD, DomKey::MEDIA_FAST_FORWARD},
    {DomCode::MEDIA_LAST, DomKey::MEDIA_LAST},
    {DomCode::MEDIA_PAUSE, DomKey::MEDIA_PAUSE},
    {DomCode::MEDIA_PLAY, DomKey::MEDIA_PLAY},
    {DomCode::MEDIA_PLAY_PAUSE, DomKey::MEDIA_PLAY_PAUSE},
    {DomCode::MEDIA_RECORD, DomKey::MEDIA_RECORD},
    {DomCode::MEDIA_REWIND, DomKey::MEDIA_REWIND},
    {DomCode::MEDIA_SELECT, DomKey::LAUNCH_MEDIA_PLAYER},
    {DomCode::MEDIA_STOP, DomKey::MEDIA_STOP},
    {DomCode::MEDIA_TRACK_NEXT, DomKey::MEDIA_TRACK_NEXT},
    {DomCode::MEDIA_TRACK_PREVIOUS, DomKey::MEDIA_TRACK_PREVIOUS},
    // {DomCode::MENU, DomKey::_}
    {DomCode::NEW, DomKey::NEW},
    {DomCode::NON_CONVERT, DomKey::NON_CONVERT},
    {DomCode::NUM_LOCK, DomKey::NUM_LOCK},
    {DomCode::NUMPAD_BACKSPACE, DomKey::BACKSPACE},
    {DomCode::NUMPAD_CLEAR, DomKey::CLEAR},
    {DomCode::NUMPAD_ENTER, DomKey::ENTER},
    // {DomCode::NUMPAD_CLEAR_ENTRY, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_ADD, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_CLEAR, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_RECALL, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_STORE, DomKey::_}
    // {DomCode::NUMPAD_MEMORY_SUBTRACT, DomKey::_}
    {DomCode::OPEN, DomKey::OPEN},
    {DomCode::META_LEFT, DomKey::META},
    {DomCode::META_RIGHT, DomKey::META},
    {DomCode::PAGE_DOWN, DomKey::PAGE_DOWN},
    {DomCode::PAGE_UP, DomKey::PAGE_UP},
    {DomCode::PASTE, DomKey::PASTE},
    {DomCode::PAUSE, DomKey::PAUSE},
    {DomCode::POWER, DomKey::POWER},
    {DomCode::PRINT, DomKey::PRINT},
    {DomCode::PRINT_SCREEN, DomKey::PRINT_SCREEN},
    {DomCode::PROGRAM_GUIDE, DomKey::GUIDE},
    {DomCode::PROPS, DomKey::PROPS},
    {DomCode::REDO, DomKey::REDO},
    {DomCode::SAVE, DomKey::SAVE},
    {DomCode::SCROLL_LOCK, DomKey::SCROLL_LOCK},
    {DomCode::SELECT, DomKey::SELECT},
    {DomCode::SELECT_TASK, DomKey::APP_SWITCH},
    {DomCode::SHIFT_LEFT, DomKey::SHIFT},
    {DomCode::SHIFT_RIGHT, DomKey::SHIFT},
    {DomCode::SPEECH_INPUT_TOGGLE, DomKey::SPEECH_INPUT_TOGGLE},
    {DomCode::SPELL_CHECK, DomKey::SPELL_CHECK},
    {DomCode::SUPER, DomKey::SUPER},
    {DomCode::TAB, DomKey::TAB},
    {DomCode::UNDO, DomKey::UNDO},
    {DomCode::VOLUME_DOWN, DomKey::AUDIO_VOLUME_DOWN},
    {DomCode::VOLUME_MUTE, DomKey::AUDIO_VOLUME_MUTE},
    {DomCode::VOLUME_UP, DomKey::AUDIO_VOLUME_UP},
    {DomCode::WAKE_UP, DomKey::WAKE_UP},
    {DomCode::ZOOM_IN, DomKey::ZOOM_IN},
    {DomCode::ZOOM_OUT, DomKey::ZOOM_OUT},
    {DomCode::ZOOM_TOGGLE, DomKey::ZOOM_TOGGLE},
};

// This table maps a DomKey to a non-located KeyboardCode.
const struct DomKeyToKeyboardCodeEntry {
  DomKey::Base dom_key;
  KeyboardCode key_code;
} kDomKeyToKeyboardCodeMap[] = {
    // No value.
    {DomKey::NONE, VKEY_UNKNOWN},
    // Special Key Values
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-special
    {DomKey::UNIDENTIFIED, VKEY_UNKNOWN},
    // Modifier Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-modifier
    {DomKey::ALT, VKEY_MENU},
    {DomKey::ALT_GRAPH, VKEY_ALTGR},
    {DomKey::CAPS_LOCK, VKEY_CAPITAL},
    {DomKey::CONTROL, VKEY_CONTROL},
    {DomKey::NUM_LOCK, VKEY_NUMLOCK},
    {DomKey::META, VKEY_LWIN},
    {DomKey::SCROLL_LOCK, VKEY_SCROLL},
    {DomKey::SHIFT, VKEY_SHIFT},
    // Whitespace Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-whitespace
    {DomKey::ENTER, VKEY_RETURN},
    {DomKey::TAB, VKEY_TAB},
    // Navigation Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-navigation
    {DomKey::ARROW_DOWN, VKEY_DOWN},
    {DomKey::ARROW_LEFT, VKEY_LEFT},
    {DomKey::ARROW_RIGHT, VKEY_RIGHT},
    {DomKey::ARROW_UP, VKEY_UP},
    {DomKey::END, VKEY_END},
    {DomKey::HOME, VKEY_HOME},
    {DomKey::PAGE_DOWN, VKEY_NEXT},
    {DomKey::PAGE_UP, VKEY_PRIOR},
    // Editing Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-editing
    {DomKey::BACKSPACE, VKEY_BACK},
    {DomKey::CLEAR, VKEY_CLEAR},
    {DomKey::CR_SEL, VKEY_CRSEL},
    {DomKey::DEL, VKEY_DELETE},
    {DomKey::ERASE_EOF, VKEY_EREOF},
    {DomKey::EX_SEL, VKEY_EXSEL},
    {DomKey::INSERT, VKEY_INSERT},
    // UI Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-ui
    {DomKey::ACCEPT, VKEY_ACCEPT},
    {DomKey::ATTN, VKEY_ATTN},
    {DomKey::CONTEXT_MENU, VKEY_APPS},
    {DomKey::ESCAPE, VKEY_ESCAPE},
    {DomKey::EXECUTE, VKEY_EXECUTE},
    {DomKey::HELP, VKEY_HELP},
    {DomKey::PAUSE, VKEY_PAUSE},
    {DomKey::PLAY, VKEY_PLAY},
    {DomKey::SELECT, VKEY_SELECT},
    // Device Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-device
#if BUILDFLAG(IS_POSIX)
    {DomKey::LAUNCH_ASSISTANT, VKEY_ASSISTANT},
    {DomKey::BRIGHTNESS_DOWN, VKEY_BRIGHTNESS_DOWN},
    {DomKey::BRIGHTNESS_UP, VKEY_BRIGHTNESS_UP},
    {DomKey::POWER, VKEY_POWER},
    {DomKey::SETTINGS, VKEY_SETTINGS},
#endif
    {DomKey::PRINT_SCREEN, VKEY_SNAPSHOT},
// IME and Composition Keys
// http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-composition
#if BUILDFLAG(IS_POSIX)
    {DomKey::COMPOSE, VKEY_COMPOSE},
#endif
    {DomKey::CONVERT, VKEY_CONVERT},
    {DomKey::FINAL_MODE, VKEY_FINAL},
    {DomKey::MODE_CHANGE, VKEY_MODECHANGE},
    {DomKey::NON_CONVERT, VKEY_NONCONVERT},
    {DomKey::PROCESS, VKEY_PROCESSKEY},
    // Keys specific to Korean keyboards
    {DomKey::HANGUL_MODE, VKEY_HANGUL},
    {DomKey::HANJA_MODE, VKEY_HANJA},
    {DomKey::JUNJA_MODE, VKEY_JUNJA},
    // Keys specific to Japanese keyboards
    {DomKey::HANKAKU, VKEY_DBE_SBCSCHAR},
    {DomKey::KANA_MODE, VKEY_KANA},
    {DomKey::KANJI_MODE, VKEY_KANJI},
    {DomKey::ZENKAKU, VKEY_DBE_DBCSCHAR},
    {DomKey::ZENKAKU_HANKAKU, VKEY_DBE_DBCSCHAR},
    // General-Purpose Function Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-function
    {DomKey::F1, VKEY_F1},
    {DomKey::F2, VKEY_F2},
    {DomKey::F3, VKEY_F3},
    {DomKey::F4, VKEY_F4},
    {DomKey::F5, VKEY_F5},
    {DomKey::F6, VKEY_F6},
    {DomKey::F7, VKEY_F7},
    {DomKey::F8, VKEY_F8},
    {DomKey::F9, VKEY_F9},
    {DomKey::F10, VKEY_F10},
    {DomKey::F11, VKEY_F11},
    {DomKey::F12, VKEY_F12},
    {DomKey::F13, VKEY_F13},
    {DomKey::F14, VKEY_F14},
    {DomKey::F15, VKEY_F15},
    {DomKey::F16, VKEY_F16},
    {DomKey::F17, VKEY_F17},
    {DomKey::F18, VKEY_F18},
    {DomKey::F19, VKEY_F19},
    {DomKey::F20, VKEY_F20},
    {DomKey::F21, VKEY_F21},
    {DomKey::F22, VKEY_F22},
    {DomKey::F23, VKEY_F23},
    {DomKey::F24, VKEY_F24},
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {DomKey::FN, VKEY_FUNCTION},
#endif
    // Multimedia Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-multimedia
    {DomKey::MEDIA_PLAY_PAUSE, VKEY_MEDIA_PLAY_PAUSE},
    {DomKey::MEDIA_STOP, VKEY_MEDIA_STOP},
    {DomKey::MEDIA_TRACK_NEXT, VKEY_MEDIA_NEXT_TRACK},
    {DomKey::MEDIA_TRACK_PREVIOUS, VKEY_MEDIA_PREV_TRACK},
    {DomKey::PRINT, VKEY_PRINT},
    {DomKey::AUDIO_VOLUME_DOWN, VKEY_VOLUME_DOWN},
    {DomKey::AUDIO_VOLUME_MUTE, VKEY_VOLUME_MUTE},
    {DomKey::AUDIO_VOLUME_UP, VKEY_VOLUME_UP},
    // Application Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-apps
    {DomKey::LAUNCH_CALCULATOR, VKEY_MEDIA_LAUNCH_APP2},
    {DomKey::LAUNCH_MAIL, VKEY_MEDIA_LAUNCH_MAIL},
    {DomKey::LAUNCH_MEDIA_PLAYER, VKEY_MEDIA_LAUNCH_MEDIA_SELECT},
    {DomKey::LAUNCH_MY_COMPUTER, VKEY_MEDIA_LAUNCH_APP1},
    // Browser Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-browser
    {DomKey::BROWSER_BACK, VKEY_BROWSER_BACK},
    {DomKey::BROWSER_FAVORITES, VKEY_BROWSER_FAVORITES},
    {DomKey::BROWSER_FORWARD, VKEY_BROWSER_FORWARD},
    {DomKey::BROWSER_HOME, VKEY_BROWSER_HOME},
    {DomKey::BROWSER_REFRESH, VKEY_BROWSER_REFRESH},
    {DomKey::BROWSER_SEARCH, VKEY_BROWSER_SEARCH},
    {DomKey::BROWSER_STOP, VKEY_BROWSER_STOP},
    // Media Controller Keys
    // http://www.w3.org/TR/DOM-Level-3-Events-key/#keys-media-controller
#if BUILDFLAG(IS_POSIX)
    {DomKey::MEDIA_FAST_FORWARD, VKEY_OEM_104},
    {DomKey::MEDIA_PAUSE, VKEY_MEDIA_PAUSE},
    {DomKey::MEDIA_PLAY, VKEY_MEDIA_PLAY},
    {DomKey::MEDIA_REWIND, VKEY_OEM_103},
#endif
    {DomKey::ZOOM_TOGGLE, VKEY_ZOOM},
};

// This table, used by DomCodeToUsLayoutKeyboardCode() and
// UsLayoutKeyboardCodeToDomCode(), maps between DOM Level 3 .code values
// and legacy Windows-based VKEY values, where the VKEYs are interpreted
// positionally (located) following a base US English layout.
const struct DomCodeToKeyboardCodeEntry {
  DomCode dom_code;
  KeyboardCode key_code;
} kDomCodeToKeyboardCodeMap[] = {
    // Entries are ordered by numeric value of the DomCode enum,
    // which is the USB physical key code.
    // DomCode::HYPER                              0x000010 Hyper
    // DomCode::SUPER                              0x000011 Super
    // DomCode::FN_LOCK                            0x000013 FLock
    // DomCode::SUSPEND                            0x000014 Suspend
    // DomCode::RESUME                             0x000015 Resume
    // DomCode::TURBO                              0x000016 Turbo
    {DomCode::SLEEP, VKEY_SLEEP},  // 0x010082 Sleep
    // DomCode::WAKE_UP                            0x010083 WakeUp
#if BUILDFLAG(IS_CHROMEOS_ASH)
    {DomCode::FN, VKEY_FUNCTION},  // 0x010097 Fn
#endif
#if BUILDFLAG(IS_POSIX)
    {DomCode::MICROPHONE_MUTE_TOGGLE,
     VKEY_MICROPHONE_MUTE_TOGGLE},  // 0x0100A9 MicrophoneMuteToggle
    {DomCode::ACCESSIBILITY,
     VKEY_ACCESSIBILITY},  // 0x0100AA System Accessibility Binding
#endif
    {DomCode::US_A, VKEY_A},                    // 0x070004 KeyA
    {DomCode::US_B, VKEY_B},                    // 0x070005 KeyB
    {DomCode::US_C, VKEY_C},                    // 0x070006 KeyC
    {DomCode::US_D, VKEY_D},                    // 0x070007 KeyD
    {DomCode::US_E, VKEY_E},                    // 0x070008 KeyE
    {DomCode::US_F, VKEY_F},                    // 0x070009 KeyF
    {DomCode::US_G, VKEY_G},                    // 0x07000A KeyG
    {DomCode::US_H, VKEY_H},                    // 0x07000B KeyH
    {DomCode::US_I, VKEY_I},                    // 0x07000C KeyI
    {DomCode::US_J, VKEY_J},                    // 0x07000D KeyJ
    {DomCode::US_K, VKEY_K},                    // 0x07000E KeyK
    {DomCode::US_L, VKEY_L},                    // 0x07000F KeyL
    {DomCode::US_M, VKEY_M},                    // 0x070010 KeyM
    {DomCode::US_N, VKEY_N},                    // 0x070011 KeyN
    {DomCode::US_O, VKEY_O},                    // 0x070012 KeyO
    {DomCode::US_P, VKEY_P},                    // 0x070013 KeyP
    {DomCode::US_Q, VKEY_Q},                    // 0x070014 KeyQ
    {DomCode::US_R, VKEY_R},                    // 0x070015 KeyR
    {DomCode::US_S, VKEY_S},                    // 0x070016 KeyS
    {DomCode::US_T, VKEY_T},                    // 0x070017 KeyT
    {DomCode::US_U, VKEY_U},                    // 0x070018 KeyU
    {DomCode::US_V, VKEY_V},                    // 0x070019 KeyV
    {DomCode::US_W, VKEY_W},                    // 0x07001A KeyW
    {DomCode::US_X, VKEY_X},                    // 0x07001B KeyX
    {DomCode::US_Y, VKEY_Y},                    // 0x07001C KeyY
    {DomCode::US_Z, VKEY_Z},                    // 0x07001D KeyZ
    {DomCode::DIGIT1, VKEY_1},                  // 0x07001E Digit1
    {DomCode::DIGIT2, VKEY_2},                  // 0x07001F Digit2
    {DomCode::DIGIT3, VKEY_3},                  // 0x070020 Digit3
    {DomCode::DIGIT4, VKEY_4},                  // 0x070021 Digit4
    {DomCode::DIGIT5, VKEY_5},                  // 0x070022 Digit5
    {DomCode::DIGIT6, VKEY_6},                  // 0x070023 Digit6
    {DomCode::DIGIT7, VKEY_7},                  // 0x070024 Digit7
    {DomCode::DIGIT8, VKEY_8},                  // 0x070025 Digit8
    {DomCode::DIGIT9, VKEY_9},                  // 0x070026 Digit9
    {DomCode::DIGIT0, VKEY_0},                  // 0x070027 Digit0
    {DomCode::ENTER, VKEY_RETURN},              // 0x070028 Enter
    {DomCode::ESCAPE, VKEY_ESCAPE},             // 0x070029 Escape
    {DomCode::BACKSPACE, VKEY_BACK},            // 0x07002A Backspace
    {DomCode::TAB, VKEY_TAB},                   // 0x07002B Tab
    {DomCode::SPACE, VKEY_SPACE},               // 0x07002C Space
    {DomCode::MINUS, VKEY_OEM_MINUS},           // 0x07002D Minus
    {DomCode::EQUAL, VKEY_OEM_PLUS},            // 0x07002E Equal
    {DomCode::BRACKET_LEFT, VKEY_OEM_4},        // 0x07002F BracketLeft
    {DomCode::BRACKET_RIGHT, VKEY_OEM_6},       // 0x070030 BracketRight
    {DomCode::BACKSLASH, VKEY_OEM_5},           // 0x070031 Backslash
    {DomCode::SEMICOLON, VKEY_OEM_1},           // 0x070033 Semicolon
    {DomCode::QUOTE, VKEY_OEM_7},               // 0x070034 Quote
    {DomCode::BACKQUOTE, VKEY_OEM_3},           // 0x070035 Backquote
    {DomCode::COMMA, VKEY_OEM_COMMA},           // 0x070036 Comma
    {DomCode::PERIOD, VKEY_OEM_PERIOD},         // 0x070037 Period
    {DomCode::SLASH, VKEY_OEM_2},               // 0x070038 Slash
    {DomCode::CAPS_LOCK, VKEY_CAPITAL},         // 0x070039 CapsLock
    {DomCode::F1, VKEY_F1},                     // 0x07003A F1
    {DomCode::F2, VKEY_F2},                     // 0x07003B F2
    {DomCode::F3, VKEY_F3},                     // 0x07003C F3
    {DomCode::F4, VKEY_F4},                     // 0x07003D F4
    {DomCode::F5, VKEY_F5},                     // 0x07003E F5
    {DomCode::F6, VKEY_F6},                     // 0x07003F F6
    {DomCode::F7, VKEY_F7},                     // 0x070040 F7
    {DomCode::F8, VKEY_F8},                     // 0x070041 F8
    {DomCode::F9, VKEY_F9},                     // 0x070042 F9
    {DomCode::F10, VKEY_F10},                   // 0x070043 F10
    {DomCode::F11, VKEY_F11},                   // 0x070044 F11
    {DomCode::F12, VKEY_F12},                   // 0x070045 F12
    {DomCode::PRINT_SCREEN, VKEY_SNAPSHOT},     // 0x070046 PrintScreen
    {DomCode::SCROLL_LOCK, VKEY_SCROLL},        // 0x070047 ScrollLock
    {DomCode::PAUSE, VKEY_PAUSE},               // 0x070048 Pause
    {DomCode::INSERT, VKEY_INSERT},             // 0x070049 Insert
    {DomCode::HOME, VKEY_HOME},                 // 0x07004A Home
    {DomCode::PAGE_UP, VKEY_PRIOR},             // 0x07004B PageUp
    {DomCode::DEL, VKEY_DELETE},                // 0x07004C Delete
    {DomCode::END, VKEY_END},                   // 0x07004D End
    {DomCode::PAGE_DOWN, VKEY_NEXT},            // 0x07004E PageDown
    {DomCode::ARROW_RIGHT, VKEY_RIGHT},         // 0x07004F ArrowRight
    {DomCode::ARROW_LEFT, VKEY_LEFT},           // 0x070050 ArrowLeft
    {DomCode::ARROW_DOWN, VKEY_DOWN},           // 0x070051 ArrowDown
    {DomCode::ARROW_UP, VKEY_UP},               // 0x070052 ArrowUp
    {DomCode::NUM_LOCK, VKEY_NUMLOCK},          // 0x070053 NumLock
    {DomCode::NUMPAD_DIVIDE, VKEY_DIVIDE},      // 0x070054 NumpadDivide
    {DomCode::NUMPAD_MULTIPLY, VKEY_MULTIPLY},  // 0x070055 NumpadMultiply
    {DomCode::NUMPAD_SUBTRACT, VKEY_SUBTRACT},  // 0x070056 NumpadSubtract
    {DomCode::NUMPAD_ADD, VKEY_ADD},            // 0x070057 NumpadAdd
    {DomCode::NUMPAD_ENTER, VKEY_RETURN},       // 0x070058 NumpadEnter
    {DomCode::NUMPAD1, VKEY_NUMPAD1},           // 0x070059 Numpad1
    {DomCode::NUMPAD2, VKEY_NUMPAD2},           // 0x07005A Numpad2
    {DomCode::NUMPAD3, VKEY_NUMPAD3},           // 0x07005B Numpad3
    {DomCode::NUMPAD4, VKEY_NUMPAD4},           // 0x07005C Numpad4
    {DomCode::NUMPAD5, VKEY_NUMPAD5},           // 0x07005D Numpad5
    {DomCode::NUMPAD6, VKEY_NUMPAD6},           // 0x07005E Numpad6
    {DomCode::NUMPAD7, VKEY_NUMPAD7},           // 0x07005F Numpad7
    {DomCode::NUMPAD8, VKEY_NUMPAD8},           // 0x070060 Numpad8
    {DomCode::NUMPAD9, VKEY_NUMPAD9},           // 0x070061 Numpad9
    {DomCode::NUMPAD0, VKEY_NUMPAD0},           // 0x070062 Numpad0
    {DomCode::NUMPAD_DECIMAL, VKEY_DECIMAL},    // 0x070063 NumpadDecimal
    {DomCode::INTL_BACKSLASH, VKEY_OEM_102},    // 0x070064 IntlBackslash
    {DomCode::CONTEXT_MENU, VKEY_APPS},         // 0x070065 ContextMenu
#if BUILDFLAG(IS_POSIX)
    {DomCode::POWER, VKEY_POWER},  // 0x070066 Power
#endif
    // DomCode::NUMPAD_EQUAL                       0x070067 NumpadEqual
    {DomCode::F13, VKEY_F13},        // 0x070068 F13
    {DomCode::F14, VKEY_F14},        // 0x070069 F14
    {DomCode::F15, VKEY_F15},        // 0x07006A F15
    {DomCode::F16, VKEY_F16},        // 0x07006B F16
    {DomCode::F17, VKEY_F17},        // 0x07006C F17
    {DomCode::F18, VKEY_F18},        // 0x07006D F18
    {DomCode::F19, VKEY_F19},        // 0x07006E F19
    {DomCode::F20, VKEY_F20},        // 0x07006F F20
    {DomCode::F21, VKEY_F21},        // 0x070070 F21
    {DomCode::F22, VKEY_F22},        // 0x070071 F22
    {DomCode::F23, VKEY_F23},        // 0x070072 F23
    {DomCode::F24, VKEY_F24},        // 0x070073 F24
    {DomCode::OPEN, VKEY_EXECUTE},   // 0x070074 Open
    {DomCode::HELP, VKEY_HELP},      // 0x070075 Help
    {DomCode::SELECT, VKEY_SELECT},  // 0x070077 Select
    // DomCode::AGAIN                              0x070079 Again
    // DomCode::UNDO                               0x07007A Undo
    // DomCode::CUT                                0x07007B Cut
    // DomCode::COPY                               0x07007C Copy
    // DomCode::PASTE                              0x07007D Paste
    // DomCode::FIND                               0x07007E Find
    {DomCode::VOLUME_MUTE, VKEY_VOLUME_MUTE},  // 0x07007F VolumeMute
    {DomCode::VOLUME_UP, VKEY_VOLUME_UP},      // 0x070080 VolumeUp
    {DomCode::VOLUME_DOWN, VKEY_VOLUME_DOWN},  // 0x070081 VolumeDown
    {DomCode::NUMPAD_COMMA, VKEY_OEM_COMMA},   // 0x070085 NumpadComma
    {DomCode::INTL_RO, VKEY_OEM_102},          // 0x070087 IntlRo
    {DomCode::KANA_MODE, VKEY_KANA},           // 0x070088 KanaMode
    {DomCode::INTL_YEN, VKEY_OEM_5},           // 0x070089 IntlYen
    {DomCode::CONVERT, VKEY_CONVERT},          // 0x07008A Convert
    {DomCode::NON_CONVERT, VKEY_NONCONVERT},   // 0x07008B NonConvert
    {DomCode::LANG1, VKEY_KANA},               // 0x070090 Lang1
    {DomCode::LANG2, VKEY_KANJI},              // 0x070091 Lang2
    // DomCode::LANG3                              0x070092 Lang3
    // DomCode::LANG4                              0x070093 Lang4
    // DomCode::LANG5                              0x070094 Lang5
    {DomCode::ABORT, VKEY_CANCEL},  // 0x07009B Abort
    // DomCode::PROPS                              0x0700A3 Props
    // DomCode::NUMPAD_PAREN_LEFT                  0x0700B6 NumpadParenLeft
    // DomCode::NUMPAD_PAREN_RIGHT                 0x0700B7 NumpadParenRight
    {DomCode::NUMPAD_BACKSPACE, VKEY_BACK},  // 0x0700BB NumpadBackspace
    // DomCode::NUMPAD_MEMORY_STORE                0x0700D0 NumpadMemoryStore
    // DomCode::NUMPAD_MEMORY_RECALL               0x0700D1 NumpadMemoryRecall
    // DomCode::NUMPAD_MEMORY_CLEAR                0x0700D2 NumpadMemoryClear
    // DomCode::NUMPAD_MEMORY_ADD                  0x0700D3 NumpadMemoryAdd
    // DomCode::NUMPAD_MEMORY_SUBTRACT             0x0700D4 NumpadMemorySubtract
    {DomCode::NUMPAD_CLEAR, VKEY_CLEAR},        // 0x0700D8 NumpadClear
    {DomCode::NUMPAD_CLEAR_ENTRY, VKEY_CLEAR},  // 0x0700D9 NumpadClearEntry
    {DomCode::CONTROL_LEFT, VKEY_LCONTROL},     // 0x0700E0 ControlLeft
    {DomCode::SHIFT_LEFT, VKEY_LSHIFT},         // 0x0700E1 ShiftLeft
    {DomCode::ALT_LEFT, VKEY_LMENU},            // 0x0700E2 AltLeft
    {DomCode::META_LEFT, VKEY_LWIN},            // 0x0700E3 OSLeft
    {DomCode::CONTROL_RIGHT, VKEY_RCONTROL},    // 0x0700E4 ControlRight
    {DomCode::SHIFT_RIGHT, VKEY_RSHIFT},        // 0x0700E5 ShiftRight
    {DomCode::ALT_RIGHT, VKEY_RMENU},           // 0x0700E6 AltRight
    {DomCode::META_RIGHT, VKEY_RWIN},           // 0x0700E7 OSRight
#if BUILDFLAG(IS_POSIX)
    {DomCode::BRIGHTNESS_UP, VKEY_BRIGHTNESS_UP},  // 0x0C006F BrightnessUp
    {DomCode::BRIGHTNESS_DOWN,
     VKEY_BRIGHTNESS_DOWN},                           // 0x0C0070 BrightnessDown
    {DomCode::KBD_ILLUM_UP, VKEY_KBD_BRIGHTNESS_UP},  // 0x0C0079 KbdIllumUp
    {DomCode::KBD_ILLUM_DOWN,
     VKEY_KBD_BRIGHTNESS_DOWN},  // 0x0C007a KbdIllumDown
    {DomCode::KEYBOARD_BACKLIGHT_TOGGLE,
     VKEY_KBD_BACKLIGHT_TOGGLE},  // 0x0C007C KeyboardBacklightToggle
#endif
    {DomCode::MEDIA_TRACK_NEXT,
     VKEY_MEDIA_NEXT_TRACK},  // 0x0C00B5 MediaTrackNext
    {DomCode::MEDIA_TRACK_PREVIOUS,
     VKEY_MEDIA_PREV_TRACK},                 // 0x0C00B6 MediaTrackPrevious
    {DomCode::MEDIA_STOP, VKEY_MEDIA_STOP},  // 0x0C00B7 MediaStop
    // DomCode::EJECT                              0x0C00B8 Eject
    {DomCode::MEDIA_PLAY_PAUSE,
     VKEY_MEDIA_PLAY_PAUSE},  // 0x0C00CD MediaPlayPause
#if BUILDFLAG(IS_POSIX)
    {DomCode::DICTATE, VKEY_DICTATE},            // 0x0C00D8 Dictate
    {DomCode::EMOJI_PICKER, VKEY_EMOJI_PICKER},  // 0x0C00D9 Emoji
#endif
    {DomCode::MEDIA_SELECT,
     VKEY_MEDIA_LAUNCH_MEDIA_SELECT},                // 0x0C0183 MediaSelect
    {DomCode::LAUNCH_MAIL, VKEY_MEDIA_LAUNCH_MAIL},  // 0x0C018A LaunchMail
    {DomCode::LAUNCH_APP2, VKEY_MEDIA_LAUNCH_APP2},  // 0x0C0192 LaunchApp2
    {DomCode::LAUNCH_APP1, VKEY_MEDIA_LAUNCH_APP1},  // 0x0C0194 LaunchApp1
#if BUILDFLAG(IS_POSIX)
    {DomCode::LAUNCH_CONTROL_PANEL,
     VKEY_SETTINGS},                              // 0x0C019F Launch Assistant
    {DomCode::LAUNCH_ASSISTANT, VKEY_ASSISTANT},  // 0x0C01CB Launch Assistant
    {DomCode::NEW, VKEY_NEW},                     // 0x0C0201 AC New
    {DomCode::CLOSE, VKEY_CLOSE},                 // 0x0C0203 AC Close
#endif
    {DomCode::BROWSER_SEARCH, VKEY_BROWSER_SEARCH},  // 0x0C0221 BrowserSearch
    {DomCode::BROWSER_HOME, VKEY_BROWSER_HOME},      // 0x0C0223 BrowserHome
    {DomCode::BROWSER_BACK, VKEY_BROWSER_BACK},      // 0x0C0224 BrowserBack
    {DomCode::BROWSER_FORWARD,
     VKEY_BROWSER_FORWARD},                      // 0x0C0225 BrowserForward
    {DomCode::BROWSER_STOP, VKEY_BROWSER_STOP},  // 0x0C0226 BrowserStop
    {DomCode::BROWSER_REFRESH,
     VKEY_BROWSER_REFRESH},  // 0x0C0227 BrowserRefresh
    {DomCode::BROWSER_FAVORITES,
     VKEY_BROWSER_FAVORITES},           // 0x0C022A BrowserFavorites
    {DomCode::ZOOM_TOGGLE, VKEY_ZOOM},  // 0x0C0232 ZoomToggle
#if BUILDFLAG(IS_POSIX)
    {DomCode::ALL_APPLICATIONS,
     VKEY_ALL_APPLICATIONS},  // 0x0C02A2 All Applications
    {DomCode::PRIVACY_SCREEN_TOGGLE,
     VKEY_PRIVACY_SCREEN_TOGGLE},  // 0x0C02D0 PrivacyScreenToggle
#endif
};

// This table, used by UsLayoutKeyboardCodeToDomCode(), maps legacy
// Windows-based VKEY values that are not part of kDomCodeToKeyboardCodeMap[]
// to suitable DomCode values, where practical.
const DomCodeToKeyboardCodeEntry kFallbackKeyboardCodeToDomCodeMap[] = {
    {DomCode::ALT_LEFT, VKEY_MENU},
    {DomCode::ALT_RIGHT, VKEY_ALTGR},
#if BUILDFLAG(IS_POSIX)
    {DomCode::CONTEXT_MENU, VKEY_COMPOSE},
#endif
    {DomCode::CONTROL_LEFT, VKEY_CONTROL},
    {DomCode::LANG1, VKEY_HANGUL},
    {DomCode::LANG2, VKEY_HANJA},
    {DomCode::LANG5, VKEY_DBE_DBCSCHAR},
    {DomCode::NUMPAD_CLEAR, VKEY_OEM_CLEAR},
    {DomCode::NUMPAD_DECIMAL, VKEY_SEPARATOR},
    {DomCode::PROPS, VKEY_CRSEL},
    {DomCode::SHIFT_LEFT, VKEY_SHIFT},
    {DomCode::SUPER, VKEY_OEM_8},
    //
    // VKEYs with no directly corresponding DomCode, but a USB usage code:
    //  VKEY_ATTN                 // 0x07009A SysReq
    //  VKEY_SEPARATOR            // 0x07009F Separator
    //  VKEY_EXSEL                // 0x0700A4 ExSel
    //  VKEY_PRINT                // 0x0C0208 AC Print
    //  VKEY_PLAY                 // 0x0C00B0 MediaPlay
    //  VKEY_OEM_103              // 0x0C00B4 MediaRewind
    //  VKEY_OEM_104              // 0x0C00B3 MediaFastForward
    //
    // VKEYs with no corresponding DomCode, but a Linux evdev usage code:
    //  VKEY_WLAN                 //  evdev KEY_WLAN
    //
    // VKEYs with no corresponding DomCode and no obvious USB usage code:
    //  VKEY_ACCEPT
    //  VKEY_BACKTAB
    //  VKEY_DBE_SBCSCHAR
    //  VKEY_EREOF
    //  VKEY_FINAL
    //  VKEY_JUNJA
    //  VKEY_MODECHANGE
    //  VKEY_NONAME
    //  VKEY_PA1
    //  VKEY_PACKET
    //  VKEY_PROCESSKEY
};

}  // namespace ui

#endif  // UI_EVENTS_KEYCODES_DOM_US_LAYOUT_DATA_H_
