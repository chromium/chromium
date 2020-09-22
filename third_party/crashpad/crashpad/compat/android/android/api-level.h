// Copyright 2018 The Crashpad Authors. All rights reserved.
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

#ifndef CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_
#define CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_

#include_next <android/api-level.h>
#include <android/ndk-version.h>

#include <sys/cdefs.h>

#if __NDK_MAJOR__ < 20

#ifdef __cplusplus
extern "C" {
#endif

// Returns the API level of the device or -1 if it can't be determined. This
// function is provided by NDK r20.
int android_get_device_api_level();

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // __NDK_MAJOR__ < 20

#endif  // CRASHPAD_COMPAT_ANDROID_ANDROID_API_LEVEL_H_
