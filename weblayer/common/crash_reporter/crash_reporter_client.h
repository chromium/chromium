// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBLAYER_COMMON_CRASH_REPORTER_CRASH_REPORTER_CLIENT_H_
#define WEBLAYER_COMMON_CRASH_REPORTER_CRASH_REPORTER_CLIENT_H_

#include <string>

namespace weblayer {

// Enable the collection of crashes for this process (of type |process_type|)
// via crashpad. This will collect both native crashes and uncaught Java
// exceptions as minidumps plus associated metadata.
void EnableCrashReporter(const std::string& process_type);

}  // namespace weblayer

#endif  // WEBLAYER_COMMON_CRASH_REPORTER_CRASH_REPORTER_CLIENT_H_
