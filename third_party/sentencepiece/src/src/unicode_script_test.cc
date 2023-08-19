// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.!

#include "unicode_script.h"
#include "absl/strings/string_view.h"
#include "common.h"
#include "testharness.h"
#include "util.h"

namespace sentencepiece {
namespace unicode_script {
ScriptType GetScriptType(absl::string_view s) {
  const auto ut = string_util::UTF8ToUnicodeText(s);
  CHECK_EQ(1, ut.size());
  return GetScript(ut[0]);
}

TEST(UnicodeScript, GetScriptTypeTest) {
  EXPECT_EQ(U_Han, GetScriptType("京"));
  EXPECT_EQ(U_Han, GetScriptType("太"));
  EXPECT_EQ(U_Hiragana, GetScriptType("い"));
  EXPECT_EQ(U_Katakana, GetScriptType("グ"));
  EXPECT_EQ(U_Common, GetScriptType("ー"));
  EXPECT_EQ(U_Latin, GetScriptType("a"));
  EXPECT_EQ(U_Latin, GetScriptType("A"));
  EXPECT_EQ(U_Common, GetScriptType("0"));
  EXPECT_EQ(U_Common, GetScriptType("$"));
  EXPECT_EQ(U_Common, GetScriptType("@"));
  EXPECT_EQ(U_Common, GetScriptType("-"));
}
}  // namespace unicode_script
}  // namespace sentencepiece
