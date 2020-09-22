// Copyright 2019 The Crashpad Authors. All rights reserved.
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
// limitations under the License.

#include <android/log.h>
#include <dlfcn.h>
#include <stdlib.h>

// The first argument passed to the trampoline is the name of the native library
// exporting the symbol `CrashpadHandlerMain`. The remaining arguments are the
// same as for `HandlerMain()`.
int main(int argc, char* argv[]) {
  static constexpr char kTag[] = "crashpad";

  if (argc < 2) {
    __android_log_print(ANDROID_LOG_FATAL, kTag, "usage: %s <path>", argv[0]);
    return EXIT_FAILURE;
  }

  void* handle = dlopen(argv[1], RTLD_LAZY | RTLD_GLOBAL);
  if (!handle) {
    __android_log_print(ANDROID_LOG_FATAL, kTag, "dlopen: %s", dlerror());
    return EXIT_FAILURE;
  }

  using MainType = int (*)(int, char*[]);
  MainType crashpad_main =
      reinterpret_cast<MainType>(dlsym(handle, "CrashpadHandlerMain"));
  if (!crashpad_main) {
    __android_log_print(ANDROID_LOG_FATAL, kTag, "dlsym: %s", dlerror());
    return EXIT_FAILURE;
  }

  return crashpad_main(argc - 1, argv + 1);
}
