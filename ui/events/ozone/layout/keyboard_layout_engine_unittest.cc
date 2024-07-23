// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"
#include "ui/events/ozone/layout/scoped_keyboard_layout_engine.h"
#include "ui/events/ozone/layout/stub/stub_keyboard_layout_engine.h"
#include "ui/events/ozone/layout/xkb/xkb_evdev_codes.h"
#include "ui/events/ozone/layout/xkb/xkb_keyboard_layout_engine.h"
#include "ui/events/types/event_type.h"

namespace ui {

namespace {

// Hardcoded US layout for standalone testing of XkbKeyboardLayoutEngine.
const char* kUsLayoutXkbKeymap =
    "xkb_keymap{"
    "xkb_keycodes \"evdev_aliases(qwerty)\"{minimum=8;maximum=255;"
    "<ESC>=9;<AE01>=10;<AE02>=11;<AE03>=12;<AE04>=13;<AE05>=14;"
    "<AE06>=15;<AE07>=16;<AE08>=17;<AE09>=18;<AE10>=19;<AE11>=20;"
    "<AE12>=21;<BKSP>=22;<TAB>=23;<AD01>=24;<AD02>=25;<AD03>=26;"
    "<AD04>=27;<AD05>=28;<AD06>=29;<AD07>=30;<AD08>=31;<AD09>=32;"
    "<AD10>=33;<AD11>=34;<AD12>=35;<RTRN>=36;<LCTL>=37;<AC01>=38;"
    "<AC02>=39;<AC03>=40;<AC04>=41;<AC05>=42;<AC06>=43;<AC07>=44;"
    "<AC08>=45;<AC09>=46;<AC10>=47;<AC11>=48;<TLDE>=49;<LFSH>=50;"
    "<BKSL>=51;<AB01>=52;<AB02>=53;<AB03>=54;<AB04>=55;<AB05>=56;"
    "<AB06>=57;<AB07>=58;<AB08>=59;<AB09>=60;<AB10>=61;<RTSH>=62;"
    "<KPMU>=63;<LALT>=64;<SPCE>=65;<CAPS>=66;<FK01>=67;<FK02>=68;"
    "<NMLK>=77;<SCLK>=78;<KP7>=79;<KP8>=80;<KP9>=81;<KPSU>=82;<KP4>=83;"
    "<KP5>=84;<KP6>=85;<KPAD>=86;<KP1>=87;<KP2>=88;<KP3>=89;<KP0>=90;"
    "<KPDL>=91;<LVL3>=92;<LSGT>=94;<FK11>=95;<FK12>=96;<KATA>=98;<HIRA>=99;"
    "<HENK>=100;<HKTG>=101;<MUHE>=102;<KPEN>=104;<RCTL>=105;"
    "<KPDV>=106;<PRSC>=107;<RALT>=108;<LNFD>=109;<HOME>=110;<UP>=111;"
    "<PGUP>=112;<LEFT>=113;<RGHT>=114;<END>=115;<DOWN>=116;<PGDN>=117;"
    "<INS>=118;<DELE>=119;<MUTE>=121;<VOL->=122;<VOL+>=123;<POWR>=124;"
    "<MDSW>=203;<ALT>=204;<META>=205;<SUPR>=206;<HYPR>=207;};"
    "xkb_types \"complete\"{virtual_modifiers NumLock,Alt,LevelThree,LAlt,"
    "RAlt,RControl,LControl,ScrollLock,LevelFive,AltGr,Meta,Super,Hyper;"
    "type \"ONE_LEVEL\"{modifiers=none;level_name[Level1]=\"Any\";};"
    "type \"TWO_LEVEL\"{modifiers=Shift;map[Shift]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Shift\";};"
    "type \"ALPHABETIC\"{modifiers=Shift+Lock;"
    "map[Shift]=Level2;map[Lock]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Caps\";};"
    "type \"FOUR_LEVEL\"{modifiers=Shift+LevelThree;map[Shift]=Level2;"
    "map[LevelThree]=Level3;map[Shift+LevelThree]=Level4;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Shift\";"
    "level_name[Level3]=\"Alt Base\";level_name[Level4]=\"Shift Alt\";};"
    "type \"KEYPAD\"{modifiers=Shift+NumLock;"
    "map[Shift]=Level2;map[NumLock]=Level2;"
    "level_name[Level1]=\"Base\";level_name[Level2]=\"Number\";};};"
    "xkb_compatibility \"complete\"{virtual_modifiers NumLock,Alt,LevelThree,"
    "LAlt,RAlt,RControl,LControl,ScrollLock,LevelFive,AltGr,Meta,Super,Hyper;"
    "interpret.useModMapMods=AnyLevel;interpret.repeat=False;"
    "interpret ISO_Level2_Latch+Exactly(Shift){useModMapMods=level1;"
    "action=LatchMods(modifiers=Shift,clearLocks,latchToLock);};"
    "interpret Shift_Lock+AnyOf(Shift+Lock){action=LockMods(modifiers=Shift);};"
    "interpret Num_Lock+AnyOf(all){virtualModifier=NumLock;"
    "action=LockMods(modifiers=NumLock);};"
    "interpret ISO_Level3_Shift+AnyOf(all){virtualModifier=LevelThree;"
    "useModMapMods=level1;action=SetMods(modifiers=LevelThree,clearLocks);};"
    "interpret ISO_Level3_Latch+AnyOf(all){"
    "virtualModifier=LevelThree;useModMapMods=level1;"
    "action=LatchMods(modifiers=LevelThree,clearLocks,latchToLock);};"
    "interpret ISO_Level3_Lock+AnyOf(all){virtualModifier=LevelThree;"
    "useModMapMods=level1;action=LockMods(modifiers=LevelThree);};"
    "interpret Alt_L+AnyOf(all){virtualModifier=Alt;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Alt_R+AnyOf(all){virtualModifier=Alt;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Meta_L+AnyOf(all){virtualModifier=Meta;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Meta_R+AnyOf(all){virtualModifier=Meta;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Super_L+AnyOf(all){virtualModifier=Super;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Super_R+AnyOf(all){virtualModifier=Super;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Hyper_L+AnyOf(all){virtualModifier=Hyper;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Hyper_R+AnyOf(all){virtualModifier=Hyper;"
    "action=SetMods(modifiers=modMapMods,clearLocks);};"
    "interpret Mode_switch+AnyOfOrNone(all){virtualModifier=AltGr;"
    "useModMapMods=level1;action=SetGroup(group=+1);};"
    "interpret ISO_Level3_Shift+AnyOfOrNone(all){"
    "action=SetMods(modifiers=LevelThree,clearLocks);};"
    "interpret ISO_Level3_Latch+AnyOfOrNone(all){"
    "action=LatchMods(modifiers=LevelThree,clearLocks,latchToLock);};"
    "interpret ISO_Level3_Lock+AnyOfOrNone(all){"
    "action=LockMods(modifiers=LevelThree);};"
    "interpret Alt_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Alt,clearLocks);};"
    "interpret Alt_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Alt,clearLocks);};"
    "interpret Meta_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Meta,clearLocks);};"
    "interpret Meta_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Meta,clearLocks);};"
    "interpret Super_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Super,clearLocks);};"
    "interpret Super_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Super,clearLocks);};"
    "interpret Hyper_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Hyper,clearLocks);};"
    "interpret Hyper_R+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Hyper,clearLocks);};"
    "interpret Shift_L+AnyOfOrNone(all){"
    "action=SetMods(modifiers=Shift,clearLocks);};"
    "interpret XF86Switch_VT_1+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=1,!same);};"
    "interpret XF86Switch_VT_2+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=2,!same);};"
    "interpret XF86Switch_VT_3+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=3,!same);};"
    "interpret XF86Switch_VT_4+AnyOfOrNone(all){repeat=True;"
    "action=SwitchScreen(screen=4,!same);};"
    "interpret Caps_Lock+AnyOfOrNone(all){action=LockMods(modifiers=Lock);};"
    "interpret Any+Exactly(Lock){action=LockMods(modifiers=Lock);};"
    "interpret Any+AnyOf(all){action=SetMods(modifiers=modMapMods,clearLocks);"
    "};};"
    "xkb_symbols \"pc_us_inet(evdev)\"{name[group1]=\"English (US)\";"
    "key<ESC>{[Escape]};key<AE01>{[1,exclam]};key<AE02>{[2,at]};"
    "key<AE03>{[3,numbersign]};key<AE04>{[4,dollar]};key<AE05>{[5,percent]};"
    "key<AE06>{[6,asciicircum]};key<AE07>{[7,ampersand]};"
    "key<AE08>{[8,asterisk]};key<AE09>{[9,parenleft]};"
    "key<AE10>{[0,parenright]};key<AE11>{[minus,underscore]};"
    "key<AE12>{[equal,plus]};key<BKSP>{[BackSpace,BackSpace]};"
    "key<TAB>{[Tab,ISO_Left_Tab]};key<AD01>{[q,Q]};key<AD02>{[w,W]};"
    "key<AD03>{[e,E]};key<AD04>{[r,R]};key<AD05>{[t,T]};key<AD06>{[y,Y]};"
    "key<AD07>{[u,U]};key<AD08>{[i,I]};key<AD09>{[o,O]};key<AD10>{[p,P]};"
    "key<AD11>{[bracketleft,braceleft]};key<AD12>{[bracketright,braceright]};"
    "key<RTRN>{[Return]};key<LCTL>{[Control_L]};key<AC01>{[a,A]};"
    "key<AC02>{[s,S]};key<AC03>{[d,D]};key<AC04>{[f,F]};key<AC05>{[g,G]};"
    "key<AC06>{[h,H]};key<AC07>{[j,J]};key<AC08>{[k,K]};key<AC09>{[l,L]};"
    "key<AC10>{[semicolon,colon]};key<AC11>{[apostrophe,quotedbl]};"
    "key<TLDE>{[grave,asciitilde]};key<LFSH>{[Shift_L]};"
    "key<BKSL>{[backslash,bar]};key<AB01>{[z,Z]};key<AB02>{[x,X]};"
    "key<AB03>{[c,C]};key<AB04>{[v,V]};key<AB05>{[b,B]};key<AB06>{[n,N]};"
    "key<AB07>{[m,M]};key<AB08>{[comma,less]};key<AB09>{[period,greater]};"
    "key<AB10>{[slash,question]};key<RTSH>{[Shift_R]};"
    "key<KPMU>{[KP_Multiply,KP_Multiply]};key<LALT>{[Alt_L,Meta_L]};"
    "key<SPCE>{[space]};key<CAPS>{[Caps_Lock]};key<FK01>{[F1,F1]};"
    "key<FK02>{[F2,F2]};key<NMLK>{[Num_Lock]};key<SCLK>{[Scroll_Lock]};"
    "key<KP7>{[KP_Home,KP_7]};key<KP8>{[KP_Up,KP_8]};key<KP9>{[KP_Prior,KP_9]};"
    "key<KPSU>{[KP_Subtract,KP_Subtract]};key<KP4>{[KP_Left,KP_4]};"
    "key<KP5>{[KP_Begin,KP_5]};key<KP6>{[KP_Right,KP_6]};"
    "key<KPAD>{[KP_Add,KP_Add]};key<KP1>{[KP_End,KP_1]};"
    "key<KP2>{[KP_Down,KP_2]};key<KP3>{[KP_Next,KP_3]};"
    "key<KP0>{[KP_Insert,KP_0]};key<KPDL>{[KP_Delete,KP_Decimal]};"
    "key<LVL3>{[ISO_Level3_Shift]};key<LSGT>{[less,greater, bar,brokenbar]};"
    "key<FK11>{[F11,F11]};key<FK12>{[F12,F12]};key<KATA>{[Katakana]};"
    "key<HIRA>{[Hiragana]};key<HENK>{[Henkan_Mode]};"
    "key<HKTG>{[Hiragana_Katakana]};key<MUHE>{[Muhenkan]};"
    "key<KPEN>{[KP_Enter]};key<RCTL>{[Control_R]};key<MDSW>{[Mode_switch]};"
    "key<KPDV>{[KP_Divide,KP_Divide]};key<PRSC>{[Print,Sys_Req]};"
    "key<RALT>{type=\"TWO_LEVEL\",symbols[Group1]=[Alt_R,Meta_R]};"
    "key<LNFD>{[Linefeed]};key<HOME>{[Home]};key<UP>{[Up]};key<PGUP>{[Prior]};"
    "key<LEFT>{[Left]};key<RGHT>{[Right]};key<END>{[End]};key<DOWN>{[Down]};"
    "key<PGDN>{[Next]};key<INS>{[Insert]};key<DELE>{[Delete]};"
    "key<MUTE>{[XF86AudioMute]};key<VOL->{[XF86AudioLowerVolume]};"
    "key<VOL+>{[XF86AudioRaiseVolume]};key<POWR>{[XF86PowerOff]};"
    "key<ALT>{[NoSymbol,Alt_L]};key<META>{[NoSymbol,Meta_L]};"
    "key<SUPR>{[NoSymbol,Super_L]};key<HYPR>{[NoSymbol,Hyper_L]};"
    "modifier_map Control{<LCTL>};modifier_map Shift{<LFSH>};"
    "modifier_map Shift{<RTSH>};modifier_map Mod1{<LALT>};"
    "modifier_map Lock{<CAPS>};modifier_map Mod2{<NMLK>};"
    "modifier_map Mod5{<LVL3>};modifier_map Control{<RCTL>};"
    "modifier_map Mod1{<RALT>};modifier_map Mod5{<MDSW>};"
    "modifier_map Mod1{<META>};modifier_map Mod4{<SUPR>};"
    "modifier_map Mod4{<HYPR>};};};";

void TestLookup(const char* name, KeyboardLayoutEngine* engine) {
  static const struct {
    DomCode input_dom_code;
    int input_flags;
    DomKey output_dom_key;
    KeyboardCode output_keycode;
    char16_t output_character;
  } kTestCases[] = {
      {DomCode::US_A, EF_NONE, DomKey::FromCharacter('a'), VKEY_A, 'a'},
      {DomCode::US_A, EF_SHIFT_DOWN, DomKey::FromCharacter('A'), VKEY_A, 'A'},
      {DomCode::US_A, EF_CONTROL_DOWN, DomKey::FromCharacter('a'), VKEY_A, 1},
      {DomCode::LAUNCH_ASSISTANT, EF_NONE, DomKey::LAUNCH_ASSISTANT,
       VKEY_ASSISTANT, 0},
  };

  for (const auto& t : kTestCases) {
    DomKey dom_key;
    KeyboardCode keycode;
    SCOPED_TRACE(base::StringPrintf(
        "%s(%s, 0x%X)", name,
        KeycodeConverter::DomCodeToCodeString(t.input_dom_code).c_str(),
        t.input_flags));
    EXPECT_TRUE(
        engine->Lookup(t.input_dom_code, t.input_flags, &dom_key, &keycode));
    EXPECT_EQ(t.output_dom_key, dom_key);
    EXPECT_EQ(t.output_keycode, keycode);
    KeyEvent key_event(EventType::kKeyPressed, keycode, t.input_dom_code,
                       t.input_flags, dom_key, EventTimeForNow());
    EXPECT_EQ(t.output_character, key_event.GetCharacter());
  }
}

}  // anonymous namespace

TEST(LayoutEngineTest, Lookup) {
  {
    // Test StubKeyboardLayoutEngine
    auto stub_engine = std::make_unique<StubKeyboardLayoutEngine>();
    TestLookup("StubKeyboardLayoutEngine", stub_engine.get());
  }

  {
    // Test XkbKeyboardLayoutEngine
    XkbEvdevCodes xkb_evdev_code_converter;
    auto xkb_engine =
        std::make_unique<XkbKeyboardLayoutEngine>(xkb_evdev_code_converter);
    xkb_engine->SetCurrentLayoutFromBuffer(kUsLayoutXkbKeymap,
                                           strlen(kUsLayoutXkbKeymap));
    TestLookup("XkbKeyboardLayoutEngine", xkb_engine.get());
  }
}

}  // namespace ui
