// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "standalone/base/logging.h"

#include <atomic>
#include <iostream>

#include "build/build_config.h"
#include "third_party/abseil-cpp/absl/base/log_severity.h"

#if BUILDFLAG(IS_POSIX)
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if defined(THREAD_SANITIZER)
#include "third_party/abseil-cpp/absl/synchronization/mutex.h"
#endif

namespace ipcz::standalone {

namespace {

std::atomic_int g_verbosity_level{0};

#if defined(THREAD_SANITIZER)
// Accessing std::cerr is supposed to be thread-safe, but it was triggering
// TSAN data race warnings. We explicitly guard it with a Mutex here to avoid
// any potential issues for now.
absl::Mutex* GetCerrMutex() {
  static absl::Mutex* mutex = new absl::Mutex();
  return mutex;
}
#endif

}  // namespace

LogMessage::LogMessage(const char* file, int line, Level level) {
  stream_ << "[";
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  stream_ << getpid() << ":" << syscall(__NR_gettid) << ":";
  const char* trimmed_file = strrchr(file, '/') + 1;
#elif BUILDFLAG(IS_ANDROID)
  stream_ << getpid() << ":" << gettid() << ":";
  const char* trimmed_file = strrchr(file, '/') + 1;
#elif BUILDFLAG(IS_WIN)
  const char* trimmed_file = file;
  stream_ << (::GetCurrentProcessId()) << ":" << ::GetCurrentThreadId() << ":";
#else
  const char* trimmed_file = file;
#endif
  stream_ << absl::LogSeverityName(level) << ":"
          << (trimmed_file ? trimmed_file : file) << "(" << line << ")] ";
}

LogMessage::~LogMessage() {
#if defined(THREAD_SANITIZER)
  // Paper over potential data race within std::cerr access under TSAN.
  absl::MutexLock lock(GetCerrMutex());
#endif

  std::cerr << stream_.str() << std::endl;
}

void SetVerbosityLevel(int level) {
  g_verbosity_level.store(level, std::memory_order_relaxed);
}

int GetVerbosityLevel() {
  return g_verbosity_level.load(std::memory_order_relaxed);
}

}  // namespace ipcz::standalone
