// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/keyboard_code_conversion_xkb.h"
#include "ui/gfx/x/keysyms/keysyms.h"

namespace ui {

// We support dead keys beyond those with predefined keysym names,
// expressed as numeric constants with |kDeadKeyFlag| set.
// Xkbcommon accepts and does not use any bits in 0x0E000000.
//
constexpr xkb_keysym_t kDeadKeyFlag = 0x08000000;
constexpr int32_t kUnicodeMax = 0x10FFFF;

DomKey NonPrintableXKeySymToDomKey(xkb_keysym_t keysym) {
  switch (keysym) {
    case XKB_KEY_BackSpace:
      return DomKey::BACKSPACE;
    case XKB_KEY_Tab:
    case XKB_KEY_KP_Tab:
    case XKB_KEY_ISO_Left_Tab:
      return DomKey::TAB;
    case XKB_KEY_Clear:
    case XKB_KEY_KP_Begin:
    case XKB_KEY_XF86Clear:
      return DomKey::CLEAR;
    case XKB_KEY_Return:
    case XKB_KEY_KP_Enter:
      return DomKey::ENTER;
    case XKB_KEY_Linefeed:
      return DomKey::ENTER;
    case XKB_KEY_Pause:
      return DomKey::PAUSE;
    case XKB_KEY_Scroll_Lock:
      return DomKey::SCROLL_LOCK;
    case XKB_KEY_Escape:
      return DomKey::ESCAPE;
    case XKB_KEY_Multi_key:
      return DomKey::COMPOSE;
    case XKB_KEY_Kanji:
      return DomKey::KANJI_MODE;
    case XKB_KEY_Muhenkan:
      return DomKey::NON_CONVERT;
    case XKB_KEY_Henkan_Mode:
      return DomKey::CONVERT;
    case XKB_KEY_Romaji:
      return DomKey::ROMAJI;
    case XKB_KEY_Hiragana:
      return DomKey::HIRAGANA;
    case XKB_KEY_Katakana:
      return DomKey::KATAKANA;
    case XKB_KEY_Hiragana_Katakana:
      return DomKey::HIRAGANA_KATAKANA;
    case XKB_KEY_Zenkaku:
      return DomKey::ZENKAKU;
    case XKB_KEY_Hankaku:
      return DomKey::HANKAKU;
    case XKB_KEY_Zenkaku_Hankaku:
      return DomKey::ZENKAKU_HANKAKU;
    case XKB_KEY_Kana_Lock:
      return DomKey::KANA_MODE;
    case XKB_KEY_Eisu_Shift:
    case XKB_KEY_Eisu_toggle:
      return DomKey::ALPHANUMERIC;
    case XKB_KEY_Hangul:
      return DomKey::HANGUL_MODE;
    case XKB_KEY_Hangul_Hanja:
      return DomKey::HANJA_MODE;
    case XKB_KEY_Codeinput:
      return DomKey::CODE_INPUT;
    case XKB_KEY_SingleCandidate:
      return DomKey::SINGLE_CANDIDATE;
    case XKB_KEY_MultipleCandidate:
      return DomKey::ALL_CANDIDATES;
    case XKB_KEY_PreviousCandidate:
      return DomKey::PREVIOUS_CANDIDATE;
    case XKB_KEY_Home:
    case XKB_KEY_KP_Home:
      return DomKey::HOME;
    case XKB_KEY_Left:
    case XKB_KEY_KP_Left:
      return DomKey::ARROW_LEFT;
    case XKB_KEY_Up:
    case XKB_KEY_KP_Up:
      return DomKey::ARROW_UP;
    case XKB_KEY_Right:
    case XKB_KEY_KP_Right:
      return DomKey::ARROW_RIGHT;
    case XKB_KEY_Down:
    case XKB_KEY_KP_Down:
      return DomKey::ARROW_DOWN;
    case XKB_KEY_Prior:
    case XKB_KEY_KP_Prior:
      return DomKey::PAGE_UP;
    case XKB_KEY_Next:
    case XKB_KEY_KP_Next:
    case XKB_KEY_XF86ScrollDown:
      return DomKey::PAGE_DOWN;
    case XKB_KEY_End:
    case XKB_KEY_KP_End:
    case XKB_KEY_XF86ScrollUp:
      return DomKey::END;
    case XKB_KEY_Select:
      return DomKey::SELECT;
    case XKB_KEY_Print:
#if BUILDFLAG(IS_CHROMEOS)
      // On ChromeOS KEY_PRINT really means print not print screen.
      return DomKey::PRINT;
#else   // !BUILDFLAG(IS_CHROMEOS)
      // For legacy reasons XKB and Linux treat Print and PrintScreen as
      // PrintScreen. See https://crbug.com/683097.
      return DomKey::PRINT_SCREEN;
#endif  // !BUILDFLAG(IS_CHROMEOS)
    case XKB_KEY_3270_PrintScreen:
      return DomKey::PRINT_SCREEN;
    case XKB_KEY_Execute:
      return DomKey::EXECUTE;
    case XKB_KEY_Insert:
    case XKB_KEY_KP_Insert:
      return DomKey::INSERT;
    case XKB_KEY_Undo:
      return DomKey::UNDO;
    case XKB_KEY_Redo:
      return DomKey::REDO;
    case XKB_KEY_Menu:
      return DomKey::CONTEXT_MENU;
    case XKB_KEY_Find:
      return DomKey::FIND;
    case XKB_KEY_Cancel:
      return DomKey::CANCEL;
    case XKB_KEY_Help:
      return DomKey::HELP;
    case XKB_KEY_Break:
    case XKB_KEY_3270_Attn:
      return DomKey::ATTN;
    case XKB_KEY_Mode_switch:
      return DomKey::MODE_CHANGE;
    case XKB_KEY_Num_Lock:
      return DomKey::NUM_LOCK;
    case XKB_KEY_F1:
    case XKB_KEY_KP_F1:
      return DomKey::F1;
    case XKB_KEY_F2:
    case XKB_KEY_KP_F2:
      return DomKey::F2;
    case XKB_KEY_F3:
    case XKB_KEY_KP_F3:
      return DomKey::F3;
    case XKB_KEY_F4:
    case XKB_KEY_KP_F4:
      return DomKey::F4;
    case XKB_KEY_F5:
      return DomKey::F5;
    case XKB_KEY_F6:
      return DomKey::F6;
    case XKB_KEY_F7:
      return DomKey::F7;
    case XKB_KEY_F8:
      return DomKey::F8;
    case XKB_KEY_F9:
      return DomKey::F9;
    case XKB_KEY_F10:
      return DomKey::F10;
    case XKB_KEY_F11:
      return DomKey::F11;
    case XKB_KEY_F12:
      return DomKey::F12;
    case XKB_KEY_XF86Tools:  // XKB 'inet' mapping of F13
    case XKB_KEY_F13:
      return DomKey::F13;
    case XKB_KEY_F14:
    case XKB_KEY_XF86Launch5:  // XKB 'inet' mapping of F14
      return DomKey::F14;
    case XKB_KEY_F15:
    case XKB_KEY_XF86Launch6:  // XKB 'inet' mapping of F15
      return DomKey::F15;
    case XKB_KEY_F16:
    case XKB_KEY_XF86Launch7:  // XKB 'inet' mapping of F16
      return DomKey::F16;
    case XKB_KEY_F17:
    case XKB_KEY_XF86Launch8:  // XKB 'inet' mapping of F17
      return DomKey::F17;
    case XKB_KEY_F18:
    case XKB_KEY_XF86Launch9:  // XKB 'inet' mapping of F18
      return DomKey::F18;
    case XKB_KEY_F19:
      return DomKey::F19;
    case XKB_KEY_F20:
      return DomKey::F20;
    case XKB_KEY_F21:
      return DomKey::F21;
    case XKB_KEY_F22:
      return DomKey::F22;
    case XKB_KEY_F23:
      return DomKey::F23;
    case XKB_KEY_F24:
      return DomKey::F24;
    case XKB_KEY_Shift_L:
    case XKB_KEY_Shift_R:
      return DomKey::SHIFT;
    case XKB_KEY_Control_L:
    case XKB_KEY_Control_R:
      return DomKey::CONTROL;
    case XKB_KEY_Caps_Lock:
      return DomKey::CAPS_LOCK;
#if BUILDFLAG(IS_CHROMEOS_ASH)
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R:
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
      // The Shift+Alt generates a KeySym for the Meta key. On ChromeOS the Meta
      // key is not used and we should still get the Alt key. crbug.com/541468.
      return DomKey::ALT;
#else
    case XKB_KEY_Meta_L:
    case XKB_KEY_Meta_R:
      return DomKey::META;
    case XKB_KEY_Alt_L:
    case XKB_KEY_Alt_R:
      return DomKey::ALT;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
    case XKB_KEY_Super_L:
    case XKB_KEY_Super_R:
      return DomKey::META;
    case XKB_KEY_Hyper_L:
    case XKB_KEY_Hyper_R:
      return DomKey::HYPER;
    case XKB_KEY_Delete:
      return DomKey::DEL;
    case XKB_KEY_SunProps:
      return DomKey::PROPS;
    case XKB_KEY_XF86Next_VMode:
      return DomKey::VIDEO_MODE_NEXT;
    case XKB_KEY_XF86MonBrightnessUp:
      return DomKey::BRIGHTNESS_UP;
    case XKB_KEY_XF86MonBrightnessDown:
      return DomKey::BRIGHTNESS_DOWN;
    case XKB_KEY_XF86Standby:
    case XKB_KEY_XF86Sleep:
    case XKB_KEY_XF86Suspend:
      return DomKey::STANDBY;
    case XKB_KEY_XF86AudioLowerVolume:
      return DomKey::AUDIO_VOLUME_DOWN;
    case XKB_KEY_XF86AudioMute:
      return DomKey::AUDIO_VOLUME_MUTE;
    case XKB_KEY_XF86AudioRaiseVolume:
      return DomKey::AUDIO_VOLUME_UP;
    case XKB_KEY_XF86AudioPlay:
      return DomKey::MEDIA_PLAY_PAUSE;
    case XKB_KEY_XF86AudioStop:
      return DomKey::MEDIA_STOP;
    case XKB_KEY_XF86AudioPrev:
      return DomKey::MEDIA_TRACK_PREVIOUS;
    case XKB_KEY_XF86AudioNext:
      return DomKey::MEDIA_TRACK_NEXT;
    case XKB_KEY_XF86HomePage:
      return DomKey::BROWSER_HOME;
    case XKB_KEY_XF86Mail:
      return DomKey::LAUNCH_MAIL;
    case XKB_KEY_XF86Search:
      return DomKey::BROWSER_SEARCH;
    case XKB_KEY_XF86AudioRecord:
      return DomKey::MEDIA_RECORD;
    case XKB_KEY_XF86Calculator:
      return DomKey::LAUNCH_CALCULATOR;
    case XKB_KEY_XF86Calendar:
      return DomKey::LAUNCH_CALENDAR;
    case XKB_KEY_XF86Back:
      return DomKey::BROWSER_BACK;
    case XKB_KEY_XF86Forward:
      return DomKey::BROWSER_FORWARD;
    case XKB_KEY_XF86Stop:
      return DomKey::BROWSER_STOP;
    case XKB_KEY_XF86Refresh:
    case XKB_KEY_XF86Reload:
      return DomKey::BROWSER_REFRESH;
    case XKB_KEY_XF86PowerOff:
      return DomKey::POWER_OFF;
    case XKB_KEY_XF86WakeUp:
      return DomKey::WAKE_UP;
    case XKB_KEY_XF86Eject:
      return DomKey::EJECT;
    case XKB_KEY_XF86ScreenSaver:
      return DomKey::LAUNCH_SCREEN_SAVER;
    case XKB_KEY_XF86WWW:
      return DomKey::LAUNCH_WEB_BROWSER;
    case XKB_KEY_XF86Favorites:
      return DomKey::BROWSER_FAVORITES;
    case XKB_KEY_XF86AudioPause:
      return DomKey::MEDIA_PAUSE;
    case XKB_KEY_XF86AudioMedia:
    case XKB_KEY_XF86Music:
      return DomKey::LAUNCH_MUSIC_PLAYER;
    case XKB_KEY_XF86MyComputer:
    case XKB_KEY_XF86Explorer:
      return DomKey::LAUNCH_MY_COMPUTER;
    case XKB_KEY_XF86AudioRewind:
      return DomKey::MEDIA_REWIND;
    case XKB_KEY_XF86CD:
    case XKB_KEY_XF86Video:
      return DomKey::LAUNCH_MEDIA_PLAYER;
    case XKB_KEY_XF86Close:
      return DomKey::CLOSE;
    case XKB_KEY_XF86Copy:
    case XKB_KEY_SunCopy:
      return DomKey::COPY;
    case XKB_KEY_XF86Cut:
    case XKB_KEY_SunCut:
      return DomKey::CUT;
    case XKB_KEY_XF86Display:
      return DomKey::DISPLAY_SWAP;
    case XKB_KEY_XF86Excel:
      return DomKey::LAUNCH_SPREADSHEET;
    case XKB_KEY_XF86LogOff:
      return DomKey::LOG_OFF;
    case XKB_KEY_XF86New:
      return DomKey::NEW;
    case XKB_KEY_XF86Open:
    case XKB_KEY_SunOpen:
      return DomKey::OPEN;
    case XKB_KEY_XF86Paste:
    case XKB_KEY_SunPaste:
      return DomKey::PASTE;
    case XKB_KEY_XF86Reply:
      return DomKey::MAIL_REPLY;
    case XKB_KEY_XF86Save:
      return DomKey::SAVE;
    case XKB_KEY_XF86Send:
      return DomKey::MAIL_SEND;
    case XKB_KEY_XF86Spell:
      return DomKey::SPELL_CHECK;
    case XKB_KEY_XF86SplitScreen:
      return DomKey::SPLIT_SCREEN_TOGGLE;
    case XKB_KEY_XF86Word:
    case XKB_KEY_XF86OfficeHome:
      return DomKey::LAUNCH_WORD_PROCESSOR;
    case XKB_KEY_XF86ZoomIn:
      return DomKey::ZOOM_IN;
    case XKB_KEY_XF86ZoomOut:
      return DomKey::ZOOM_OUT;
    case XKB_KEY_XF86WebCam:
      return DomKey::LAUNCH_WEB_CAM;
    case XKB_KEY_XF86MailForward:
      return DomKey::MAIL_FORWARD;
    case XKB_KEY_XF86AudioForward:
      return DomKey::MEDIA_FAST_FORWARD;
    case XKB_KEY_XF86AudioRandomPlay:
      return DomKey::RANDOM_TOGGLE;
    case XKB_KEY_XF86Subtitle:
      return DomKey::SUBTITLE;
    case XKB_KEY_XF86Hibernate:
      return DomKey::HIBERNATE;
    case XKB_KEY_3270_EraseEOF:
      return DomKey::ERASE_EOF;
    case XKB_KEY_3270_Play:
      return DomKey::PLAY;
    case XKB_KEY_3270_ExSelect:
      return DomKey::EX_SEL;
    case XKB_KEY_3270_CursorSelect:
      return DomKey::CR_SEL;
    case XKB_KEY_ISO_Level3_Shift:
      return DomKey::ALT_GRAPH;
    case XKB_KEY_ISO_Level3_Latch:
      return DomKey::ALT_GRAPH_LATCH;
    case XKB_KEY_ISO_Level5_Shift:
      return DomKey::SHIFT_LEVEL5;
    case XKB_KEY_ISO_Next_Group:
      return DomKey::GROUP_NEXT;
    case XKB_KEY_ISO_Prev_Group:
      return DomKey::GROUP_PREVIOUS;
    case XKB_KEY_ISO_First_Group:
      return DomKey::GROUP_FIRST;
    case XKB_KEY_ISO_Last_Group:
      return DomKey::GROUP_LAST;
    case XKB_KEY_dead_grave:
      // combining grave accent
      return DomKey::DeadKeyFromCombiningCharacter(0x0300);
    case XKB_KEY_dead_acute:
      // combining acute accent
      return DomKey::DeadKeyFromCombiningCharacter(0x0301);
    case XKB_KEY_dead_circumflex:
      // combining circumflex accent
      return DomKey::DeadKeyFromCombiningCharacter(0x0302);
    case XKB_KEY_dead_tilde:
      // combining tilde
      return DomKey::DeadKeyFromCombiningCharacter(0x0303);
    case XKB_KEY_dead_macron:
      // combining macron
      return DomKey::DeadKeyFromCombiningCharacter(0x0304);
    case XKB_KEY_dead_breve:
      // combining breve
      return DomKey::DeadKeyFromCombiningCharacter(0x0306);
    case XKB_KEY_dead_abovedot:
      // combining dot above
      return DomKey::DeadKeyFromCombiningCharacter(0x0307);
    case XKB_KEY_dead_diaeresis:
      // combining diaeresis
      return DomKey::DeadKeyFromCombiningCharacter(0x0308);
    case XKB_KEY_dead_abovering:
      // combining ring above
      return DomKey::DeadKeyFromCombiningCharacter(0x030A);
    case XKB_KEY_dead_doubleacute:
      // combining double acute accent
      return DomKey::DeadKeyFromCombiningCharacter(0x030B);
    case XKB_KEY_dead_caron:
      // combining caron
      return DomKey::DeadKeyFromCombiningCharacter(0x030C);
    case XKB_KEY_dead_cedilla:
      // combining cedilla
      return DomKey::DeadKeyFromCombiningCharacter(0x0327);
    case XKB_KEY_dead_ogonek:
      // combining ogonek
      return DomKey::DeadKeyFromCombiningCharacter(0x0328);
    case XKB_KEY_dead_iota:
      // combining greek ypogegrammeni
      return DomKey::DeadKeyFromCombiningCharacter(0x0345);
    case XKB_KEY_dead_voiced_sound:
      // combining voiced sound mark
      return DomKey::DeadKeyFromCombiningCharacter(0x3099);
    case XKB_KEY_dead_semivoiced_sound:
      // combining semi-voiced sound mark
      return DomKey::DeadKeyFromCombiningCharacter(0x309A);
    case XKB_KEY_dead_belowdot:
      // combining dot below
      return DomKey::DeadKeyFromCombiningCharacter(0x0323);
    case XKB_KEY_dead_hook:
      // combining hook above
      return DomKey::DeadKeyFromCombiningCharacter(0x0309);
    case XKB_KEY_dead_horn:
      // combining horn
      return DomKey::DeadKeyFromCombiningCharacter(0x031B);
    case XKB_KEY_dead_stroke:
      // combining long solidus overlay
      return DomKey::DeadKeyFromCombiningCharacter(0x0338);
    case XKB_KEY_dead_abovecomma:
      // combining comma above
      return DomKey::DeadKeyFromCombiningCharacter(0x0313);
    case XKB_KEY_dead_abovereversedcomma:
      // combining reversed comma above
      return DomKey::DeadKeyFromCombiningCharacter(0x0314);
    case XKB_KEY_dead_doublegrave:
      // combining double grave accent
      return DomKey::DeadKeyFromCombiningCharacter(0x030F);
    case XKB_KEY_dead_belowring:
      // combining ring below
      return DomKey::DeadKeyFromCombiningCharacter(0x0325);
    case XKB_KEY_dead_belowmacron:
      // combining macron below
      return DomKey::DeadKeyFromCombiningCharacter(0x0331);
    case XKB_KEY_dead_belowcircumflex:
      // combining circumflex accent below
      return DomKey::DeadKeyFromCombiningCharacter(0x032D);
    case XKB_KEY_dead_belowtilde:
      // combining tilde below
      return DomKey::DeadKeyFromCombiningCharacter(0x0330);
    case XKB_KEY_dead_belowbreve:
      // combining breve below
      return DomKey::DeadKeyFromCombiningCharacter(0x032E);
    case XKB_KEY_dead_belowdiaeresis:
      // combining diaeresis below
      return DomKey::DeadKeyFromCombiningCharacter(0x0324);
    case XKB_KEY_dead_invertedbreve:
      // combining inverted breve
      return DomKey::DeadKeyFromCombiningCharacter(0x0311);
    case XKB_KEY_dead_belowcomma:
      // combining comma below
      return DomKey::DeadKeyFromCombiningCharacter(0x0326);
    case XKB_KEY_dead_currency:
      // currency sign
      return DomKey::DeadKeyFromCombiningCharacter(0x00A4);
    case XKB_KEY_dead_greek:
      // greek question mark
      return DomKey::DeadKeyFromCombiningCharacter(0x037E);
    default:
      if (keysym & kDeadKeyFlag) {
        int32_t character = keysym & ~kDeadKeyFlag;
        if ((character >= 0) && (character <= kUnicodeMax)) {
          return DomKey::DeadKeyFromCombiningCharacter(character);
        }
      }
      return DomKey::NONE;
  }
}
DomKey XKeySymToDomKey(xkb_keysym_t keysym, char16_t character) {
  DomKey dom_key = NonPrintableXKeySymToDomKey(keysym);
  if (dom_key != DomKey::NONE)
    return dom_key;
  return DomKey::FromCharacter(character);
}

}  // namespace ui
