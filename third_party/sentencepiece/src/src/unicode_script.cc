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

#include <unordered_map>

#include "absl/container/flat_hash_map.h"
#include "unicode_script.h"
#include "unicode_script_map.h"
#include "util.h"

namespace sentencepiece {
namespace unicode_script {
namespace {
class GetScriptInternal {
 public:
  GetScriptInternal() { InitTable(&smap_); }

  ScriptType GetScript(char32 c) const {
    return port::FindWithDefault(smap_, c, ScriptType::U_Common);
  }

 private:
  absl::flat_hash_map<char32, ScriptType> smap_;
};
}  // namespace

ScriptType GetScript(char32 c) {
  static GetScriptInternal sc;
  return sc.GetScript(c);
}
}  // namespace unicode_script
}  // namespace sentencepiece
