// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/layout/xkb/xkb_composer.h"

#include <locale.h>
#include <xkbcommon/xkbcommon-keysyms.h>

#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_environment_variable_override.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event_constants.h"

namespace ui {
namespace {

constexpr char kTestComposeTable[] = R"(
<Multi_key> <o> : "ö" U00F6
<Multi_key> <O> : "Ö" U00D6
<Multi_key> <apostrophe> <a> : "á" aacute
<Multi_key> <t> <u> : "👍"
<Multi_key> <m> <w> : "multi word"
<dead_acute> <c> : "ç" ccedilla
<dead_acute> <dead_circumflex> <a> : "ấ" U1EA5
)";

TEST(XkbComposerTest, CustomMultiKeySequencesAndPreedit) {
  XkbComposer composer;
  ASSERT_TRUE(composer.SetComposeTableFromBuffer(kTestComposeTable));

  // Unmatched plain key is not consumed.
  EXPECT_FALSE(composer.FilterKeyPress(XKB_KEY_x, EF_NONE));
  EXPECT_TRUE(composer.preedit_string().empty());
  EXPECT_TRUE(composer.composed_character().empty());

  // Multi_key -> middle-dot preedit, then 'o' -> "ö".
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(composer.is_composing());
  EXPECT_EQ(composer.preedit_string(),
            std::u16string(1, XkbComposer::kPreeditComposeSymbol));
  EXPECT_TRUE(composer.composed_character().empty());

  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_o, EF_NONE));
  EXPECT_FALSE(composer.is_composing());
  EXPECT_TRUE(composer.preedit_string().empty());
  EXPECT_EQ(composer.composed_character(), u"ö");

  // Shift modifier press mid-sequence is ignored without cancelling.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_FALSE(composer.FilterKeyPress(XKB_KEY_Shift_L, EF_NONE));
  EXPECT_TRUE(composer.is_composing());
  EXPECT_EQ(composer.preedit_string(),
            std::u16string(1, XkbComposer::kPreeditComposeSymbol));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_O, EF_SHIFT_DOWN));
  EXPECT_EQ(composer.composed_character(), u"Ö");
}

TEST(XkbComposerTest, ThreeKeyPreeditAndMultiCharEmojiCommit) {
  XkbComposer composer;
  ASSERT_TRUE(composer.SetComposeTableFromBuffer(kTestComposeTable));

  // <Multi_key> <apostrophe> <a> -> preedits "·", then "'", then commits "á".
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_EQ(composer.preedit_string(), u"·");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_apostrophe, EF_NONE));
  EXPECT_EQ(composer.preedit_string(), u"'");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_a, EF_NONE));
  EXPECT_TRUE(composer.preedit_string().empty());
  EXPECT_EQ(composer.composed_character(), u"á");

  // Non-BMP emoji ("👍" = U+1F44D, UTF-16 surrogate pair).
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_t, EF_NONE));
  EXPECT_EQ(composer.preedit_string(), u"t");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_u, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"👍");

  // Multi-character string commit.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_m, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_w, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"multi word");
}

TEST(XkbComposerTest, DeadKeysAndCancellationDropsSequence) {
  XkbComposer composer;
  ASSERT_TRUE(composer.SetComposeTableFromBuffer(kTestComposeTable));

  // <dead_acute> <c> -> non-combining acute preedit "´", then "ç".
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_acute, EF_NONE));
  EXPECT_EQ(composer.preedit_string(), u"´");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_c, EF_NONE));
  EXPECT_TRUE(composer.preedit_string().empty());
  EXPECT_EQ(composer.composed_character(), u"ç");

  // Chained dead keys preedit "´^", then cancelled by unmatched '1'.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_acute, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_circumflex, EF_NONE));
  EXPECT_EQ(composer.preedit_string(), u"´^");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_1, EF_NONE));
  EXPECT_FALSE(composer.is_composing());
  EXPECT_TRUE(composer.preedit_string().empty());
  EXPECT_TRUE(composer.composed_character().empty());

  // Subsequent normal key after cancellation is not consumed.
  EXPECT_FALSE(composer.FilterKeyPress(XKB_KEY_c, EF_NONE));
}

TEST(XkbComposerTest, DeadAcuteCProducesCedilla) {
  XkbComposer composer;
  ASSERT_TRUE(composer.SetComposeTableFromBuffer(R"(
<dead_acute> <c> : "ć" cacute
<dead_acute> <C> : "Ć" Cacute
)"));

  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_acute, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_c, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"ć");

  composer.set_dead_acute_c_produces_cedilla(true);
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_acute, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_c, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"ç");
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_dead_acute, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_C, EF_SHIFT_DOWN));
  EXPECT_EQ(composer.composed_character(), u"Ç");
}

TEST(XkbComposerTest, ShortcutModifiersNotInterceptedAndCancelPending) {
  XkbComposer composer;
  ASSERT_TRUE(
      composer.SetComposeTableFromBuffer("<a> : \"ä\"\n"
                                         "<Multi_key> <c> : \"©\"\n"
                                         "<Multi_key> <KP_Divide> : \"÷\"\n"));

  // Ctrl+A is not intercepted even when plain <a> is a 1-key sequence.
  EXPECT_FALSE(composer.FilterKeyPress(XKB_KEY_a, EF_CONTROL_DOWN));
  EXPECT_TRUE(composer.composed_character().empty());

  // Plain 'a' matches the 1-key sequence.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_a, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"ä");

  // Ctrl+C mid-sequence cancels instead of matching <c>.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_FALSE(composer.FilterKeyPress(XKB_KEY_Control_L, EF_NONE));
  EXPECT_FALSE(
      composer.FilterKeyPress(XKB_KEY_ISO_Level3_Shift, EF_CONTROL_DOWN));
  EXPECT_TRUE(composer.is_composing());
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_c, EF_CONTROL_DOWN));
  EXPECT_FALSE(composer.is_composing());
  EXPECT_TRUE(composer.composed_character().empty());

  // Modified non-modifier keysyms in 0xff00..0xffe0 (such as KP_Divide) cancel
  // instead of advancing or completing a sequence.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_KP_Divide, EF_CONTROL_DOWN));
  EXPECT_FALSE(composer.is_composing());
  EXPECT_TRUE(composer.composed_character().empty());

  // Unmodified KP_Divide still completes the sequence.
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_KP_Divide, EF_NONE));
  EXPECT_EQ(composer.composed_character(), u"÷");
}

TEST(XkbComposerTest, DefaultComposeTableLoadsOffThreadAndIsShared) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath compose_file = temp_dir.GetPath().Append("XCompose");
  ASSERT_TRUE(base::WriteFile(compose_file, "<Multi_key> <q> : \"custom\"\n"));
  base::ScopedEnvironmentVariableOverride xcompose_env("XCOMPOSEFILE",
                                                       compose_file.value());
  XkbComposer::ResetDefaultComposeTableForTesting();

  // Nothing is consumed until the thread pool task has compiled the table.
  XkbComposer first;
  EXPECT_FALSE(first.EnsureInitialized());
  EXPECT_FALSE(first.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return first.EnsureInitialized(); }));
  EXPECT_TRUE(first.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(first.FilterKeyPress(XKB_KEY_q, EF_NONE));
  EXPECT_EQ(first.composed_character(), u"custom");

  // Later composers share the loaded table without reading the file again.
  ASSERT_TRUE(base::DeleteFile(compose_file));
  XkbComposer second;
  ASSERT_TRUE(second.EnsureInitialized());
  EXPECT_TRUE(second.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
  EXPECT_TRUE(second.FilterKeyPress(XKB_KEY_q, EF_NONE));
  EXPECT_EQ(second.composed_character(), u"custom");

  XkbComposer::ResetDefaultComposeTableForTesting();
}

TEST(XkbComposerTest, DefaultComposeTableLocaleFallback) {
  base::test::TaskEnvironment task_environment;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  // Locales missing from compose.dir fall back to en_US.UTF-8.
  const base::FilePath locale_dir = temp_dir.GetPath().Append("xlocale");
  const base::FilePath en_us_dir = locale_dir.Append("en_US.UTF-8");
  ASSERT_TRUE(base::CreateDirectory(en_us_dir));
  ASSERT_TRUE(base::WriteFile(locale_dir.Append("compose.dir"),
                              "en_US.UTF-8/Compose: en_US.UTF-8\n"));
  ASSERT_TRUE(base::WriteFile(en_us_dir.Append("Compose"),
                              "<Multi_key> <z> : \"fallback\"\n"));

  base::ScopedEnvironmentVariableOverride unset_xcompose("XCOMPOSEFILE");
  base::ScopedEnvironmentVariableOverride home_env("HOME",
                                                   temp_dir.GetPath().value());
  base::ScopedEnvironmentVariableOverride xdg_env("XDG_CONFIG_HOME",
                                                  temp_dir.GetPath().value());
  base::ScopedEnvironmentVariableOverride xlocaledir_env("XLOCALEDIR",
                                                         locale_dir.value());

  const char* prev_ctype = setlocale(LC_CTYPE, nullptr);
  const std::string saved_ctype = prev_ctype ? prev_ctype : "C";
  bool switched = false;
  for (const char* candidate : {"fr_FR.UTF-8", "de_DE.UTF-8", "es_ES.UTF-8"}) {
    if (setlocale(LC_CTYPE, candidate)) {
      switched = true;
      break;
    }
  }
  if (switched) {
    XkbComposer::ResetDefaultComposeTableForTesting();
    XkbComposer composer;
    ASSERT_TRUE(
        base::test::RunUntil([&]() { return composer.EnsureInitialized(); }));
    EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_Multi_key, EF_NONE));
    EXPECT_TRUE(composer.FilterKeyPress(XKB_KEY_z, EF_NONE));
    EXPECT_EQ(composer.composed_character(), u"fallback");
    XkbComposer::ResetDefaultComposeTableForTesting();
  }
  setlocale(LC_CTYPE, saved_ctype.c_str());
}

}  // namespace
}  // namespace ui
