// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tensorflow/lite/minimal_logging.h"

#include <stdarg.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"

namespace tflite {
namespace logging_internal {

namespace {

int GetChromiumLogSeverity(LogSeverity severity) {
  switch (severity) {
    case TFLITE_LOG_VERBOSE:
      return ::logging::LOGGING_VERBOSE;
    case TFLITE_LOG_INFO:
      return ::logging::LOGGING_INFO;
    case TFLITE_LOG_WARNING:
      return ::logging::LOGGING_WARNING;
    case TFLITE_LOG_ERROR:
      return ::logging::LOGGING_ERROR;
    // LOG_SILENT doesn't exist, Use LOGGING_VERBOSE instead.
    case TFLITE_LOG_SILENT:
    default:
      return ::logging::LOGGING_VERBOSE;
  }
}

}  // namespace

#ifndef NDEBUG
// In debug builds, default is VERBOSE.
LogSeverity MinimalLogger::minimum_log_severity_ = TFLITE_LOG_VERBOSE;
#else
// In prod builds, default is INFO.
LogSeverity MinimalLogger::minimum_log_severity_ = TFLITE_LOG_INFO;
#endif

void MinimalLogger::LogFormatted(LogSeverity severity,
                                 const char* format,
                                 va_list args) {
  const int chromium_log_severity = GetChromiumLogSeverity(severity);
  if (chromium_log_severity >= ::logging::GetMinLogLevel()) {
    auto formatted_message = ::base::StringPrintV(format, args);
    ::logging::RawLog(chromium_log_severity, formatted_message.c_str());
  }
}

}  // namespace logging_internal
}  // namespace tflite
