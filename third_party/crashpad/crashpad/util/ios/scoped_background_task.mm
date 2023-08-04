// Copyright 2022 The Crashpad Authors
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

#include "util/ios/scoped_background_task.h"

#import <Foundation/Foundation.h>

namespace crashpad {
namespace internal {

ScopedBackgroundTask::ScopedBackgroundTask(const char* task_name)
    : task_complete_semaphore_(dispatch_semaphore_create(0)) {
  __weak dispatch_semaphore_t weak_task_complete_semaphore =
      task_complete_semaphore_;
  NSString* name = [NSString stringWithUTF8String:task_name];
  [[NSProcessInfo processInfo]
      performExpiringActivityWithReason:name
                             usingBlock:^(BOOL expired) {
                               if (expired) {
                                 // TODO(crbug.com/crashpad/400): Notify the
                                 // BG task creator that time's up -- this will
                                 // be useful for BackgroundTasks and
                                 // NSURLSessions.
                                 return;
                               }
                               __strong dispatch_semaphore_t
                                   strong_task_complete_semaphore =
                                       weak_task_complete_semaphore;
                               if (!strong_task_complete_semaphore) {
                                 return;
                               }
                               dispatch_semaphore_wait(
                                   strong_task_complete_semaphore,
                                   DISPATCH_TIME_FOREVER);
                             }];
}

ScopedBackgroundTask::~ScopedBackgroundTask() {
  dispatch_semaphore_signal(task_complete_semaphore_);
}

}  // namespace internal
}  // namespace crashpad
