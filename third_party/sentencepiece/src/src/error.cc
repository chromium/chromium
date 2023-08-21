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

#include <cstring>

#include "common.h"
#include "init.h"
#include "sentencepiece_processor.h"

namespace sentencepiece {
namespace error {
int gTestCounter = 0;

void Abort() {
  if (GetTestCounter() == 1) {
    SetTestCounter(2);
  } else {
    std::cerr << "Program terminated with an unrecoverable error." << std::endl;
    ShutdownLibrary();
    exit(-1);
  }
}

void Exit(int code) {
  if (GetTestCounter() == 1) {
    SetTestCounter(2);
  } else {
    ShutdownLibrary();
    exit(code);
  }
}

void SetTestCounter(int c) {
  gTestCounter = c;
}
bool GetTestCounter() {
  return gTestCounter;
}
}  // namespace error

}  // namespace sentencepiece
