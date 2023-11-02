// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_android.h"

#include <android/keycodes.h>

namespace ui {
namespace {

const int kCombiningAccent = 0x80000000;
const int kCombiningAccentMask = 0x7fffffff;

// The minimum Android NDK does not provide values for these yet:
enum {
  AKEYCODE_ASSIST = 219,
  AKEYCODE_BRIGHTNESS_DOWN = 220,
  AKEYCODE_BRIGHTNESS_UP = 221,
  AKEYCODE_MEDIA_AUDIO_TRACK = 222,
  AKEYCODE_SLEEP = 223,
  AKEYCODE_WAKEUP = 224,
  AKEYCODE_PAIRING = 225,
  AKEYCODE_MEDIA_TOP_MENU = 226,
  AKEYCODE_11 = 227,
  AKEYCODE_12 = 228,
  AKEYCODE_LAST_CHANNEL = 229,
  AKEYCODE_TV_DATA_SERVICE = 230,
  AKEYCODE_VOICE_ASSIST = 231,
  AKEYCODE_TV_RADIO_SERVICE = 232,
  AKEYCODE_TV_TELETEXT = 233,
  AKEYCODE_TV_NUMBER_ENTRY = 234,
  AKEYCODE_TV_TERRESTRIAL_ANALOG = 235,
  AKEYCODE_TV_TERRESTRIAL_DIGITAL = 236,
  AKEYCODE_TV_SATELLITE = 237,
  AKEYCODE_TV_SATELLITE_BS = 238,
  AKEYCODE_TV_SATELLITE_CS = 239,
  AKEYCODE_TV_SATELLITE_SERVICE = 240,
  AKEYCODE_TV_NETWORK = 241,
  AKEYCODE_TV_ANTENNA_CABLE = 242,
  AKEYCODE_TV_INPUT_HDMI_1 = 243,
  AKEYCODE_TV_INPUT_HDMI_2 = 244,
  AKEYCODE_TV_INPUT_HDMI_3 = 245,
  AKEYCODE_TV_INPUT_HDMI_4 = 246,
  AKEYCODE_TV_INPUT_COMPOSITE_1 = 247,
  AKEYCODE_TV_INPUT_COMPOSITE_2 = 248,
  AKEYCODE_TV_INPUT_COMPONENT_1 = 249,
  AKEYCODE_TV_INPUT_COMPONENT_2 = 250,
  AKEYCODE_TV_INPUT_VGA_1 = 251,
  AKEYCODE_TV_AUDIO_DESCRIPTION = 252,
  AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP = 253,
  AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254,
  AKEYCODE_TV_ZOOM_MODE = 255,
  AKEYCODE_TV_CONTENTS_MENU = 256,
  AKEYCODE_TV_MEDIA_CONTEXT_MENU = 257,
  AKEYCODE_TV_TIMER_PROGRAMMING = 258,
  AKEYCODE_HELP = 259,
  AKEYCODE_NAVIGATE_PREVIOUS = 260,
  AKEYCODE_NAVIGATE_NEXT = 261,
  AKEYCODE_NAVIGATE_IN = 262,
  AKEYCODE_NAVIGATE_OUT = 263,
  AKEYCODE_MEDIA_SKIP_FORWARD = 272,
  AKEYCODE_MEDIA_SKIP_BACKWARD = 273,
  AKEYCODE_MEDIA_STEP_FORWARD = 274,
  AKEYCODE_MEDIA_STEP_BACKWARD = 275,
  AKEYCODE_CUT = 277,
  AKEYCODE_COPY = 278,
  AKEYCODE_PASTE = 279,
};

DomKey GetDomKeyFromAndroidKeycode(int keycode) {
  switch (keycode) {
    default:
    case AKEYCODE_UNKNOWN:
      return DomKey::NONE;
    case AKEYCODE_HOME:
      return DomKey::GO_HOME;
    case AKEYCODE_BACK:
      return DomKey::GO_BACK;
    case AKEYCODE_CALL:
      return DomKey::CALL;
    case AKEYCODE_ENDCALL:
      return DomKey::END_CALL;
    case AKEYCODE_DPAD_UP:
      return DomKey::ARROW_UP;
    case AKEYCODE_DPAD_DOWN:
      return DomKey::ARROW_DOWN;
    case AKEYCODE_DPAD_LEFT:
      return DomKey::ARROW_LEFT;
    case AKEYCODE_DPAD_RIGHT:
      return DomKey::ARROW_RIGHT;
    case AKEYCODE_DPAD_CENTER:
      return DomKey::ENTER;
    case AKEYCODE_VOLUME_UP:
      return DomKey::AUDIO_VOLUME_UP;
    case AKEYCODE_VOLUME_DOWN:
      return DomKey::AUDIO_VOLUME_DOWN;
    case AKEYCODE_POWER:
      return DomKey::POWER;
    case AKEYCODE_CAMERA:
      return DomKey::CAMERA;
    case AKEYCODE_CLEAR:
      return DomKey::CLEAR;
    case AKEYCODE_ALT_LEFT:
    case AKEYCODE_ALT_RIGHT:
      return DomKey::ALT;
    case AKEYCODE_SHIFT_LEFT:
    case AKEYCODE_SHIFT_RIGHT:
      return DomKey::SHIFT;
    case AKEYCODE_TAB:
      return DomKey::TAB;
    case AKEYCODE_SYM:
      return DomKey::SYMBOL;
    case AKEYCODE_EXPLORER:
      return DomKey::LAUNCH_WEB_BROWSER;
    case AKEYCODE_ENVELOPE:
      return DomKey::LAUNCH_MAIL;
    case AKEYCODE_ENTER:
      return DomKey::ENTER;
    case AKEYCODE_DEL:
      return DomKey::BACKSPACE;
    case AKEYCODE_HEADSETHOOK:
      return DomKey::HEADSET_HOOK;
    case AKEYCODE_FOCUS:
      return DomKey::CAMERA_FOCUS;
    case AKEYCODE_NOTIFICATION:
      return DomKey::NOTIFICATION;
    case AKEYCODE_MENU:
      return DomKey::CONTEXT_MENU;
    case AKEYCODE_SEARCH:
      return DomKey::BROWSER_SEARCH;
    case AKEYCODE_MEDIA_PLAY_PAUSE:
      return DomKey::MEDIA_PLAY_PAUSE;
    case AKEYCODE_MEDIA_STOP:
      return DomKey::MEDIA_STOP;
    case AKEYCODE_MEDIA_NEXT:
      return DomKey::MEDIA_TRACK_NEXT;
    case AKEYCODE_MEDIA_PREVIOUS:
      return DomKey::MEDIA_TRACK_PREVIOUS;
    case AKEYCODE_MEDIA_REWIND:
      return DomKey::MEDIA_REWIND;
    case AKEYCODE_MEDIA_FAST_FORWARD:
      return DomKey::MEDIA_FAST_FORWARD;
    case AKEYCODE_MUTE:
      return DomKey::MICROPHONE_VOLUME_MUTE;
    case AKEYCODE_PAGE_UP:
      return DomKey::PAGE_UP;
    case AKEYCODE_PAGE_DOWN:
      return DomKey::PAGE_DOWN;
    case AKEYCODE_SWITCH_CHARSET:
      return DomKey::MODE_CHANGE;
    case AKEYCODE_ESCAPE:
      return DomKey::ESCAPE;
    case AKEYCODE_FORWARD_DEL:
      return DomKey::DEL;
    case AKEYCODE_CTRL_LEFT:
    case AKEYCODE_CTRL_RIGHT:
      return DomKey::CONTROL;
    case AKEYCODE_CAPS_LOCK:
      return DomKey::CAPS_LOCK;
    case AKEYCODE_SCROLL_LOCK:
      return DomKey::SCROLL_LOCK;
    case AKEYCODE_META_LEFT:
    case AKEYCODE_META_RIGHT:
      return DomKey::META;
    case AKEYCODE_FUNCTION:
      return DomKey::FN;
    case AKEYCODE_SYSRQ:
      return DomKey::PRINT_SCREEN;
    case AKEYCODE_BREAK:
      return DomKey::PAUSE;
    case AKEYCODE_MOVE_HOME:
      return DomKey::HOME;
    case AKEYCODE_MOVE_END:
      return DomKey::END;
    case AKEYCODE_INSERT:
      return DomKey::INSERT;
    case AKEYCODE_FORWARD:
      return DomKey::BROWSER_FORWARD;
    case AKEYCODE_MEDIA_PLAY:
      return DomKey::MEDIA_PLAY;
    case AKEYCODE_MEDIA_PAUSE:
      return DomKey::MEDIA_PAUSE;
    case AKEYCODE_MEDIA_CLOSE:
      return DomKey::CLOSE;
    case AKEYCODE_MEDIA_EJECT:
      return DomKey::EJECT;
    case AKEYCODE_MEDIA_RECORD:
      return DomKey::MEDIA_RECORD;
    case AKEYCODE_F1:
      return DomKey::F1;
    case AKEYCODE_F2:
      return DomKey::F2;
    case AKEYCODE_F3:
      return DomKey::F3;
    case AKEYCODE_F4:
      return DomKey::F4;
    case AKEYCODE_F5:
      return DomKey::F5;
    case AKEYCODE_F6:
      return DomKey::F6;
    case AKEYCODE_F7:
      return DomKey::F7;
    case AKEYCODE_F8:
      return DomKey::F8;
    case AKEYCODE_F9:
      return DomKey::F9;
    case AKEYCODE_F10:
      return DomKey::F10;
    case AKEYCODE_F11:
      return DomKey::F11;
    case AKEYCODE_F12:
      return DomKey::F12;
    case AKEYCODE_NUM_LOCK:
      return DomKey::NUM_LOCK;
    case AKEYCODE_NUMPAD_ENTER:
      return DomKey::ENTER;
    case AKEYCODE_VOLUME_MUTE:
      return DomKey::AUDIO_VOLUME_MUTE;
    case AKEYCODE_INFO:
      return DomKey::INFO;
    case AKEYCODE_CHANNEL_UP:
      return DomKey::CHANNEL_UP;
    case AKEYCODE_CHANNEL_DOWN:
      return DomKey::CHANNEL_DOWN;
    case AKEYCODE_ZOOM_IN:
      return DomKey::ZOOM_IN;
    case AKEYCODE_ZOOM_OUT:
      return DomKey::ZOOM_OUT;
    case AKEYCODE_TV:
      return DomKey::TV;
    case AKEYCODE_GUIDE:
      return DomKey::GUIDE;
    case AKEYCODE_BOOKMARK:
      return DomKey::BROWSER_FAVORITES;
    case AKEYCODE_CAPTIONS:
      return DomKey::CLOSED_CAPTION_TOGGLE;
    case AKEYCODE_SETTINGS:
      return DomKey::SETTINGS;
    case AKEYCODE_TV_POWER:
      return DomKey::TV_POWER;
    case AKEYCODE_TV_INPUT:
      return DomKey::TV_INPUT;
    case AKEYCODE_STB_POWER:
      return DomKey::STB_POWER;
    case AKEYCODE_STB_INPUT:
      return DomKey::STB_INPUT;
    case AKEYCODE_AVR_POWER:
      return DomKey::AVR_POWER;
    case AKEYCODE_AVR_INPUT:
      return DomKey::AVR_INPUT;
    case AKEYCODE_PROG_RED:
      return DomKey::COLOR_F0_RED;
    case AKEYCODE_PROG_GREEN:
      return DomKey::COLOR_F1_GREEN;
    case AKEYCODE_PROG_YELLOW:
      return DomKey::COLOR_F2_YELLOW;
    case AKEYCODE_PROG_BLUE:
      return DomKey::COLOR_F3_BLUE;
    case AKEYCODE_APP_SWITCH:
      return DomKey::APP_SWITCH;
    case AKEYCODE_LANGUAGE_SWITCH:
      return DomKey::GROUP_NEXT;
    case AKEYCODE_MANNER_MODE:
      return DomKey::MANNER_MODE;
    case AKEYCODE_3D_MODE:
      return DomKey::TV_3D_MODE;
    case AKEYCODE_CONTACTS:
      return DomKey::LAUNCH_CONTACTS;
    case AKEYCODE_CALENDAR:
      return DomKey::LAUNCH_CALENDAR;
    case AKEYCODE_MUSIC:
      return DomKey::LAUNCH_MUSIC_PLAYER;
    case AKEYCODE_CALCULATOR:
      return DomKey::LAUNCH_CALCULATOR;
    case AKEYCODE_ZENKAKU_HANKAKU:
      return DomKey::ZENKAKU_HANKAKU;
    case AKEYCODE_EISU:
      return DomKey::EISU;
    case AKEYCODE_MUHENKAN:
      return DomKey::NON_CONVERT;
    case AKEYCODE_HENKAN:
      return DomKey::CONVERT;
    case AKEYCODE_KATAKANA_HIRAGANA:
      return DomKey::HIRAGANA_KATAKANA;
    case AKEYCODE_KANA:
      return DomKey::KANJI_MODE;
    case AKEYCODE_BRIGHTNESS_DOWN:
      return DomKey::BRIGHTNESS_DOWN;
    case AKEYCODE_BRIGHTNESS_UP:
      return DomKey::BRIGHTNESS_UP;
    case AKEYCODE_MEDIA_AUDIO_TRACK:
      return DomKey::MEDIA_AUDIO_TRACK;
    case AKEYCODE_SLEEP:
      return DomKey::STANDBY;
    case AKEYCODE_WAKEUP:
      return DomKey::WAKE_UP;
    case AKEYCODE_PAIRING:
      return DomKey::PAIRING;
    case AKEYCODE_MEDIA_TOP_MENU:
      return DomKey::MEDIA_TOP_MENU;
    case AKEYCODE_LAST_CHANNEL:
      return DomKey::MEDIA_LAST;
    case AKEYCODE_TV_DATA_SERVICE:
      return DomKey::TV_DATA_SERVICE;
    case AKEYCODE_TV_RADIO_SERVICE:
      return DomKey::TV_RADIO_SERVICE;
    case AKEYCODE_TV_TELETEXT:
      return DomKey::TELETEXT;
    case AKEYCODE_TV_NUMBER_ENTRY:
      return DomKey::TV_NUMBER_ENTRY;
    case AKEYCODE_TV_TERRESTRIAL_ANALOG:
      return DomKey::TV_TERRESTRIAL_ANALOG;
    case AKEYCODE_TV_TERRESTRIAL_DIGITAL:
      return DomKey::TV_TERRESTRIAL_DIGITAL;
    case AKEYCODE_TV_SATELLITE:
      return DomKey::TV_SATELLITE;
    case AKEYCODE_TV_SATELLITE_BS:
      return DomKey::TV_SATELLITE_BS;
    case AKEYCODE_TV_SATELLITE_CS:
      return DomKey::TV_SATELLITE_CS;
    case AKEYCODE_TV_SATELLITE_SERVICE:
      return DomKey::TV_SATELLITE_TOGGLE;
    case AKEYCODE_TV_NETWORK:
      return DomKey::TV_NETWORK;
    case AKEYCODE_TV_ANTENNA_CABLE:
      return DomKey::TV_ANTENNA_CABLE;
    case AKEYCODE_TV_INPUT_HDMI_1:
      return DomKey::TV_INPUT_HDMI1;
    case AKEYCODE_TV_INPUT_HDMI_2:
      return DomKey::TV_INPUT_HDMI2;
    case AKEYCODE_TV_INPUT_HDMI_3:
      return DomKey::TV_INPUT_HDMI3;
    case AKEYCODE_TV_INPUT_HDMI_4:
      return DomKey::TV_INPUT_HDMI4;
    case AKEYCODE_TV_INPUT_COMPOSITE_1:
      return DomKey::TV_INPUT_COMPOSITE1;
    case AKEYCODE_TV_INPUT_COMPOSITE_2:
      return DomKey::TV_INPUT_COMPOSITE2;
    case AKEYCODE_TV_INPUT_COMPONENT_1:
      return DomKey::TV_INPUT_COMPONENT1;
    case AKEYCODE_TV_INPUT_COMPONENT_2:
      return DomKey::TV_INPUT_COMPONENT2;
    case AKEYCODE_TV_INPUT_VGA_1:
      return DomKey::TV_INPUT_VGA1;
    case AKEYCODE_TV_AUDIO_DESCRIPTION:
      return DomKey::TV_AUDIO_DESCRIPTION;
    case AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP:
      return DomKey::TV_AUDIO_DESCRIPTION_MIX_UP;
    case AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN:
      return DomKey::TV_AUDIO_DESCRIPTION_MIX_DOWN;
    case AKEYCODE_TV_ZOOM_MODE:
      return DomKey::ZOOM_TOGGLE;
    case AKEYCODE_TV_CONTENTS_MENU:
      return DomKey::TV_CONTENTS_MENU;
    case AKEYCODE_TV_TIMER_PROGRAMMING:
      return DomKey::TV_TIMER;
    case AKEYCODE_HELP:
      return DomKey::HELP;
    case AKEYCODE_NAVIGATE_PREVIOUS:
      return DomKey::NAVIGATE_PREVIOUS;
    case AKEYCODE_NAVIGATE_NEXT:
      return DomKey::NAVIGATE_NEXT;
    case AKEYCODE_NAVIGATE_IN:
      return DomKey::NAVIGATE_IN;
    case AKEYCODE_NAVIGATE_OUT:
      return DomKey::NAVIGATE_OUT;
    case AKEYCODE_MEDIA_SKIP_FORWARD:
      return DomKey::MEDIA_SKIP_FORWARD;
    case AKEYCODE_MEDIA_SKIP_BACKWARD:
      return DomKey::MEDIA_SKIP_BACKWARD;
    case AKEYCODE_MEDIA_STEP_FORWARD:
      return DomKey::MEDIA_STEP_FORWARD;
    case AKEYCODE_MEDIA_STEP_BACKWARD:
      return DomKey::MEDIA_STEP_BACKWARD;
    case AKEYCODE_CUT:
      return DomKey::CUT;
    case AKEYCODE_COPY:
      return DomKey::COPY;
    case AKEYCODE_PASTE:
      return DomKey::PASTE;
    case AKEYCODE_DVR:
      return DomKey::DVR;

    // The following codes should already be handled as printable
    // character mapping.

    // case AKEYCODE_0:
    // case AKEYCODE_1:
    // case AKEYCODE_2:
    // case AKEYCODE_3:
    // case AKEYCODE_4:
    // case AKEYCODE_5:
    // case AKEYCODE_6:
    // case AKEYCODE_7:
    // case AKEYCODE_8:
    // case AKEYCODE_9:
    // case AKEYCODE_STAR:
    // case AKEYCODE_POUND:
    // case AKEYCODE_A:
    // case AKEYCODE_B:
    // case AKEYCODE_C:
    // case AKEYCODE_D:
    // case AKEYCODE_E:
    // case AKEYCODE_F:
    // case AKEYCODE_G:
    // case AKEYCODE_H:
    // case AKEYCODE_I:
    // case AKEYCODE_J:
    // case AKEYCODE_K:
    // case AKEYCODE_L:
    // case AKEYCODE_M:
    // case AKEYCODE_N:
    // case AKEYCODE_O:
    // case AKEYCODE_P:
    // case AKEYCODE_Q:
    // case AKEYCODE_R:
    // case AKEYCODE_S:
    // case AKEYCODE_T:
    // case AKEYCODE_U:
    // case AKEYCODE_V:
    // case AKEYCODE_W:
    // case AKEYCODE_X:
    // case AKEYCODE_Y:
    // case AKEYCODE_Z:
    // case AKEYCODE_COMMA:
    // case AKEYCODE_PERIOD:
    // case AKEYCODE_GRAVE:
    // case AKEYCODE_MINUS:
    // case AKEYCODE_EQUALS:
    // case AKEYCODE_LEFT_BRACKET:
    // case AKEYCODE_RIGHT_BRACKET:
    // case AKEYCODE_BACKSLASH:
    // case AKEYCODE_SEMICOLON:
    // case AKEYCODE_APOSTROPHE:
    // case AKEYCODE_SLASH:
    // case AKEYCODE_AT:
    // case AKEYCODE_NUM:
    // case AKEYCODE_NUMPAD_0:
    // case AKEYCODE_NUMPAD_1:
    // case AKEYCODE_NUMPAD_2:
    // case AKEYCODE_NUMPAD_3:
    // case AKEYCODE_NUMPAD_4:
    // case AKEYCODE_NUMPAD_5:
    // case AKEYCODE_NUMPAD_6:
    // case AKEYCODE_NUMPAD_7:
    // case AKEYCODE_NUMPAD_8:
    // case AKEYCODE_NUMPAD_9:
    // case AKEYCODE_NUMPAD_DIVIDE:
    // case AKEYCODE_NUMPAD_MULTIPLY:
    // case AKEYCODE_NUMPAD_SUBTRACT:
    // case AKEYCODE_NUMPAD_ADD:
    // case AKEYCODE_NUMPAD_DOT:
    // case AKEYCODE_NUMPAD_COMMA:
    // case AKEYCODE_NUMPAD_EQUALS:
    // case AKEYCODE_NUMPAD_LEFT_PAREN:
    // case AKEYCODE_NUMPAD_RIGHT_PAREN:
    // case AKEYCODE_SPACE:
    // case AKEYCODE_PLUS:

    // The following codes are unsupported. ie; there is no
    // applicable mapping from the Android keycode to DOM Code
    // currently.

    // case AKEYCODE_SOFT_LEFT:
    // case AKEYCODE_SOFT_RIGHT:
    // case AKEYCODE_PICTSYMBOLS:
    // case AKEYCODE_BUTTON_A:
    // case AKEYCODE_BUTTON_B:
    // case AKEYCODE_BUTTON_C:
    // case AKEYCODE_BUTTON_X:
    // case AKEYCODE_BUTTON_Y:
    // case AKEYCODE_BUTTON_Z:
    // case AKEYCODE_BUTTON_L1:
    // case AKEYCODE_BUTTON_R1:
    // case AKEYCODE_BUTTON_L2:
    // case AKEYCODE_BUTTON_R2:
    // case AKEYCODE_BUTTON_THUMBL:
    // case AKEYCODE_BUTTON_THUMBR:
    // case AKEYCODE_BUTTON_START:
    // case AKEYCODE_BUTTON_SELECT:
    // case AKEYCODE_BUTTON_MODE:
    // case AKEYCODE_WINDOW:
    // case AKEYCODE_BUTTON_1:
    // case AKEYCODE_BUTTON_2:
    // case AKEYCODE_BUTTON_3:
    // case AKEYCODE_BUTTON_4:
    // case AKEYCODE_BUTTON_5:
    // case AKEYCODE_BUTTON_6:
    // case AKEYCODE_BUTTON_7:
    // case AKEYCODE_BUTTON_8:
    // case AKEYCODE_BUTTON_9:
    // case AKEYCODE_BUTTON_10:
    // case AKEYCODE_BUTTON_11:
    // case AKEYCODE_BUTTON_12:
    // case AKEYCODE_BUTTON_13:
    // case AKEYCODE_BUTTON_14:
    // case AKEYCODE_BUTTON_15:
    // case AKEYCODE_BUTTON_16:
    // case AKEYCODE_YEN:
    // case AKEYCODE_RO:
    // case AKEYCODE_ASSIST:
    // case AKEYCODE_11:
    // case AKEYCODE_12:
    // case AKEYCODE_TV_DATA:
    // case AKEYCODE_VOICE_ASSIST:
    // case AKEYCODE_TV_MEDIA_CONTEXT_MENU:
  }
}

}  // namespace

DomKey GetDomKeyFromAndroidEvent(int keycode, int unicode_character) {
  // Android maps ENTER to '\n'; but the DOM maps it to '\r'; ensure
  // the difference in mapping is mitigated.
  if (unicode_character == '\n')
    unicode_character = '\r';

  // Android generates unicode_characters with the high bit on indicating
  // the key is a combining character.
  if (unicode_character & kCombiningAccent) {
    return DomKey::DeadKeyFromCombiningCharacter(unicode_character &
                                                 kCombiningAccentMask);
  }

  // |unicode_character| is the character generated by applying the
  // keyboard layout and modifiers. When the |unicode_character|
  // is non-zero then a printable character has been successfully
  // mapped; otherwise generate the DomKey from the keycode.
  if (unicode_character)
    return DomKey::FromCharacter(unicode_character);
  return GetDomKeyFromAndroidKeycode(keycode);
}

KeyboardCode KeyboardCodeFromAndroidKeyCode(int keycode) {
  // Does not provide all key codes, and does not handle all keys.
  switch (keycode) {
    case AKEYCODE_DEL:
      return VKEY_BACK;
    case AKEYCODE_TAB:
      return VKEY_TAB;
    case AKEYCODE_CLEAR:
      return VKEY_CLEAR;
    case AKEYCODE_DPAD_CENTER:
    case AKEYCODE_ENTER:
      return VKEY_RETURN;
    case AKEYCODE_SHIFT_LEFT:
      return VKEY_LSHIFT;
    case AKEYCODE_SHIFT_RIGHT:
      return VKEY_RSHIFT;
    case AKEYCODE_BACK:
      return VKEY_BROWSER_BACK;
    case AKEYCODE_FORWARD:
      return VKEY_BROWSER_FORWARD;
    case AKEYCODE_SPACE:
      return VKEY_SPACE;
    case AKEYCODE_MOVE_HOME:
      return VKEY_HOME;
    case AKEYCODE_DPAD_LEFT:
      return VKEY_LEFT;
    case AKEYCODE_DPAD_UP:
      return VKEY_UP;
    case AKEYCODE_DPAD_RIGHT:
      return VKEY_RIGHT;
    case AKEYCODE_DPAD_DOWN:
      return VKEY_DOWN;
    case AKEYCODE_0:
      return VKEY_0;
    case AKEYCODE_1:
      return VKEY_1;
    case AKEYCODE_2:
      return VKEY_2;
    case AKEYCODE_3:
      return VKEY_3;
    case AKEYCODE_4:
      return VKEY_4;
    case AKEYCODE_5:
      return VKEY_5;
    case AKEYCODE_6:
      return VKEY_6;
    case AKEYCODE_7:
      return VKEY_7;
    case AKEYCODE_8:
      return VKEY_8;
    case AKEYCODE_9:
      return VKEY_9;
    case AKEYCODE_A:
      return VKEY_A;
    case AKEYCODE_B:
      return VKEY_B;
    case AKEYCODE_C:
      return VKEY_C;
    case AKEYCODE_D:
      return VKEY_D;
    case AKEYCODE_E:
      return VKEY_E;
    case AKEYCODE_F:
      return VKEY_F;
    case AKEYCODE_G:
      return VKEY_G;
    case AKEYCODE_H:
      return VKEY_H;
    case AKEYCODE_I:
      return VKEY_I;
    case AKEYCODE_J:
      return VKEY_J;
    case AKEYCODE_K:
      return VKEY_K;
    case AKEYCODE_L:
      return VKEY_L;
    case AKEYCODE_M:
      return VKEY_M;
    case AKEYCODE_N:
      return VKEY_N;
    case AKEYCODE_O:
      return VKEY_O;
    case AKEYCODE_P:
      return VKEY_P;
    case AKEYCODE_Q:
      return VKEY_Q;
    case AKEYCODE_R:
      return VKEY_R;
    case AKEYCODE_S:
      return VKEY_S;
    case AKEYCODE_T:
      return VKEY_T;
    case AKEYCODE_U:
      return VKEY_U;
    case AKEYCODE_V:
      return VKEY_V;
    case AKEYCODE_W:
      return VKEY_W;
    case AKEYCODE_X:
      return VKEY_X;
    case AKEYCODE_Y:
      return VKEY_Y;
    case AKEYCODE_Z:
      return VKEY_Z;
    case AKEYCODE_VOLUME_DOWN:
      return VKEY_VOLUME_DOWN;
    case AKEYCODE_VOLUME_UP:
      return VKEY_VOLUME_UP;
    case AKEYCODE_MEDIA_NEXT:
      return VKEY_MEDIA_NEXT_TRACK;
    case AKEYCODE_MEDIA_PREVIOUS:
      return VKEY_MEDIA_PREV_TRACK;
    case AKEYCODE_MEDIA_STOP:
      return VKEY_MEDIA_STOP;
    case AKEYCODE_MEDIA_PAUSE:
      return VKEY_MEDIA_PLAY_PAUSE;
    // Colon key.
    case AKEYCODE_SEMICOLON:
      return VKEY_OEM_1;
    case AKEYCODE_COMMA:
      return VKEY_OEM_COMMA;
    case AKEYCODE_MINUS:
      return VKEY_OEM_MINUS;
    case AKEYCODE_EQUALS:
      return VKEY_OEM_PLUS;
    case AKEYCODE_PERIOD:
      return VKEY_OEM_PERIOD;
    case AKEYCODE_SLASH:
      return VKEY_OEM_2;
    case AKEYCODE_LEFT_BRACKET:
      return VKEY_OEM_4;
    case AKEYCODE_BACKSLASH:
      return VKEY_OEM_5;
    case AKEYCODE_RIGHT_BRACKET:
      return VKEY_OEM_6;
    case AKEYCODE_MUTE:
    case AKEYCODE_VOLUME_MUTE:
      return VKEY_VOLUME_MUTE;
    case AKEYCODE_ESCAPE:
      return VKEY_ESCAPE;
    case AKEYCODE_MEDIA_PLAY:
    case AKEYCODE_MEDIA_PLAY_PAUSE:
      return VKEY_MEDIA_PLAY_PAUSE;
    case AKEYCODE_MOVE_END:
      return VKEY_END;
    case AKEYCODE_ALT_LEFT:
      return VKEY_LMENU;
    case AKEYCODE_ALT_RIGHT:
      return VKEY_RMENU;
    case AKEYCODE_GRAVE:
      return VKEY_OEM_3;
    case AKEYCODE_APOSTROPHE:
      return VKEY_OEM_3;
    case AKEYCODE_MEDIA_REWIND:
      return VKEY_OEM_103;
    case AKEYCODE_MEDIA_FAST_FORWARD:
      return VKEY_OEM_104;
    case AKEYCODE_PAGE_UP:
      return VKEY_PRIOR;
    case AKEYCODE_PAGE_DOWN:
      return VKEY_NEXT;
    case AKEYCODE_FORWARD_DEL:
      return VKEY_DELETE;
    case AKEYCODE_CTRL_LEFT:
      return VKEY_LCONTROL;
    case AKEYCODE_CTRL_RIGHT:
      return VKEY_RCONTROL;
    case AKEYCODE_CAPS_LOCK:
      return VKEY_CAPITAL;
    case AKEYCODE_SCROLL_LOCK:
      return VKEY_SCROLL;
    case AKEYCODE_META_LEFT:
      return VKEY_LWIN;
    case AKEYCODE_META_RIGHT:
      return VKEY_RWIN;
    case AKEYCODE_BREAK:
      return VKEY_PAUSE;
    case AKEYCODE_INSERT:
      return VKEY_INSERT;
    case AKEYCODE_F1:
      return VKEY_F1;
    case AKEYCODE_F2:
      return VKEY_F2;
    case AKEYCODE_F3:
      return VKEY_F3;
    case AKEYCODE_F4:
      return VKEY_F4;
    case AKEYCODE_F5:
      return VKEY_F5;
    case AKEYCODE_F6:
      return VKEY_F6;
    case AKEYCODE_F7:
      return VKEY_F7;
    case AKEYCODE_F8:
      return VKEY_F8;
    case AKEYCODE_F9:
      return VKEY_F9;
    case AKEYCODE_F10:
      return VKEY_F10;
    case AKEYCODE_F11:
      return VKEY_F11;
    case AKEYCODE_F12:
      return VKEY_F12;
    case AKEYCODE_NUM_LOCK:
      return VKEY_NUMLOCK;
    case AKEYCODE_NUMPAD_0:
      return VKEY_NUMPAD0;
    case AKEYCODE_NUMPAD_1:
      return VKEY_NUMPAD1;
    case AKEYCODE_NUMPAD_2:
      return VKEY_NUMPAD2;
    case AKEYCODE_NUMPAD_3:
      return VKEY_NUMPAD3;
    case AKEYCODE_NUMPAD_4:
      return VKEY_NUMPAD4;
    case AKEYCODE_NUMPAD_5:
      return VKEY_NUMPAD5;
    case AKEYCODE_NUMPAD_6:
      return VKEY_NUMPAD6;
    case AKEYCODE_NUMPAD_7:
      return VKEY_NUMPAD7;
    case AKEYCODE_NUMPAD_8:
      return VKEY_NUMPAD8;
    case AKEYCODE_NUMPAD_9:
      return VKEY_NUMPAD9;
    case AKEYCODE_NUMPAD_DIVIDE:
      return VKEY_DIVIDE;
    case AKEYCODE_NUMPAD_MULTIPLY:
      return VKEY_MULTIPLY;
    case AKEYCODE_NUMPAD_SUBTRACT:
      return VKEY_SUBTRACT;
    case AKEYCODE_NUMPAD_ADD:
      return VKEY_ADD;
    case AKEYCODE_NUMPAD_DOT:
      return VKEY_DECIMAL;
    case AKEYCODE_CHANNEL_UP:
      return VKEY_PRIOR;
    case AKEYCODE_CHANNEL_DOWN:
      return VKEY_NEXT;
    default:
      return VKEY_UNKNOWN;
  }
}

}  // namespace ui
