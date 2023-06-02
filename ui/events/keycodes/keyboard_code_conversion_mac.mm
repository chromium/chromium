// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "base/mac/foundation_util.h"

#import <Carbon/Carbon.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/mac/foundation_util.h"
#include "base/mac/mac_logging.h"
#include "base/mac/scoped_cftyperef.h"
#include "ui/events/keycodes/dom/keycode_converter.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ui {

namespace {

// Per Apple docs, the buffer length can be up to 255 but is rarely more than 4.
// https://developer.apple.com/documentation/coreservices/1390584-uckeytranslate
constexpr int kUCKeyTranslateBufferLength = 4;

bool IsUnicodeControl(unichar c) {
  // C0 control characters: http://unicode.org/charts/PDF/U0000.pdf
  // C1 control characters: http://unicode.org/charts/PDF/U0080.pdf
  return c <= 0x1F || (c >= 0x7F && c <= 0x9F);
}

// This value is not defined but shows up as 0x36.
const int kVK_RightCommand = 0x36;
// Context menu is not defined but shows up as 0x6E.
const int kVK_ContextMenu = 0x6E;

// A struct to hold a Windows keycode to Mac virtual keycode mapping.
struct KeyCodeMap {
  KeyboardCode keycode;
  int macKeycode;
  unichar characterIgnoringAllModifiers;
};

// Customized less operator for using std::lower_bound() on a KeyCodeMap array.
bool operator<(const KeyCodeMap& a, const KeyCodeMap& b) {
  return a.keycode < b.keycode;
}

// This array must keep sorted ascending according to the value of |keycode|,
// so that we can binary search it.
// TODO(suzhe): This map is not complete, missing entries have macKeycode == -1.
const KeyCodeMap kKeyCodesMap[] = {
  { VKEY_BACK /* 0x08 */, kVK_Delete, kBackspaceCharCode },
  { VKEY_TAB /* 0x09 */, kVK_Tab, kTabCharCode },
  { VKEY_BACKTAB /* 0x0A */, 0x21E4, '\031' },
  { VKEY_CLEAR /* 0x0C */, kVK_ANSI_KeypadClear, kClearCharCode },
  { VKEY_RETURN /* 0x0D */, kVK_Return, kReturnCharCode },
  { VKEY_SHIFT /* 0x10 */, kVK_Shift, 0 },
  { VKEY_CONTROL /* 0x11 */, kVK_Control, 0 },
  { VKEY_MENU /* 0x12 */, kVK_Option, 0 },
  { VKEY_PAUSE /* 0x13 */, -1, NSPauseFunctionKey },
  { VKEY_CAPITAL /* 0x14 */, kVK_CapsLock, 0 },
  { VKEY_KANA /* 0x15 */, kVK_JIS_Kana, 0 },
  { VKEY_HANGUL /* 0x15 */, -1, 0 },
  { VKEY_JUNJA /* 0x17 */, -1, 0 },
  { VKEY_FINAL /* 0x18 */, -1, 0 },
  { VKEY_HANJA /* 0x19 */, -1, 0 },
  { VKEY_KANJI /* 0x19 */, -1, 0 },
  { VKEY_ESCAPE /* 0x1B */, kVK_Escape, kEscapeCharCode },
  { VKEY_CONVERT /* 0x1C */, -1, 0 },
  { VKEY_NONCONVERT /* 0x1D */, -1, 0 },
  { VKEY_ACCEPT /* 0x1E */, -1, 0 },
  { VKEY_MODECHANGE /* 0x1F */, -1, 0 },
  { VKEY_SPACE /* 0x20 */, kVK_Space, kSpaceCharCode },
  { VKEY_PRIOR /* 0x21 */, kVK_PageUp, NSPageUpFunctionKey },
  { VKEY_NEXT /* 0x22 */, kVK_PageDown, NSPageDownFunctionKey },
  { VKEY_END /* 0x23 */, kVK_End, NSEndFunctionKey },
  { VKEY_HOME /* 0x24 */, kVK_Home, NSHomeFunctionKey },
  { VKEY_LEFT /* 0x25 */, kVK_LeftArrow, NSLeftArrowFunctionKey },
  { VKEY_UP /* 0x26 */, kVK_UpArrow, NSUpArrowFunctionKey },
  { VKEY_RIGHT /* 0x27 */, kVK_RightArrow, NSRightArrowFunctionKey },
  { VKEY_DOWN /* 0x28 */, kVK_DownArrow, NSDownArrowFunctionKey },
  { VKEY_SELECT /* 0x29 */, -1, 0 },
  { VKEY_PRINT /* 0x2A */, -1, NSPrintFunctionKey },
  { VKEY_EXECUTE /* 0x2B */, -1, NSExecuteFunctionKey },
  { VKEY_SNAPSHOT /* 0x2C */, -1, NSPrintScreenFunctionKey },
  { VKEY_INSERT /* 0x2D */, kVK_Help, NSInsertFunctionKey },
  { VKEY_DELETE /* 0x2E */, kVK_ForwardDelete, NSDeleteFunctionKey },
  { VKEY_HELP /* 0x2F */, kVK_Help, kHelpCharCode },
  { VKEY_0 /* 0x30 */, kVK_ANSI_0, '0' },
  { VKEY_1 /* 0x31 */, kVK_ANSI_1, '1' },
  { VKEY_2 /* 0x32 */, kVK_ANSI_2, '2' },
  { VKEY_3 /* 0x33 */, kVK_ANSI_3, '3' },
  { VKEY_4 /* 0x34 */, kVK_ANSI_4, '4' },
  { VKEY_5 /* 0x35 */, kVK_ANSI_5, '5' },
  { VKEY_6 /* 0x36 */, kVK_ANSI_6, '6' },
  { VKEY_7 /* 0x37 */, kVK_ANSI_7, '7' },
  { VKEY_8 /* 0x38 */, kVK_ANSI_8, '8' },
  { VKEY_9 /* 0x39 */, kVK_ANSI_9, '9' },
  { VKEY_A /* 0x41 */, kVK_ANSI_A, 'a' },
  { VKEY_B /* 0x42 */, kVK_ANSI_B, 'b' },
  { VKEY_C /* 0x43 */, kVK_ANSI_C, 'c' },
  { VKEY_D /* 0x44 */, kVK_ANSI_D, 'd' },
  { VKEY_E /* 0x45 */, kVK_ANSI_E, 'e' },
  { VKEY_F /* 0x46 */, kVK_ANSI_F, 'f' },
  { VKEY_G /* 0x47 */, kVK_ANSI_G, 'g' },
  { VKEY_H /* 0x48 */, kVK_ANSI_H, 'h' },
  { VKEY_I /* 0x49 */, kVK_ANSI_I, 'i' },
  { VKEY_J /* 0x4A */, kVK_ANSI_J, 'j' },
  { VKEY_K /* 0x4B */, kVK_ANSI_K, 'k' },
  { VKEY_L /* 0x4C */, kVK_ANSI_L, 'l' },
  { VKEY_M /* 0x4D */, kVK_ANSI_M, 'm' },
  { VKEY_N /* 0x4E */, kVK_ANSI_N, 'n' },
  { VKEY_O /* 0x4F */, kVK_ANSI_O, 'o' },
  { VKEY_P /* 0x50 */, kVK_ANSI_P, 'p' },
  { VKEY_Q /* 0x51 */, kVK_ANSI_Q, 'q' },
  { VKEY_R /* 0x52 */, kVK_ANSI_R, 'r' },
  { VKEY_S /* 0x53 */, kVK_ANSI_S, 's' },
  { VKEY_T /* 0x54 */, kVK_ANSI_T, 't' },
  { VKEY_U /* 0x55 */, kVK_ANSI_U, 'u' },
  { VKEY_V /* 0x56 */, kVK_ANSI_V, 'v' },
  { VKEY_W /* 0x57 */, kVK_ANSI_W, 'w' },
  { VKEY_X /* 0x58 */, kVK_ANSI_X, 'x' },
  { VKEY_Y /* 0x59 */, kVK_ANSI_Y, 'y' },
  { VKEY_Z /* 0x5A */, kVK_ANSI_Z, 'z' },
  { VKEY_LWIN /* 0x5B */, kVK_Command, 0 },
  { VKEY_RWIN /* 0x5C */, kVK_RightCommand, 0 },
  { VKEY_APPS /* 0x5D */, kVK_RightCommand, 0 },
  { VKEY_SLEEP /* 0x5F */, -1, 0 },
  { VKEY_NUMPAD0 /* 0x60 */, kVK_ANSI_Keypad0, '0' },
  { VKEY_NUMPAD1 /* 0x61 */, kVK_ANSI_Keypad1, '1' },
  { VKEY_NUMPAD2 /* 0x62 */, kVK_ANSI_Keypad2, '2' },
  { VKEY_NUMPAD3 /* 0x63 */, kVK_ANSI_Keypad3, '3' },
  { VKEY_NUMPAD4 /* 0x64 */, kVK_ANSI_Keypad4, '4' },
  { VKEY_NUMPAD5 /* 0x65 */, kVK_ANSI_Keypad5, '5' },
  { VKEY_NUMPAD6 /* 0x66 */, kVK_ANSI_Keypad6, '6' },
  { VKEY_NUMPAD7 /* 0x67 */, kVK_ANSI_Keypad7, '7' },
  { VKEY_NUMPAD8 /* 0x68 */, kVK_ANSI_Keypad8, '8' },
  { VKEY_NUMPAD9 /* 0x69 */, kVK_ANSI_Keypad9, '9' },
  { VKEY_MULTIPLY /* 0x6A */, kVK_ANSI_KeypadMultiply, '*' },
  { VKEY_ADD /* 0x6B */, kVK_ANSI_KeypadPlus, '+' },
  { VKEY_SEPARATOR /* 0x6C */, -1, 0 },
  { VKEY_SUBTRACT /* 0x6D */, kVK_ANSI_KeypadMinus, '-' },
  { VKEY_DECIMAL /* 0x6E */, kVK_ANSI_KeypadDecimal, '.' },
  { VKEY_DIVIDE /* 0x6F */, kVK_ANSI_KeypadDivide, '/' },
  { VKEY_F1 /* 0x70 */, kVK_F1, NSF1FunctionKey },
  { VKEY_F2 /* 0x71 */, kVK_F2, NSF2FunctionKey },
  { VKEY_F3 /* 0x72 */, kVK_F3, NSF3FunctionKey },
  { VKEY_F4 /* 0x73 */, kVK_F4, NSF4FunctionKey },
  { VKEY_F5 /* 0x74 */, kVK_F5, NSF5FunctionKey },
  { VKEY_F6 /* 0x75 */, kVK_F6, NSF6FunctionKey },
  { VKEY_F7 /* 0x76 */, kVK_F7, NSF7FunctionKey },
  { VKEY_F8 /* 0x77 */, kVK_F8, NSF8FunctionKey },
  { VKEY_F9 /* 0x78 */, kVK_F9, NSF9FunctionKey },
  { VKEY_F10 /* 0x79 */, kVK_F10, NSF10FunctionKey },
  { VKEY_F11 /* 0x7A */, kVK_F11, NSF11FunctionKey },
  { VKEY_F12 /* 0x7B */, kVK_F12, NSF12FunctionKey },
  { VKEY_F13 /* 0x7C */, kVK_F13, NSF13FunctionKey },
  { VKEY_F14 /* 0x7D */, kVK_F14, NSF14FunctionKey },
  { VKEY_F15 /* 0x7E */, kVK_F15, NSF15FunctionKey },
  { VKEY_F16 /* 0x7F */, kVK_F16, NSF16FunctionKey },
  { VKEY_F17 /* 0x80 */, kVK_F17, NSF17FunctionKey },
  { VKEY_F18 /* 0x81 */, kVK_F18, NSF18FunctionKey },
  { VKEY_F19 /* 0x82 */, kVK_F19, NSF19FunctionKey },
  { VKEY_F20 /* 0x83 */, kVK_F20, NSF20FunctionKey },
  { VKEY_F21 /* 0x84 */, -1, NSF21FunctionKey },
  { VKEY_F22 /* 0x85 */, -1, NSF22FunctionKey },
  { VKEY_F23 /* 0x86 */, -1, NSF23FunctionKey },
  { VKEY_F24 /* 0x87 */, -1, NSF24FunctionKey },
  { VKEY_NUMLOCK /* 0x90 */, -1, 0 },
  { VKEY_SCROLL /* 0x91 */, -1, NSScrollLockFunctionKey },
  { VKEY_LSHIFT /* 0xA0 */, kVK_Shift, 0 },
  { VKEY_RSHIFT /* 0xA1 */, kVK_Shift, 0 },
  { VKEY_LCONTROL /* 0xA2 */, kVK_Control, 0 },
  { VKEY_RCONTROL /* 0xA3 */, kVK_Control, 0 },
  { VKEY_LMENU /* 0xA4 */, -1, 0 },
  { VKEY_RMENU /* 0xA5 */, -1, 0 },
  { VKEY_BROWSER_BACK /* 0xA6 */, -1, 0 },
  { VKEY_BROWSER_FORWARD /* 0xA7 */, -1, 0 },
  { VKEY_BROWSER_REFRESH /* 0xA8 */, -1, 0 },
  { VKEY_BROWSER_STOP /* 0xA9 */, -1, 0 },
  { VKEY_BROWSER_SEARCH /* 0xAA */, -1, 0 },
  { VKEY_BROWSER_FAVORITES /* 0xAB */, -1, 0 },
  { VKEY_BROWSER_HOME /* 0xAC */, -1, 0 },
  { VKEY_VOLUME_MUTE /* 0xAD */, -1, 0 },
  { VKEY_VOLUME_DOWN /* 0xAE */, -1, 0 },
  { VKEY_VOLUME_UP /* 0xAF */, -1, 0 },
  { VKEY_MEDIA_NEXT_TRACK /* 0xB0 */, -1, 0 },
  { VKEY_MEDIA_PREV_TRACK /* 0xB1 */, -1, 0 },
  { VKEY_MEDIA_STOP /* 0xB2 */, -1, 0 },
  { VKEY_MEDIA_PLAY_PAUSE /* 0xB3 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_MAIL /* 0xB4 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_MEDIA_SELECT /* 0xB5 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_APP1 /* 0xB6 */, -1, 0 },
  { VKEY_MEDIA_LAUNCH_APP2 /* 0xB7 */, -1, 0 },
  { VKEY_OEM_1 /* 0xBA */, kVK_ANSI_Semicolon, ';' },
  { VKEY_OEM_PLUS /* 0xBB */, kVK_ANSI_Equal, '=' },
  { VKEY_OEM_COMMA /* 0xBC */, kVK_ANSI_Comma, ',' },
  { VKEY_OEM_MINUS /* 0xBD */, kVK_ANSI_Minus, '-' },
  { VKEY_OEM_PERIOD /* 0xBE */, kVK_ANSI_Period, '.' },
  { VKEY_OEM_2 /* 0xBF */, kVK_ANSI_Slash, '/' },
  { VKEY_OEM_3 /* 0xC0 */, kVK_ANSI_Grave, '`' },
  { VKEY_OEM_4 /* 0xDB */, kVK_ANSI_LeftBracket, '[' },
  { VKEY_OEM_5 /* 0xDC */, kVK_ANSI_Backslash, '\\' },
  { VKEY_OEM_6 /* 0xDD */, kVK_ANSI_RightBracket, ']' },
  { VKEY_OEM_7 /* 0xDE */, kVK_ANSI_Quote, '\'' },
  { VKEY_OEM_8 /* 0xDF */, -1, 0 },
  { VKEY_OEM_102 /* 0xE2 */, -1, 0 },
  { VKEY_PROCESSKEY /* 0xE5 */, -1, 0 },
  { VKEY_PACKET /* 0xE7 */, -1, 0 },
  { VKEY_ATTN /* 0xF6 */, -1, 0 },
  { VKEY_CRSEL /* 0xF7 */, -1, 0 },
  { VKEY_EXSEL /* 0xF8 */, -1, 0 },
  { VKEY_EREOF /* 0xF9 */, -1, 0 },
  { VKEY_PLAY /* 0xFA */, -1, 0 },
  { VKEY_ZOOM /* 0xFB */, -1, 0 },
  { VKEY_NONAME /* 0xFC */, -1, 0 },
  { VKEY_PA1 /* 0xFD */, -1, 0 },
  { VKEY_OEM_CLEAR /* 0xFE */, kVK_ANSI_KeypadClear, kClearCharCode }
};

bool IsKeypadOrNumericKeyEvent(NSEvent* event) {
  // Check that this is the type of event that has a keyCode.
  switch (event.type) {
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged:
      break;
    default:
      return false;
  }

  switch (event.keyCode) {
    case kVK_ANSI_KeypadClear:
    case kVK_ANSI_KeypadEquals:
    case kVK_ANSI_KeypadMultiply:
    case kVK_ANSI_KeypadDivide:
    case kVK_ANSI_KeypadMinus:
    case kVK_ANSI_KeypadPlus:
    case kVK_ANSI_KeypadEnter:
    case kVK_ANSI_KeypadDecimal:
    case kVK_ANSI_Keypad0:
    case kVK_ANSI_Keypad1:
    case kVK_ANSI_Keypad2:
    case kVK_ANSI_Keypad3:
    case kVK_ANSI_Keypad4:
    case kVK_ANSI_Keypad5:
    case kVK_ANSI_Keypad6:
    case kVK_ANSI_Keypad7:
    case kVK_ANSI_Keypad8:
    case kVK_ANSI_Keypad9:
    case kVK_ANSI_0:
    case kVK_ANSI_1:
    case kVK_ANSI_2:
    case kVK_ANSI_3:
    case kVK_ANSI_4:
    case kVK_ANSI_5:
    case kVK_ANSI_6:
    case kVK_ANSI_7:
    case kVK_ANSI_8:
    case kVK_ANSI_9:
      return true;
  }

  return false;
}

// A convenient array for getting symbol characters on the number keys.
const char kShiftCharsForNumberKeys[] = ")!@#$%^&*(";

// Translates from character code to keyboard code.
KeyboardCode KeyboardCodeFromCharCode(unichar charCode) {
  switch (charCode) {
    case 'a': case 'A': return VKEY_A;
    case 'b': case 'B': return VKEY_B;
    case 'c': case 'C': return VKEY_C;
    case 'd': case 'D': return VKEY_D;
    case 'e': case 'E': return VKEY_E;
    case 'f': case 'F': return VKEY_F;
    case 'g': case 'G': return VKEY_G;
    case 'h': case 'H': return VKEY_H;
    case 'i': case 'I': return VKEY_I;
    case 'j': case 'J': return VKEY_J;
    case 'k': case 'K': return VKEY_K;
    case 'l': case 'L': return VKEY_L;
    case 'm': case 'M': return VKEY_M;
    case 'n': case 'N': return VKEY_N;
    case 'o': case 'O': return VKEY_O;
    case 'p': case 'P': return VKEY_P;
    case 'q': case 'Q': return VKEY_Q;
    case 'r': case 'R': return VKEY_R;
    case 's': case 'S': return VKEY_S;
    case 't': case 'T': return VKEY_T;
    case 'u': case 'U': return VKEY_U;
    case 'v': case 'V': return VKEY_V;
    case 'w': case 'W': return VKEY_W;
    case 'x': case 'X': return VKEY_X;
    case 'y': case 'Y': return VKEY_Y;
    case 'z': case 'Z': return VKEY_Z;

    case NSPauseFunctionKey: return VKEY_PAUSE;
    case NSSelectFunctionKey: return VKEY_SELECT;
    case NSPrintFunctionKey: return VKEY_PRINT;
    case NSExecuteFunctionKey: return VKEY_EXECUTE;
    case NSPrintScreenFunctionKey: return VKEY_SNAPSHOT;
    case NSInsertFunctionKey: return VKEY_INSERT;
    case NSF21FunctionKey: return VKEY_F21;
    case NSF22FunctionKey: return VKEY_F22;
    case NSF23FunctionKey: return VKEY_F23;
    case NSF24FunctionKey: return VKEY_F24;
    case NSScrollLockFunctionKey: return VKEY_SCROLL;

    // U.S. Specific mappings.  Mileage may vary.
    case ';': case ':': return VKEY_OEM_1;
    case '=': case '+': return VKEY_OEM_PLUS;
    case ',': case '<': return VKEY_OEM_COMMA;
    case '-': case '_': return VKEY_OEM_MINUS;
    case '.': case '>': return VKEY_OEM_PERIOD;
    case '/': case '?': return VKEY_OEM_2;
    case '`': case '~': return VKEY_OEM_3;
    case '[': case '{': return VKEY_OEM_4;
    case '\\': case '|': return VKEY_OEM_5;
    case ']': case '}': return VKEY_OEM_6;
    case '\'': case '"': return VKEY_OEM_7;
  }

  return VKEY_UNKNOWN;
}

DomKey DomKeyFromKeyCode(unsigned short keyCode) {
  switch (keyCode) {
    case kVK_ANSI_KeypadEnter:
    case kVK_Return:
      return DomKey::ENTER;
    case kVK_Tab:
      return DomKey::TAB;
    case kVK_Delete:
      return DomKey::BACKSPACE;
    case kVK_Escape:
      return DomKey::ESCAPE;
    case kVK_Command:
    case kVK_RightCommand:
      return DomKey::META;
    case kVK_Shift:
    case kVK_RightShift:
      return DomKey::SHIFT;
    case kVK_CapsLock:
      return DomKey::CAPS_LOCK;
    case kVK_Option:
    case kVK_RightOption:
      return DomKey::ALT;
    case kVK_Control:
    case kVK_RightControl:
      return DomKey::CONTROL;
    case kVK_Function:
      return DomKey::FN;
    case kVK_VolumeUp:
      return DomKey::AUDIO_VOLUME_UP;
    case kVK_VolumeDown:
      return DomKey::AUDIO_VOLUME_DOWN;
    case kVK_Mute:
      return DomKey::AUDIO_VOLUME_MUTE;
    case kVK_F1:
      return DomKey::F1;
    case kVK_F2:
      return DomKey::F2;
    case kVK_F3:
      return DomKey::F3;
    case kVK_F4:
      return DomKey::F4;
    case kVK_F5:
      return DomKey::F5;
    case kVK_F6:
      return DomKey::F6;
    case kVK_F7:
      return DomKey::F7;
    case kVK_F8:
      return DomKey::F8;
    case kVK_F9:
      return DomKey::F9;
    case kVK_F10:
      return DomKey::F10;
    case kVK_F11:
      return DomKey::F11;
    case kVK_F12:
      return DomKey::F12;
    case kVK_F13:
      return DomKey::F13;
    case kVK_F14:
      return DomKey::F14;
    case kVK_F15:
      return DomKey::F15;
    case kVK_F16:
      return DomKey::F16;
    case kVK_F17:
      return DomKey::F17;
    case kVK_F18:
      return DomKey::F18;
    case kVK_F19:
      return DomKey::F19;
    case kVK_F20:
      return DomKey::F20;
    case kVK_Help:
      return DomKey::HELP;
    case kVK_Home:
      return DomKey::HOME;
    case kVK_PageUp:
      return DomKey::PAGE_UP;
    case kVK_ForwardDelete:
      return DomKey::DEL;
    case kVK_End:
      return DomKey::END;
    case kVK_PageDown:
      return DomKey::PAGE_DOWN;
    case kVK_LeftArrow:
      return DomKey::ARROW_LEFT;
    case kVK_RightArrow:
      return DomKey::ARROW_RIGHT;
    case kVK_DownArrow:
      return DomKey::ARROW_DOWN;
    case kVK_UpArrow:
      return DomKey::ARROW_UP;
    case kVK_ContextMenu:
      return DomKey::CONTEXT_MENU;
    case kVK_JIS_Eisu:
      return DomKey::EISU;
    case kVK_JIS_Kana:
      return DomKey::KANJI_MODE;
    default:
      return DomKey::NONE;
  }
}

DomKey DomKeyFromCharCode(unichar char_code) {
  switch (char_code) {
    case NSUpArrowFunctionKey:
      return DomKey::ARROW_UP;
    case NSDownArrowFunctionKey:
      return DomKey::ARROW_DOWN;
    case NSLeftArrowFunctionKey:
      return DomKey::ARROW_LEFT;
    case NSRightArrowFunctionKey:
      return DomKey::ARROW_RIGHT;
    case NSF1FunctionKey:
      return DomKey::F1;
    case NSF2FunctionKey:
      return DomKey::F2;
    case NSF3FunctionKey:
      return DomKey::F3;
    case NSF4FunctionKey:
      return DomKey::F4;
    case NSF5FunctionKey:
      return DomKey::F5;
    case NSF6FunctionKey:
      return DomKey::F6;
    case NSF7FunctionKey:
      return DomKey::F7;
    case NSF8FunctionKey:
      return DomKey::F8;
    case NSF9FunctionKey:
      return DomKey::F9;
    case NSF10FunctionKey:
      return DomKey::F10;
    case NSF11FunctionKey:
      return DomKey::F11;
    case NSF12FunctionKey:
      return DomKey::F12;
    case NSF13FunctionKey:
      return DomKey::F13;
    case NSF14FunctionKey:
      return DomKey::F14;
    case NSF15FunctionKey:
      return DomKey::F15;
    case NSF16FunctionKey:
      return DomKey::F16;
    case NSF17FunctionKey:
      return DomKey::F17;
    case NSF18FunctionKey:
      return DomKey::F18;
    case NSF19FunctionKey:
      return DomKey::F19;
    case NSF20FunctionKey:
      return DomKey::F20;
    case NSF21FunctionKey:
      return DomKey::F21;
    case NSF22FunctionKey:
      return DomKey::F22;
    case NSF23FunctionKey:
      return DomKey::F23;
    case NSF24FunctionKey:
      return DomKey::F24;
    case NSInsertFunctionKey:
      return DomKey::INSERT;
    case NSDeleteFunctionKey:
      return DomKey::DEL;
    case NSHomeFunctionKey:
      return DomKey::HOME;
    case NSEndFunctionKey:
      return DomKey::END;
    case NSPageUpFunctionKey:
      return DomKey::PAGE_UP;
    case NSPageDownFunctionKey:
      return DomKey::PAGE_DOWN;
    case NSPrintScreenFunctionKey:
      return DomKey::PRINT_SCREEN;
    case NSScrollLockFunctionKey:
      return DomKey::SCROLL_LOCK;
    case NSPauseFunctionKey:
      return DomKey::PAUSE;
    case NSPrintFunctionKey:
      return DomKey::PRINT;
    case NSClearLineFunctionKey:
      return DomKey::CLEAR;
    case NSSelectFunctionKey:
      return DomKey::SELECT;
    case NSExecuteFunctionKey:
      return DomKey::EXECUTE;
    case NSUndoFunctionKey:
      return DomKey::UNDO;
    case NSRedoFunctionKey:
      return DomKey::REDO;
    case NSFindFunctionKey:
      return DomKey::FIND;
    case NSHelpFunctionKey:
      return DomKey::HELP;
    default:
      return DomKey::FromCharacter(char_code);
  }
}

UniChar MacKeycodeAndModifiersToCharacter(unsigned short mac_keycode,
                                          int modifiers,
                                          bool* is_dead_key) {
  // Convert NSEvent modifiers to format UCKeyTranslate accepts. See docs
  // on UCKeyTranslate for more info.
  int unicode_modifiers = 0;
  if (modifiers & NSEventModifierFlagShift)
    unicode_modifiers |= shiftKey;
  if (modifiers & NSEventModifierFlagCapsLock)
    unicode_modifiers |= alphaLock;
  // if (modifiers & NSEventModifierFlagControl)
  //   unicode_modifiers |= controlKey;
  if (modifiers & NSEventModifierFlagOption)
    unicode_modifiers |= optionKey;
  // if (modifiers & NSEventModifierFlagCommand)
  //   unicode_modifiers |= cmdKey;
  UInt32 modifier_key_state = (unicode_modifiers >> 8) & 0xFF;

  UInt32 dead_key_state = 0;
  base::ScopedCFTypeRef<TISInputSourceRef> input_source(
      TISCopyCurrentKeyboardLayoutInputSource());
  UniChar translated_char = TranslatedUnicodeCharFromKeyCode(
      input_source.get(), static_cast<UInt16>(mac_keycode), kUCKeyActionDown,
      modifier_key_state, LMGetKbdLast(), &dead_key_state);

  *is_dead_key = dead_key_state != 0;
  if (*is_dead_key) {
    translated_char = TranslatedUnicodeCharFromKeyCode(
        input_source.get(), static_cast<UInt16>(kVK_Space), kUCKeyActionDown, 0,
        LMGetKbdLast(), &dead_key_state);
  }

  return translated_char;
}

}  // namespace

int MacKeyCodeForWindowsKeyCode(KeyboardCode keycode,
                                NSUInteger flags,
                                unichar* us_keyboard_shifted_character,
                                unichar* keyboard_character) {
  // In release code, |flags| is used to lookup accelerators, so logic to handle
  // caps lock properly isn't implemented.
  DCHECK_EQ(0u, flags & NSEventModifierFlagCapsLock);

  KeyCodeMap from;
  from.keycode = keycode;

  const KeyCodeMap* ptr = std::lower_bound(
      kKeyCodesMap, kKeyCodesMap + std::size(kKeyCodesMap), from);

  if (ptr >= kKeyCodesMap + std::size(kKeyCodesMap) ||
      ptr->keycode != keycode || ptr->macKeycode == -1)
    return -1;

  int macKeycode = ptr->macKeycode;
  if (keyboard_character)
    *keyboard_character = ptr->characterIgnoringAllModifiers;

  if (!us_keyboard_shifted_character)
    return macKeycode;

  *us_keyboard_shifted_character = ptr->characterIgnoringAllModifiers;

  // Fill in |us_keyboard_shifted_character| according to flags.
  if (flags & NSEventModifierFlagShift) {
    if (keycode >= VKEY_0 && keycode <= VKEY_9) {
      *us_keyboard_shifted_character =
          kShiftCharsForNumberKeys[keycode - VKEY_0];
    } else if (keycode >= VKEY_A && keycode <= VKEY_Z) {
      *us_keyboard_shifted_character = 'A' + (keycode - VKEY_A);
    } else {
      switch (macKeycode) {
        case kVK_ANSI_Grave:
          *us_keyboard_shifted_character = '~';
          break;
        case kVK_ANSI_Minus:
          *us_keyboard_shifted_character = '_';
          break;
        case kVK_ANSI_Equal:
          *us_keyboard_shifted_character = '+';
          break;
        case kVK_ANSI_LeftBracket:
          *us_keyboard_shifted_character = '{';
          break;
        case kVK_ANSI_RightBracket:
          *us_keyboard_shifted_character = '}';
          break;
        case kVK_ANSI_Backslash:
          *us_keyboard_shifted_character = '|';
          break;
        case kVK_ANSI_Semicolon:
          *us_keyboard_shifted_character = ':';
          break;
        case kVK_ANSI_Quote:
          *us_keyboard_shifted_character = '\"';
          break;
        case kVK_ANSI_Comma:
          *us_keyboard_shifted_character = '<';
          break;
        case kVK_ANSI_Period:
          *us_keyboard_shifted_character = '>';
          break;
        case kVK_ANSI_Slash:
          *us_keyboard_shifted_character = '?';
          break;
        default:
          break;
      }
    }
  }

  // TODO(suzhe): Support characters for Option key bindings.
  return macKeycode;
}

KeyboardCode KeyboardCodeFromKeyCode(unsigned short keyCode) {
  static const KeyboardCode kKeyboardCodes[] = {
      /* 0 */ VKEY_A,
      /* 1 */ VKEY_S,
      /* 2 */ VKEY_D,
      /* 3 */ VKEY_F,
      /* 4 */ VKEY_H,
      /* 5 */ VKEY_G,
      /* 6 */ VKEY_Z,
      /* 7 */ VKEY_X,
      /* 8 */ VKEY_C,
      /* 9 */ VKEY_V,
      /* 0x0A */ VKEY_OEM_3,  // Section key.
      /* 0x0B */ VKEY_B,
      /* 0x0C */ VKEY_Q,
      /* 0x0D */ VKEY_W,
      /* 0x0E */ VKEY_E,
      /* 0x0F */ VKEY_R,
      /* 0x10 */ VKEY_Y,
      /* 0x11 */ VKEY_T,
      /* 0x12 */ VKEY_1,
      /* 0x13 */ VKEY_2,
      /* 0x14 */ VKEY_3,
      /* 0x15 */ VKEY_4,
      /* 0x16 */ VKEY_6,
      /* 0x17 */ VKEY_5,
      /* 0x18 */ VKEY_OEM_PLUS,  // =+
      /* 0x19 */ VKEY_9,
      /* 0x1A */ VKEY_7,
      /* 0x1B */ VKEY_OEM_MINUS,  // -_
      /* 0x1C */ VKEY_8,
      /* 0x1D */ VKEY_0,
      /* 0x1E */ VKEY_OEM_6,  // ]}
      /* 0x1F */ VKEY_O,
      /* 0x20 */ VKEY_U,
      /* 0x21 */ VKEY_OEM_4,  // {[
      /* 0x22 */ VKEY_I,
      /* 0x23 */ VKEY_P,
      /* 0x24 */ VKEY_RETURN,  // Return
      /* 0x25 */ VKEY_L,
      /* 0x26 */ VKEY_J,
      /* 0x27 */ VKEY_OEM_7,  // '"
      /* 0x28 */ VKEY_K,
      /* 0x29 */ VKEY_OEM_1,      // ;:
      /* 0x2A */ VKEY_OEM_5,      // \|
      /* 0x2B */ VKEY_OEM_COMMA,  // ,<
      /* 0x2C */ VKEY_OEM_2,      // /?
      /* 0x2D */ VKEY_N,
      /* 0x2E */ VKEY_M,
      /* 0x2F */ VKEY_OEM_PERIOD,  // .>
      /* 0x30 */ VKEY_TAB,
      /* 0x31 */ VKEY_SPACE,
      /* 0x32 */ VKEY_OEM_3,    // `~
      /* 0x33 */ VKEY_BACK,     // Backspace
      /* 0x34 */ VKEY_UNKNOWN,  // n/a
      /* 0x35 */ VKEY_ESCAPE,
      /* 0x36 */ VKEY_APPS,     // Right Command
      /* 0x37 */ VKEY_LWIN,     // Left Command
      /* 0x38 */ VKEY_SHIFT,    // Left Shift
      /* 0x39 */ VKEY_CAPITAL,  // Caps Lock
      /* 0x3A */ VKEY_MENU,     // Left Option
      /* 0x3B */ VKEY_CONTROL,  // Left Ctrl
      /* 0x3C */ VKEY_SHIFT,    // Right Shift
      /* 0x3D */ VKEY_MENU,     // Right Option
      /* 0x3E */ VKEY_CONTROL,  // Right Ctrl
      /* 0x3F */ VKEY_UNKNOWN,  // fn
      /* 0x40 */ VKEY_F17,
      /* 0x41 */ VKEY_DECIMAL,   // Num Pad .
      /* 0x42 */ VKEY_UNKNOWN,   // n/a
      /* 0x43 */ VKEY_MULTIPLY,  // Num Pad *
      /* 0x44 */ VKEY_UNKNOWN,   // n/a
      /* 0x45 */ VKEY_ADD,       // Num Pad +
      /* 0x46 */ VKEY_UNKNOWN,   // n/a
      /* 0x47 */ VKEY_CLEAR,     // Num Pad Clear
      /* 0x48 */ VKEY_VOLUME_UP,
      /* 0x49 */ VKEY_VOLUME_DOWN,
      /* 0x4A */ VKEY_VOLUME_MUTE,
      /* 0x4B */ VKEY_DIVIDE,    // Num Pad /
      /* 0x4C */ VKEY_RETURN,    // Num Pad Enter
      /* 0x4D */ VKEY_UNKNOWN,   // n/a
      /* 0x4E */ VKEY_SUBTRACT,  // Num Pad -
      /* 0x4F */ VKEY_F18,
      /* 0x50 */ VKEY_F19,
      /* 0x51 */ VKEY_OEM_PLUS,  // Num Pad =.
      /* 0x52 */ VKEY_NUMPAD0,
      /* 0x53 */ VKEY_NUMPAD1,
      /* 0x54 */ VKEY_NUMPAD2,
      /* 0x55 */ VKEY_NUMPAD3,
      /* 0x56 */ VKEY_NUMPAD4,
      /* 0x57 */ VKEY_NUMPAD5,
      /* 0x58 */ VKEY_NUMPAD6,
      /* 0x59 */ VKEY_NUMPAD7,
      /* 0x5A */ VKEY_F20,
      /* 0x5B */ VKEY_NUMPAD8,
      /* 0x5C */ VKEY_NUMPAD9,
      /* 0x5D */ VKEY_UNKNOWN,  // Yen (JIS Keyboard Only)
      /* 0x5E */ VKEY_UNKNOWN,  // Underscore (JIS Keyboard Only)
      /* 0x5F */ VKEY_UNKNOWN,  // KeypadComma (JIS Keyboard Only)
      /* 0x60 */ VKEY_F5,
      /* 0x61 */ VKEY_F6,
      /* 0x62 */ VKEY_F7,
      /* 0x63 */ VKEY_F3,
      /* 0x64 */ VKEY_F8,
      /* 0x65 */ VKEY_F9,
      /* 0x66 */ VKEY_UNKNOWN,  // Eisu (JIS Keyboard Only)
      /* 0x67 */ VKEY_F11,
      /* 0x68 */ VKEY_UNKNOWN,  // Kana (JIS Keyboard Only)
      /* 0x69 */ VKEY_F13,
      /* 0x6A */ VKEY_F16,
      /* 0x6B */ VKEY_F14,
      /* 0x6C */ VKEY_UNKNOWN,  // n/a
      /* 0x6D */ VKEY_F10,
      /* 0x6E */ VKEY_APPS,  // Context Menu key
      /* 0x6F */ VKEY_F12,
      /* 0x70 */ VKEY_UNKNOWN,  // n/a
      /* 0x71 */ VKEY_F15,
      /* 0x72 */ VKEY_INSERT,  // Help
      /* 0x73 */ VKEY_HOME,    // Home
      /* 0x74 */ VKEY_PRIOR,   // Page Up
      /* 0x75 */ VKEY_DELETE,  // Forward Delete
      /* 0x76 */ VKEY_F4,
      /* 0x77 */ VKEY_END,  // End
      /* 0x78 */ VKEY_F2,
      /* 0x79 */ VKEY_NEXT,  // Page Down
      /* 0x7A */ VKEY_F1,
      /* 0x7B */ VKEY_LEFT,    // Left Arrow
      /* 0x7C */ VKEY_RIGHT,   // Right Arrow
      /* 0x7D */ VKEY_DOWN,    // Down Arrow
      /* 0x7E */ VKEY_UP,      // Up Arrow
      /* 0x7F */ VKEY_UNKNOWN  // n/a
  };

  if (keyCode >= 0x80)
    return VKEY_UNKNOWN;

  return kKeyboardCodes[keyCode];
}

KeyboardCode KeyboardCodeFromNSEvent(NSEvent* event) {
  KeyboardCode code = VKEY_UNKNOWN;

  // Numeric keys 0-9 should always return |keyCode| 0-9.
  // https://developer.mozilla.org/en-US/docs/Web/API/KeyboardEvent/keyCode#Printable_keys_in_standard_position
  if (!IsKeypadOrNumericKeyEvent(event) &&
      (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp)) {
    // Handles Dvorak-QWERTY Cmd case.
    // https://github.com/WebKit/webkit/blob/4d41c98b1de467f5d2a8fcba84d7c5268f11b0cc/Source/WebCore/platform/mac/PlatformEventFactoryMac.mm#L329
    NSString* characters = event.characters;
    if (characters.length > 0) {
      code = KeyboardCodeFromCharCode([characters characterAtIndex:0]);
    }
    if (code)
      return code;

    characters = event.charactersIgnoringModifiers;
    if (characters.length > 0) {
      code = KeyboardCodeFromCharCode([characters characterAtIndex:0]);
    }
    if (code)
      return code;
  }
  return KeyboardCodeFromKeyCode(event.keyCode);
}

int ISOKeyboardKeyCodeMap(int nativeKeyCode) {
  // macOS will swap 'Backquote' and 'IntlBackslash' if it's an ISO keyboard.
  // https://crbug.com/600607
  switch (nativeKeyCode) {
    case kVK_ISO_Section:
      return kVK_ANSI_Grave;
    case kVK_ANSI_Grave:
      return kVK_ISO_Section;
    default:
      return nativeKeyCode;
  }
}

DomCode DomCodeFromNSEvent(NSEvent* event) {
  if (KBGetLayoutType(LMGetKbdType()) == kKeyboardISO) {
    return ui::KeycodeConverter::NativeKeycodeToDomCode(
        ISOKeyboardKeyCodeMap(event.keyCode));
  }

  return ui::KeycodeConverter::NativeKeycodeToDomCode(event.keyCode);
}

DomKey DomKeyFromNSEvent(NSEvent* event) {
  // Apply the lookup based on the character first since that has the
  // Keyboard layout and modifiers already applied; whereas the keyCode
  // doesn't.
  if (event.type == NSEventTypeKeyDown || event.type == NSEventTypeKeyUp) {
    // Cannot use `event.characters` to check whether it's a dead key, because
    // KeyUp event has the character form of the dead key in `event.characters`.
    bool is_dead_key = false;
    // MacKeycodeAndModifiersToCharacter() is efficient (around 6E-4 ms).
    unichar dead_dom_key_char = MacKeycodeAndModifiersToCharacter(
        event.keyCode, event.modifierFlags, &is_dead_key);
    if (is_dead_key)
      return DomKey::DeadKeyFromCombiningCharacter(dead_dom_key_char);

    // Mac Eisu Kana key events have a space symbol (U+0020) as [event
    // characters]. However, the symbol is not generated for users and the event
    // is just used for enabling/disabling an IME.
    if (event.keyCode == kVK_JIS_Eisu || event.keyCode == kVK_JIS_Kana) {
      return DomKeyFromKeyCode(event.keyCode);
    }

    // `event.characters` will have dead key state applied.
    NSString* characters = event.characters;
    if (characters.length > 0) {
      // An invalid dead key combination will produce two characters, according
      // to spec DomKey should be the last character.
      // e.g. On French keyboard [+a will produce "^q", DomKey should be 'q'.
      unichar dom_key_char =
          [characters characterAtIndex:characters.length - 1];
      if (IsUnicodeControl(dom_key_char)) {
        // Filter non-glyph modifiers if the generated characters are part of
        // Unicode 'Other, Control' General Category.
        // https://w3c.github.io/uievents-key/#selecting-key-attribute-values
        bool unused_is_dead_key;
        const int kAllowedModifiersMask = NSEventModifierFlagShift |
                                          NSEventModifierFlagCapsLock |
                                          NSEventModifierFlagOption;
        // MacKeycodeAndModifiersToCharacter() is efficient (around 6E-4 ms).
        dom_key_char = MacKeycodeAndModifiersToCharacter(
            event.keyCode, event.modifierFlags & kAllowedModifiersMask,
            &unused_is_dead_key);
      }

      // We need to check again because keys like ESC will produce control
      // characters even without any modifiers.
      if (!IsUnicodeControl(dom_key_char))
        return DomKeyFromCharCode(dom_key_char);
    }
  }
  return DomKeyFromKeyCode(event.keyCode);
}

UniChar TranslatedUnicodeCharFromKeyCode(TISInputSourceRef input_source,
                                         UInt16 key_code,
                                         UInt16 key_action,
                                         UInt32 modifier_key_state,
                                         UInt32 keyboard_type,
                                         UInt32* dead_key_state) {
  DCHECK(dead_key_state);

  CFDataRef layout_data =
      base::mac::CFCast<CFDataRef>(TISGetInputSourceProperty(
          input_source, kTISPropertyUnicodeKeyLayoutData));
  if (!layout_data)
    return 0xFFFD;  // REPLACEMENT CHARACTER

  const UCKeyboardLayout* keyboard_layout =
      reinterpret_cast<const UCKeyboardLayout*>(CFDataGetBytePtr(layout_data));
  DCHECK(keyboard_layout);

  UniChar char_buffer[kUCKeyTranslateBufferLength] = {0};
  UniCharCount buffer_length = kUCKeyTranslateBufferLength;

  OSStatus ret = UCKeyTranslate(
      keyboard_layout, key_code, key_action, modifier_key_state, keyboard_type,
      kUCKeyTranslateNoDeadKeysBit, dead_key_state, kUCKeyTranslateBufferLength,
      &buffer_length, char_buffer);
  OSSTATUS_DCHECK(ret == noErr, ret);

  // TODO(input-dev): Handle multiple character case. Should be rare.
  return char_buffer[0];
}

}  // namespace ui
