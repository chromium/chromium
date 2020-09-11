// Copyright 2020 The Crashpad Authors. All rights reserved.
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

#include <dlfcn.h>
#include <pthread.h>

#include "base/logging.h"
#include "client/crashpad_client.h"
#include "util/misc/no_cfi_icall.h"

namespace {

using StartRoutineType = void* (*)(void*);

struct StartParams {
  StartRoutineType start_routine;
  void* arg;
};

void* InitializeSignalStackAndStart(StartParams* params) {
  crashpad::CrashpadClient::InitializeSignalStackForThread();

  StartParams local_params = *params;
  delete params;

  return local_params.start_routine(local_params.arg);
}

}  // namespace

extern "C" {

__attribute__((visibility("default"))) int pthread_create(
    pthread_t* thread,
    const pthread_attr_t* attr,
    StartRoutineType start_routine,
    void* arg) {
  static const crashpad::NoCfiIcall<decltype(pthread_create)*>
      next_pthread_create([]() {
        const auto next_pthread_create = dlsym(RTLD_NEXT, "pthread_create");
        CHECK(next_pthread_create) << "dlsym: " << dlerror();
        return next_pthread_create;
      }());

  StartParams* params = new StartParams;
  params->start_routine = start_routine;
  params->arg = arg;

  int result = next_pthread_create(
      thread,
      attr,
      reinterpret_cast<StartRoutineType>(InitializeSignalStackAndStart),
      params);
  if (result != 0) {
    delete params;
  }
  return result;
}

}  // extern "C"
