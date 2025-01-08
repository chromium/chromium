/*
 * Copyright 2020 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "audio/dsp/portable/logging.h"

#ifndef __cplusplus
#error This test must compile in C++ mode.
#endif

#include <memory>

// We can't use gtest because it includes base/logging.h.

typedef struct Thing {
  int stuff;
} Thing;

static int things_made;

struct Thing* MakeThing() {
  ++things_made;
  return new Thing();
}

// Suppress lint warning that recommends to use ABSL_CHECK_EQ below.
#define ABSL_CHECK_EQ(a, b) ABSL_CHECK(a == b)

void DoesntEvaluateTwice() {
  things_made = 0;
  std::unique_ptr<Thing> my_thing(ABSL_CHECK_NOTNULL(MakeThing()));
  ABSL_CHECK_EQ(things_made, 1);
}

int main(int argc, char** argv) {
  srand(0);
  DoesntEvaluateTwice();

  puts("PASS");
  return EXIT_SUCCESS;
}
