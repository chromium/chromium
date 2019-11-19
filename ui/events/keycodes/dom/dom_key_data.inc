// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file has no header guard because it is explicily intended
// to be included more than once with different definitions of the
// macros DOM_KEY_MAP and DOM_KEY_MAP_DECLARATION.

// This is a table of DOM Level 3 .key values, used to generate identifiers
// (enums) and string tables. These names are defined by:
//
//   [0] DOM Level 3 KeyboardEvent key Values,
//       http://www.w3.org/TR/uievents-key/
//       (current as of W3C Working Draft 15 December 2015)
//
// That reference should be consulted on the meaning and usage of these
// values; this file does not attempt to describe that.
//
// The names given in the table here directly match the DOM Level 3 value,
// and for the most part the corresponding enum identifier is generated from
// the name by converting W3C's CamelCaseStyle to Chromium's C_MACRO_STYLE.
//
// NOTE: Some uses of this data expect that encodings remain fixed,
// so existing key values should not be changed.

DOM_KEY_MAP_DECLARATION {

#if defined(OS_ANDROID)
#define DOM_KEY_MAP_ANDROID
#include "ui/events/keycodes/dom/keycode_conversion_data_android_generated.inc"
#undef DOM_KEY_MAP_ANDROID
#endif

  // To avoid ambiguities, a small number of .key values that do have
  // standard names are encoded identical to corresponding Unicode characters;
  // for example, DomKey('\t') == DomKey::TAB.
  //
  //          Key                     Enum        Unicode code point
  DOM_KEY_UNI("Backspace",            BACKSPACE,  0x0008),
  DOM_KEY_UNI("Tab",                  TAB,        0x0009),
  DOM_KEY_UNI("Enter",                ENTER,      0x000D),
  DOM_KEY_UNI("Escape",               ESCAPE,     0x001B),
  // The enum constant for 'Delete' (Forward Delete) is named DEL because
  // DELETE conflicts with an unscoped declaration in Windows' <windows.h>.
  DOM_KEY_UNI("Delete",               DEL,        0x007F),

  // =========================================================
  // Special Key Values
  // http://www.w3.org/TR/uievents-key/#keys-special
  // =========================================================

  // Unable to identify another key value
  DOM_KEY_MAP("Unidentified",         UNIDENTIFIED,             0x0001),

  // ==========================================================
  // Modifier Keys
  // http://www.w3.org/TR/uievents-key/#keys-modifier
  // ==========================================================

  //          Key                     Enum                      Value
  //          "Accel" is never returned in a key event.
  DOM_KEY_MAP("Accel",                ACCEL,                    0x0101),
  DOM_KEY_MAP("Alt",                  ALT,                      0x0102),
  DOM_KEY_MAP("AltGraph",             ALT_GRAPH,                0x0103),
  DOM_KEY_MAP("CapsLock",             CAPS_LOCK,                0x0104),
  DOM_KEY_MAP("Control",              CONTROL,                  0x0105),
  DOM_KEY_MAP("Fn",                   FN,                       0x0106),
  DOM_KEY_MAP("FnLock",               FN_LOCK,                  0x0107),
  DOM_KEY_MAP("Hyper",                HYPER,                    0x0108),
  DOM_KEY_MAP("Meta",                 META,                     0x0109),
  DOM_KEY_MAP("NumLock",              NUM_LOCK,                 0x010A),
  //          "OS" was removed from the spec.
  //         ("OS",                   OS,                       0x010B),
  DOM_KEY_MAP("ScrollLock",           SCROLL_LOCK,              0x010C),
  DOM_KEY_MAP("Shift",                SHIFT,                    0x010D),
  DOM_KEY_MAP("Super",                SUPER,                    0x010E),
  DOM_KEY_MAP("Symbol",               SYMBOL,                   0x010F),
  DOM_KEY_MAP("SymbolLock",           SYMBOL_LOCK,              0x0110),

  // Non-standard value corresponding to XKB keysym ISO_Level5_Shift,
  // an additional printable-character modifier like Shift and AltGraph,
  // used by such layouts as Neo (German) and Multix (French-Canadian).
  //          Key                     Enum                      Value
  DOM_KEY_MAP("ShiftLevel5",          SHIFT_LEVEL5,             0x0111),

  // Non-standard value representing a latching-modifier key in ui::KeyEvent.
  // This is present on some keyboard layouts (Latvian, Tibetan, Cameroon),
  // and applies the modifier to the next (non-modifier) key.
  // These events are consumed internally and not exposed to the web,
  // either directly or as synthetic modifier up/down events.
  //          Key                     Enum                      Value
  DOM_KEY_MAP(".AltGraphLatch",       ALT_GRAPH_LATCH,          0x0112),

  // ============================================================
  // Whitespace Keys
  // http://www.w3.org/TR/uievents-key/#keys-whitespace
  // ============================================================

  //          Key                     Enum                      Value
  //          "Enter" is encoded in the Unicode space (at the top of this list).
  //          "Separator" was removed from the spec.
  //         ("Separator",            SEPARATOR,                0x0201),
  //          "Tab" is encoded in the Unicode space.

  // ============================================================
  // Navigation Keys
  // http://www.w3.org/TR/uievents-key/#keys-navigation
  // ============================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("ArrowDown",            ARROW_DOWN,               0x0301),
  DOM_KEY_MAP("ArrowLeft",            ARROW_LEFT,               0x0302),
  DOM_KEY_MAP("ArrowRight",           ARROW_RIGHT,              0x0303),
  DOM_KEY_MAP("ArrowUp",              ARROW_UP,                 0x0304),
  DOM_KEY_MAP("End",                  END,                      0x0305),
  DOM_KEY_MAP("Home",                 HOME,                     0x0306),
  DOM_KEY_MAP("PageDown",             PAGE_DOWN,                0x0307),
  DOM_KEY_MAP("PageUp",               PAGE_UP,                  0x0308),

  // =========================================================
  // Editing Keys
  // http://www.w3.org/TR/uievents-key/#keys-editing
  // =========================================================

  //          Key                     Enum                      Value
  //          "Backspace" is encoded in the Unicode space.
  DOM_KEY_MAP("Clear",                CLEAR,                    0x0401),
  DOM_KEY_MAP("Copy",                 COPY,                     0x0402),
 //           "CrSel" is short for "Cursor Select":
  DOM_KEY_MAP("CrSel",                CR_SEL,                   0x0403),
  DOM_KEY_MAP("Cut",                  CUT,                      0x0404),
  //          "Delete" is encoded in the Unicode space.
 //           "EraseEof" is short for "Erase to End of Field":
  DOM_KEY_MAP("EraseEof",             ERASE_EOF,                0x0405),
 //           "ExSel" is short for "Extend Selection":
  DOM_KEY_MAP("ExSel",                EX_SEL,                   0x0406),
  DOM_KEY_MAP("Insert",               INSERT,                   0x0407),
  DOM_KEY_MAP("Paste",                PASTE,                    0x0408),
  DOM_KEY_MAP("Redo",                 REDO,                     0x0409),
  DOM_KEY_MAP("Undo",                 UNDO,                     0x040A),

  // ====================================================
  // UI Keys
  // http://www.w3.org/TR/uievents-key/#keys-ui
  // ====================================================

  //          Key                     Enum                      Value
  //          "Accept" accepts the current IME conversion:
  DOM_KEY_MAP("Accept",               ACCEPT,                   0x0501),
  //          "Again" is used to repeat an action:
  DOM_KEY_MAP("Again",                AGAIN,                    0x0502),
  //          "Attn" is short for "Attention":
  DOM_KEY_MAP("Attn",                 ATTN,                     0x0503),
  DOM_KEY_MAP("Cancel",               CANCEL,                   0x0504),
  DOM_KEY_MAP("ContextMenu",          CONTEXT_MENU,             0x0505),
  //          "Escape" is encoded in the Unicode space.
  DOM_KEY_MAP("Execute",              EXECUTE,                  0x0506),
  DOM_KEY_MAP("Find",                 FIND,                     0x0507),
  DOM_KEY_MAP("Help",                 HELP,                     0x0508),
  //          "Pause" applies to program state - not to playing media:
  DOM_KEY_MAP("Pause",                PAUSE,                    0x0509),
  //          "Play" applies to program state - not to playing media:
  DOM_KEY_MAP("Play",                 PLAY,                     0x050A),
  //          "Props" is short for "Properties":
  DOM_KEY_MAP("Props",                PROPS,                    0x050B),
  DOM_KEY_MAP("Select",               SELECT,                   0x050C),
  DOM_KEY_MAP("ZoomIn",               ZOOM_IN,                  0x050D),
  DOM_KEY_MAP("ZoomOut",              ZOOM_OUT,                 0x050E),

  // ========================================================
  // Device Keys
  // http://www.w3.org/TR/uievents-key/#keys-device
  // ========================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("BrightnessDown",       BRIGHTNESS_DOWN,          0x0601),
  DOM_KEY_MAP("BrightnessUp",         BRIGHTNESS_UP,            0x0602),
  //          "Camera" moved to Mobile Phone Keys section.
  //         ("Camera",               CAMERA,                   0x0603),
  DOM_KEY_MAP("Eject",                EJECT,                    0x0604),
  DOM_KEY_MAP("LogOff",               LOG_OFF,                  0x0605),
  DOM_KEY_MAP("Power",                POWER,                    0x0606),
  DOM_KEY_MAP("PowerOff",             POWER_OFF,                0x0607),
  DOM_KEY_MAP("PrintScreen",          PRINT_SCREEN,             0x0608),
  DOM_KEY_MAP("Hibernate",            HIBERNATE,                0x0609),
  DOM_KEY_MAP("Standby",              STANDBY,                  0x060A),
  DOM_KEY_MAP("WakeUp",               WAKE_UP,                  0x060B),

  // =============================================================
  // IME and Composition Keys
  // http://www.w3.org/TR/uievents-key/#keys-composition
  // =============================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("AllCandidates",        ALL_CANDIDATES,           0x0701),
  DOM_KEY_MAP("Alphanumeric",         ALPHANUMERIC,             0x0702),
  DOM_KEY_MAP("CodeInput",            CODE_INPUT,               0x0703),
  DOM_KEY_MAP("Compose",              COMPOSE,                  0x0704),
  DOM_KEY_MAP("Convert",              CONVERT,                  0x0705),
  //          "Dead" is represented as a flag plus combining character.
  DOM_KEY_MAP("FinalMode",            FINAL_MODE,               0x0706),
  DOM_KEY_MAP("GroupFirst",           GROUP_FIRST,              0x0707),
  DOM_KEY_MAP("GroupLast",            GROUP_LAST,               0x0708),
  DOM_KEY_MAP("GroupNext",            GROUP_NEXT,               0x0709),
  DOM_KEY_MAP("GroupPrevious",        GROUP_PREVIOUS,           0x070A),
  DOM_KEY_MAP("ModeChange",           MODE_CHANGE,              0x070B),
  DOM_KEY_MAP("NextCandidate",        NEXT_CANDIDATE,           0x070C),
  DOM_KEY_MAP("NonConvert",           NON_CONVERT,              0x070D),
  DOM_KEY_MAP("PreviousCandidate",    PREVIOUS_CANDIDATE,       0x070E),
  DOM_KEY_MAP("Process",              PROCESS,                  0x070F),
  DOM_KEY_MAP("SingleCandidate",      SINGLE_CANDIDATE,         0x0710),

  // Keys specific to Korean keyboards
  DOM_KEY_MAP("HangulMode",           HANGUL_MODE,              0x0711),
  DOM_KEY_MAP("HanjaMode",            HANJA_MODE,               0x0712),
  DOM_KEY_MAP("JunjaMode",            JUNJA_MODE,               0x0713),

  // Keys specific to Japanese keyboards
  DOM_KEY_MAP("Eisu",                 EISU,                     0x0714),
  DOM_KEY_MAP("Hankaku",              HANKAKU,                  0x0715),
  DOM_KEY_MAP("Hiragana",             HIRAGANA,                 0x0716),
  DOM_KEY_MAP("HiraganaKatakana",     HIRAGANA_KATAKANA,        0x0717),
  DOM_KEY_MAP("KanaMode",             KANA_MODE,                0x0718),
  DOM_KEY_MAP("KanjiMode",            KANJI_MODE,               0x0719),
  DOM_KEY_MAP("Katakana",             KATAKANA,                 0x071A),
  DOM_KEY_MAP("Romaji",               ROMAJI,                   0x071B),
  DOM_KEY_MAP("Zenkaku",              ZENKAKU,                  0x071C),
  DOM_KEY_MAP("ZenkakuHankaku",       ZENKAKU_HANKAKU,          0x071D),

  // ==========================================================
  // General-Purpose Function Keys
  // http://www.w3.org/TR/uievents-key/#keys-function
  // ==========================================================

  // DOM defines open-ended sets, but if we want a finite numeric encoding we
  // need to draw the line somewhere. USB and Windows (VK) and Linux (evdev)
  // all support 24, so we define 24.

  //          Key                     Enum                      Value
  DOM_KEY_MAP("F1",                   F1,                       0x0801),
  DOM_KEY_MAP("F2",                   F2,                       0x0802),
  DOM_KEY_MAP("F3",                   F3,                       0x0803),
  DOM_KEY_MAP("F4",                   F4,                       0x0804),
  DOM_KEY_MAP("F5",                   F5,                       0x0805),
  DOM_KEY_MAP("F6",                   F6,                       0x0806),
  DOM_KEY_MAP("F7",                   F7,                       0x0807),
  DOM_KEY_MAP("F8",                   F8,                       0x0808),
  DOM_KEY_MAP("F9",                   F9,                       0x0809),
  DOM_KEY_MAP("F10",                  F10,                      0x080A),
  DOM_KEY_MAP("F11",                  F11,                      0x080B),
  DOM_KEY_MAP("F12",                  F12,                      0x080C),
  DOM_KEY_MAP("F13",                  F13,                      0x080D),
  DOM_KEY_MAP("F14",                  F14,                      0x080E),
  DOM_KEY_MAP("F15",                  F15,                      0x080F),
  DOM_KEY_MAP("F16",                  F16,                      0x0810),
  DOM_KEY_MAP("F17",                  F17,                      0x0811),
  DOM_KEY_MAP("F18",                  F18,                      0x0812),
  DOM_KEY_MAP("F19",                  F19,                      0x0813),
  DOM_KEY_MAP("F20",                  F20,                      0x0814),
  DOM_KEY_MAP("F21",                  F21,                      0x0815),
  DOM_KEY_MAP("F22",                  F22,                      0x0816),
  DOM_KEY_MAP("F23",                  F23,                      0x0817),
  DOM_KEY_MAP("F24",                  F24,                      0x0818),
  DOM_KEY_MAP("Soft1",                SOFT1,                    0x0901),
  DOM_KEY_MAP("Soft2",                SOFT2,                    0x0902),
  DOM_KEY_MAP("Soft3",                SOFT3,                    0x0903),
  DOM_KEY_MAP("Soft4",                SOFT4,                    0x0904),
  DOM_KEY_MAP("Soft5",                SOFT5,                    0x0905),
  DOM_KEY_MAP("Soft6",                SOFT6,                    0x0906),
  DOM_KEY_MAP("Soft7",                SOFT7,                    0x0907),
  DOM_KEY_MAP("Soft8",                SOFT8,                    0x0908),

  // ============================================================
  // Multimedia Keys
  // http://www.w3.org/TR/uievents-key/#keys-multimedia
  // ============================================================

  // Note: some keys were previously listed in the Media Controller Keys
  // section of the spec, and retain their original numbering (0x0D__).

  //          Key                     Enum                      Value
  DOM_KEY_MAP("ChannelDown",          CHANNEL_DOWN,             0x0D0A),
  DOM_KEY_MAP("ChannelUp",            CHANNEL_UP,               0x0D0B),
  DOM_KEY_MAP("Close",                CLOSE,                    0x0A01),
  DOM_KEY_MAP("MailForward",          MAIL_FORWARD,             0x0A02),
  DOM_KEY_MAP("MailReply",            MAIL_REPLY,               0x0A03),
  DOM_KEY_MAP("MailSend",             MAIL_SEND,                0x0A04),
  DOM_KEY_MAP("MediaClose",           MEDIA_CLOSE,              0x0D5B),
  DOM_KEY_MAP("MediaFastForward",     MEDIA_FAST_FORWARD,       0x0D2C),
  DOM_KEY_MAP("MediaPause",           MEDIA_PAUSE,              0x0D2E),
  DOM_KEY_MAP("MediaPlay",            MEDIA_PLAY,               0x0D2F),
  DOM_KEY_MAP("MediaPlayPause",       MEDIA_PLAY_PAUSE,         0x0A05),
  DOM_KEY_MAP("MediaRecord",          MEDIA_RECORD,             0x0D30),
  DOM_KEY_MAP("MediaRewind",          MEDIA_REWIND,             0x0D31),
  //          "MediaSelect" was removed from the spec.
  //         ("MediaSelect",          MEDIA_SELECT,             0x0A06),
  DOM_KEY_MAP("MediaStop",            MEDIA_STOP,               0x0A07),
  DOM_KEY_MAP("MediaTrackNext",       MEDIA_TRACK_NEXT,         0x0A08),
  DOM_KEY_MAP("MediaTrackPrevious",   MEDIA_TRACK_PREVIOUS,     0x0A09),
  DOM_KEY_MAP("New",                  NEW,                      0x0A0A),
  DOM_KEY_MAP("Open",                 OPEN,                     0x0A0B),
  DOM_KEY_MAP("Print",                PRINT,                    0x0A0C),
  DOM_KEY_MAP("Save",                 SAVE,                     0x0A0D),
  DOM_KEY_MAP("SpellCheck",           SPELL_CHECK,              0x0A0E),

  // ============================================================
  // Multimedia Numpad Keys
  // http://www.w3.org/TR/uievents-key/#keys-multimedia-numpad
  // ============================================================

  DOM_KEY_MAP("Key11",                KEY11,                    0x1201),
  DOM_KEY_MAP("Key12",                KEY12,                    0x1202),

  // =======================================================
  // Audio Keys
  // http://www.w3.org/TR/uievents-key/#keys-audio
  // =======================================================

  // Note: some keys were previously listed in the Multimedia Keys
  // (0x0A__) or Media Controller Keys (0x0D__) sections of the spec,
  // and retain their original numbering.

  //          Key                     Enum                      Value
  DOM_KEY_MAP("AudioBalanceLeft",     AUDIO_BALANCE_LEFT,       0x0D01),
  DOM_KEY_MAP("AudioBalanceRight",    AUDIO_BALANCE_RIGHT,      0x0D02),
  DOM_KEY_MAP("AudioBassBoostDown",   AUDIO_BASS_BOOST_DOWN,    0x0D03),
  DOM_KEY_MAP("AudioBassBoostToggle", AUDIO_BASS_BOOST_TOGGLE,  0x0E02),
  DOM_KEY_MAP("AudioBassBoostUp",     AUDIO_BASS_BOOST_UP,      0x0D04),
  DOM_KEY_MAP("AudioFaderFront",      AUDIO_FADER_FRONT,        0x0D05),
  DOM_KEY_MAP("AudioFaderRear",       AUDIO_FADER_REAR,         0x0D06),
  DOM_KEY_MAP("AudioSurroundModeNext",AUDIO_SURROUND_MODE_NEXT, 0x0D07),
  DOM_KEY_MAP("AudioTrebleDown",      AUDIO_TREBLE_DOWN,        0x0E04),
  DOM_KEY_MAP("AudioTrebleUp",        AUDIO_TREBLE_UP,          0x0E05),
  DOM_KEY_MAP("AudioVolumeDown",      AUDIO_VOLUME_DOWN,        0x0A0F),
  DOM_KEY_MAP("AudioVolumeUp",        AUDIO_VOLUME_UP,          0x0A10),
  DOM_KEY_MAP("AudioVolumeMute",      AUDIO_VOLUME_MUTE,        0x0A11),
  DOM_KEY_MAP("MicrophoneToggle",     MICROPHONE_TOGGLE,        0x0E06),
  DOM_KEY_MAP("MicrophoneVolumeDown", MICROPHONE_VOLUME_DOWN,   0x0E07),
  DOM_KEY_MAP("MicrophoneVolumeUp",   MICROPHONE_VOLUME_UP,     0x0E08),
  DOM_KEY_MAP("MicrophoneVolumeMute", MICROPHONE_VOLUME_MUTE,   0x0E09),

  // ========================================================
  // Speech Keys
  // http://www.w3.org/TR/uievents-key/#keys-speech
  // ========================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("SpeechCorrectionList", SPEECH_CORRECTION_LIST,   0x0F01),
  DOM_KEY_MAP("SpeechInputToggle",    SPEECH_INPUT_TOGGLE,      0x0F02),

  // ======================================================
  // Application Keys
  // http://www.w3.org/TR/uievents-key/#keys-apps
  // ======================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("LaunchApplication1",   LAUNCH_MY_COMPUTER,       0x0B06),
  DOM_KEY_MAP("LaunchApplication2",   LAUNCH_CALCULATOR,        0x0B01),
  DOM_KEY_MAP("LaunchCalendar",       LAUNCH_CALENDAR,          0x0B02),
  DOM_KEY_MAP("LaunchContacts",       LAUNCH_CONTACTS,          0x0B0C),
  DOM_KEY_MAP("LaunchMail",           LAUNCH_MAIL,              0x0B03),
  DOM_KEY_MAP("LaunchMediaPlayer",    LAUNCH_MEDIA_PLAYER,      0x0B04),
  DOM_KEY_MAP("LaunchMusicPlayer",    LAUNCH_MUSIC_PLAYER,      0x0B05),
  DOM_KEY_MAP("LaunchPhone",          LAUNCH_PHONE,             0x0B0D),
  DOM_KEY_MAP("LaunchScreenSaver",    LAUNCH_SCREEN_SAVER,      0x0B07),
  DOM_KEY_MAP("LaunchSpreadsheet",    LAUNCH_SPREADSHEET,       0x0B08),
  DOM_KEY_MAP("LaunchWebBrowser",     LAUNCH_WEB_BROWSER,       0x0B09),
  DOM_KEY_MAP("LaunchWebCam",         LAUNCH_WEB_CAM,           0x0B0A),
  DOM_KEY_MAP("LaunchWordProcessor",  LAUNCH_WORD_PROCESSOR,    0x0B0B),
  // Note: Launch Assistant entry is currently Chrome-specific.
  // It is not included in any web standard because it is not exposed
  // to the web platform.
  DOM_KEY_MAP("LaunchAssistant",      LAUNCH_ASSISTANT,         0x0B0E),
  // Similarly to Launch Assistant, Launch Control Panel is also
  // Chrome-specific.
  DOM_KEY_MAP("LaunchControlPanel",   LAUNCH_CONTROL_PANEL,     0x0B0F),

  // =========================================================
  // Browser Keys
  // http://www.w3.org/TR/uievents-key/#keys-browser
  // =========================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("BrowserBack",          BROWSER_BACK,             0x0C01),
  DOM_KEY_MAP("BrowserFavorites",     BROWSER_FAVORITES,        0x0C02),
  DOM_KEY_MAP("BrowserForward",       BROWSER_FORWARD,          0x0C03),
  DOM_KEY_MAP("BrowserHome",          BROWSER_HOME,             0x0C04),
  DOM_KEY_MAP("BrowserRefresh",       BROWSER_REFRESH,          0x0C05),
  DOM_KEY_MAP("BrowserSearch",        BROWSER_SEARCH,           0x0C06),
  DOM_KEY_MAP("BrowserStop",          BROWSER_STOP,             0x0C07),

  // =========================================================
  // Mobile Phone Keys
  // http://www.w3.org/TR/uievents-key/#keys-mobile
  // =========================================================

  // Note: some keys were previously listed in the Device Keys (0x06__)
  // section and retain their original numbering.

  //          Key                     Enum                      Value
  DOM_KEY_MAP("AppSwitch",            APP_SWITCH,               0x1001),
  DOM_KEY_MAP("Call",                 CALL,                     0x1002),
  DOM_KEY_MAP("Camera",               CAMERA,                   0x0603),
  DOM_KEY_MAP("CameraFocus",          CAMERA_FOCUS,             0x1003),
  DOM_KEY_MAP("EndCall",              END_CALL,                 0x1004),
  DOM_KEY_MAP("GoBack",               GO_BACK,                  0x1005),
  DOM_KEY_MAP("GoHome",               GO_HOME,                  0x1006),
  DOM_KEY_MAP("HeadsetHook",          HEADSET_HOOK,             0x1007),
  DOM_KEY_MAP("LastNumberRedial",     LAST_NUMBER_REDIAL,       0x1008),
  DOM_KEY_MAP("Notification",         NOTIFICATION,             0x1009),
  DOM_KEY_MAP("MannerMode",           MANNER_MODE,              0x100A),
  DOM_KEY_MAP("VoiceDial",            VOICE_DIAL,               0x100B),

  // =========================================================
  // TV Keys
  // http://www.w3.org/TR/uievents-key/#keys-tv
  // =========================================================

  // Note: some keys were previously listed in the Media Controller
  // Keys (0x0D__) section and retain their original numbering.

  //          Key                     Enum                      Value
  DOM_KEY_MAP("TV",                   TV,                       0x0D49),
  DOM_KEY_MAP("TV3DMode",             TV_3D_MODE,               0x1101),
  DOM_KEY_MAP("TVAntennaCable",       TV_ANTENNA_CABLE,         0x1102),
  DOM_KEY_MAP("TVAudioDescription",   TV_AUDIO_DESCRIPTION,     0x1103),
  DOM_KEY_MAP("TVAudioDescriptionMixDown", TV_AUDIO_DESCRIPTION_MIX_DOWN,
                                                                0x1104),
  DOM_KEY_MAP("TVAudioDescriptionMixUp", TV_AUDIO_DESCRIPTION_MIX_UP,
                                                                0x1105),
  DOM_KEY_MAP("TVContentsMenu",       TV_CONTENTS_MENU,         0x1106),
  DOM_KEY_MAP("TVDataService",        TV_DATA_SERVICE,          0x1107),
  DOM_KEY_MAP("TVInput",              TV_INPUT,                 0x0D4A),
  DOM_KEY_MAP("TVInputComponent1",    TV_INPUT_COMPONENT1,      0x1108),
  DOM_KEY_MAP("TVInputComponent2",    TV_INPUT_COMPONENT2,      0x1109),
  DOM_KEY_MAP("TVInputComposite1",    TV_INPUT_COMPOSITE1,      0x110A),
  DOM_KEY_MAP("TVInputComposite2",    TV_INPUT_COMPOSITE2,      0x110B),
  DOM_KEY_MAP("TVInputHDMI1",         TV_INPUT_HDMI1,           0x110C),
  DOM_KEY_MAP("TVInputHDMI2",         TV_INPUT_HDMI2,           0x110D),
  DOM_KEY_MAP("TVInputHDMI3",         TV_INPUT_HDMI3,           0x110E),
  DOM_KEY_MAP("TVInputHDMI4",         TV_INPUT_HDMI4,           0x110F),
  DOM_KEY_MAP("TVInputVGA1",          TV_INPUT_VGA1,            0x1110),
  DOM_KEY_MAP("TVMediaContext",       TV_MEDIA_CONTEXT,         0x1111),
  DOM_KEY_MAP("TVNetwork",            TV_NETWORK,               0x1112),
  DOM_KEY_MAP("TVNumberEntry",        TV_NUMBER_ENTRY,          0x1113),
  DOM_KEY_MAP("TVPower",              TV_POWER,                 0x0D4B),
  DOM_KEY_MAP("TVRadioService",       TV_RADIO_SERVICE,         0x1114),
  DOM_KEY_MAP("TVSatellite",          TV_SATELLITE,             0x1115),
  DOM_KEY_MAP("TVSatelliteBS",        TV_SATELLITE_BS,          0x1116),
  DOM_KEY_MAP("TVSatelliteCS",        TV_SATELLITE_CS,          0x1117),
  DOM_KEY_MAP("TVSatelliteToggle",    TV_SATELLITE_TOGGLE,      0x1118),
  DOM_KEY_MAP("TVTerrestrialAnalog",  TV_TERRESTRIAL_ANALOG,    0x1119),
  DOM_KEY_MAP("TVTerrestrialDigital", TV_TERRESTRIAL_DIGITAL,   0x111A),
  DOM_KEY_MAP("TVTimer",              TV_TIMER,                 0x111B),

  // ==================================================================
  // Media Controller Keys
  // http://www.w3.org/TR/uievents-key/#keys-media-controller
  // ==================================================================

  //          Key                     Enum                      Value
  DOM_KEY_MAP("AVRInput",             AVR_INPUT,                0x0D08),
  DOM_KEY_MAP("AVRPower",             AVR_POWER,                0x0D09),
  // Moved to Multimedia Keys section:
  //         ("ChannelDown",          CHANNEL_DOWN,             0x0D0A),
  //         ("ChannelUp",            CHANNEL_UP,               0x0D0B),
  DOM_KEY_MAP("ColorF0Red",           COLOR_F0_RED,             0x0D0C),
  DOM_KEY_MAP("ColorF1Green",         COLOR_F1_GREEN,           0x0D0D),
  DOM_KEY_MAP("ColorF2Yellow",        COLOR_F2_YELLOW,          0x0D0E),
  DOM_KEY_MAP("ColorF3Blue",          COLOR_F3_BLUE,            0x0D0F),
  DOM_KEY_MAP("ColorF4Grey",          COLOR_F4_GREY,            0x0D10),
  DOM_KEY_MAP("ColorF5Brown",         COLOR_F5_BROWN,           0x0D11),
  DOM_KEY_MAP("ClosedCaptionToggle",  CLOSED_CAPTION_TOGGLE,    0x0D12),
  DOM_KEY_MAP("Dimmer",               DIMMER,                   0x0D13),
  DOM_KEY_MAP("DisplaySwap",          DISPLAY_SWAP,             0x0D14),
  DOM_KEY_MAP("DVR",                  DVR,                      0x0D4F),
  DOM_KEY_MAP("Exit",                 EXIT,                     0x0D15),
  DOM_KEY_MAP("FavoriteClear0",       FAVORITE_CLEAR0,          0x0D16),
  DOM_KEY_MAP("FavoriteClear1",       FAVORITE_CLEAR1,          0x0D17),
  DOM_KEY_MAP("FavoriteClear2",       FAVORITE_CLEAR2,          0x0D18),
  DOM_KEY_MAP("FavoriteClear3",       FAVORITE_CLEAR3,          0x0D19),
  DOM_KEY_MAP("FavoriteRecall0",      FAVORITE_RECALL0,         0x0D1A),
  DOM_KEY_MAP("FavoriteRecall1",      FAVORITE_RECALL1,         0x0D1B),
  DOM_KEY_MAP("FavoriteRecall2",      FAVORITE_RECALL2,         0x0D1C),
  DOM_KEY_MAP("FavoriteRecall3",      FAVORITE_RECALL3,         0x0D1D),
  DOM_KEY_MAP("FavoriteStore0",       FAVORITE_STORE0,          0x0D1E),
  DOM_KEY_MAP("FavoriteStore1",       FAVORITE_STORE1,          0x0D1F),
  DOM_KEY_MAP("FavoriteStore2",       FAVORITE_STORE2,          0x0D20),
  DOM_KEY_MAP("FavoriteStore3",       FAVORITE_STORE3,          0x0D21),
  DOM_KEY_MAP("Guide",                GUIDE,                    0x0D22),
  DOM_KEY_MAP("GuideNextDay",         GUIDE_NEXT_DAY,           0x0D23),
  DOM_KEY_MAP("GuidePreviousDay",     GUIDE_PREVIOUS_DAY,       0x0D24),
  DOM_KEY_MAP("Info",                 INFO,                     0x0D25),
  DOM_KEY_MAP("InstantReplay",        INSTANT_REPLAY,           0x0D26),
  DOM_KEY_MAP("Link",                 LINK,                     0x0D27),
  DOM_KEY_MAP("ListProgram",          LIST_PROGRAM,             0x0D28),
  DOM_KEY_MAP("LiveContent",          LIVE_CONTENT,             0x0D29),
  DOM_KEY_MAP("Lock",                 LOCK,                     0x0D2A),
  DOM_KEY_MAP("MediaApps",            MEDIA_APPS,               0x0D2B),
  DOM_KEY_MAP("MediaAudioTrack",      MEDIA_AUDIO_TRACK,        0x0D50),
  // Moved to Multimedia Keys section:
  //         ("MediaFastForward",     MEDIA_FAST_FORWARD,       0x0D2C),
  DOM_KEY_MAP("MediaLast",            MEDIA_LAST,               0x0D2D),
  // Moved to Multimedia Keys section:
  //         ("MediaPause",           MEDIA_PAUSE,              0x0D2E),
  //         ("MediaPlay",            MEDIA_PLAY,               0x0D2F),
  //         ("MediaRecord",          MEDIA_RECORD,             0x0D30),
  //         ("MediaRewind",          MEDIA_REWIND,             0x0D31),
  DOM_KEY_MAP("MediaSkipBackward",    MEDIA_SKIP_BACKWARD,      0x0D51),
  DOM_KEY_MAP("MediaSkipForward",     MEDIA_SKIP_FORWARD,       0x0D52),
  DOM_KEY_MAP("MediaSkip",            MEDIA_SKIP,               0x0D32),
  DOM_KEY_MAP("MediaStepBackward",    MEDIA_STEP_BACKWARD,      0x0D53),
  DOM_KEY_MAP("MediaStepForward",     MEDIA_STEP_FORWARD,       0x0D54),
  DOM_KEY_MAP("MediaTopMenu",         MEDIA_TOP_MENU,           0x0D55),
  DOM_KEY_MAP("NavigateIn",           NAVIGATE_IN,              0x0D56),
  DOM_KEY_MAP("NavigateNext",         NAVIGATE_NEXT,            0x0D57),
  DOM_KEY_MAP("NavigateOut",          NAVIGATE_OUT,             0x0D58),
  DOM_KEY_MAP("NavigatePrevious",     NAVIGATE_PREVIOUS,        0x0D59),
  DOM_KEY_MAP("NextFavoriteChannel",  NEXT_FAVORITE_CHANNEL,    0x0D33),
  DOM_KEY_MAP("NextUserProfile",      NEXT_USER_PROFILE,        0x0D34),
  DOM_KEY_MAP("OnDemand",             ON_DEMAND,                0x0D35),
  DOM_KEY_MAP("Pairing",              PAIRING,                  0x0D5A),
  DOM_KEY_MAP("PinPDown",             PINP_DOWN,                0x0D36),
  DOM_KEY_MAP("PinPMove",             PINP_MOVE,                0x0D37),
  DOM_KEY_MAP("PinPToggle",           PINP_TOGGLE,              0x0D38),
  DOM_KEY_MAP("PinPUp",               PINP_UP,                  0x0D39),
  DOM_KEY_MAP("PlaySpeedDown",        PLAY_SPEED_DOWN,          0x0D3A),
  DOM_KEY_MAP("PlaySpeedReset",       PLAY_SPEED_RESET,         0x0D3B),
  DOM_KEY_MAP("PlaySpeedUp",          PLAY_SPEED_UP,            0x0D3C),
  DOM_KEY_MAP("RandomToggle",         RANDOM_TOGGLE,            0x0D3D),
  DOM_KEY_MAP("RcLowBattery",         RC_LOW_BATTERY,           0x0D3E),
  DOM_KEY_MAP("RecordSpeedNext",      RECORD_SPEED_NEXT,        0x0D3F),
  DOM_KEY_MAP("RfBypass",             RF_BYPASS,                0x0D40),
  DOM_KEY_MAP("ScanChannelsToggle",   SCAN_CHANNELS_TOGGLE,     0x0D41),
  DOM_KEY_MAP("ScreenModeNext",       SCREEN_MODE_NEXT,         0x0D42),
  DOM_KEY_MAP("Settings",             SETTINGS,                 0x0D43),
  DOM_KEY_MAP("SplitScreenToggle",    SPLIT_SCREEN_TOGGLE,      0x0D44),
  DOM_KEY_MAP("STBInput",             STB_INPUT,                0x0D45),
  DOM_KEY_MAP("STBPower",             STB_POWER,                0x0D46),
  DOM_KEY_MAP("Subtitle",             SUBTITLE,                 0x0D47),
  DOM_KEY_MAP("Teletext",             TELETEXT,                 0x0D48),
  DOM_KEY_MAP("VideoModeNext",        VIDEO_MODE_NEXT,          0x0D4C),
  DOM_KEY_MAP("Wink",                 WINK,                     0x0D4D),
  DOM_KEY_MAP("ZoomToggle",           ZOOM_TOGGLE,              0x0D4E),
};
