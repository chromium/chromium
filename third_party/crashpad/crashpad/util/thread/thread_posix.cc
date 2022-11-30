// Copyright 2015 The Crashpad Authors
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

#include "util/thread/thread.h"

#include <errno.h>

#include <ostream>

#include "base/check.h"
#include "build/build_config.h"

namespace crashpad {

void Thread::Start() {
  DCHECK(!platform_thread_);
  errno = pthread_create(&platform_thread_, nullptr, ThreadEntryThunk, this);
  PCHECK(errno == 0) << "pthread_create";
}

void Thread::Join() {
  DCHECK(platform_thread_);
  errno = pthread_join(platform_thread_, nullptr);
  PCHECK(errno == 0) << "pthread_join";
  platform_thread_ = 0;
}

#if BUILDFLAG(IS_APPLE)
uint64_t Thread::GetThreadIdForTesting() {
  uint64_t thread_self;
  errno = pthread_threadid_np(pthread_self(), &thread_self);
  PCHECK(errno == 0) << "pthread_threadid_np";
  return thread_self;
}
#endif  // BUILDFLAG(IS_APPLE)

// static
void* Thread::ThreadEntryThunk(void* argument) {
  Thread* self = reinterpret_cast<Thread*>(argument);
  self->ThreadMain();
  return nullptr;
}

}  // namespace crashpad
