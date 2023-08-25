// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_
#define THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_

#include <string>
#include <utility>

#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "third_party/leveldatabase/src/include/leveldb/env.h"

namespace leveldb {

class ChromiumLogger : public Logger {
 public:
  explicit ChromiumLogger(base::File file) : file_(std::move(file)) {}

  ~ChromiumLogger() override = default;

  void Logv(const char* format, va_list arguments) override {
    base::Time::Exploded now_exploded;
    base::Time::Now().LocalExplode(&now_exploded);

    std::string str =
        base::StringPrintf(
            "%04d/%02d/%02d-%02d:%02d:%02d.%03d %" PRIx64 " ",
            now_exploded.year, now_exploded.month, now_exploded.day_of_month,
            now_exploded.hour, now_exploded.minute, now_exploded.second,
            now_exploded.millisecond,
            static_cast<uint64_t>(base::PlatformThread::CurrentId())) +
        base::StringPrintV(format, arguments);
    if (str.back() != '\n') {
      str.push_back('\n');
    }

    file_.WriteAtCurrentPosAndCheck(base::as_bytes(base::make_span(str)));
  }

 private:
  base::File file_;
};

}  // namespace leveldb

#endif  // THIRD_PARTY_LEVELDATABASE_CHROMIUM_LOGGER_H_
