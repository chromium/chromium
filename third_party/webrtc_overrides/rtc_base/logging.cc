// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// NOTE:
// Since this file includes Chromium headers, it must not include
// third_party/webrtc/rtc_base/logging.h since it defines some of the same
// macros as Chromium does and we'll run into conflicts.

#if defined(WEBRTC_MAC) && !defined(WEBRTC_IOS)
#include <CoreServices/CoreServices.h>
#endif  // OS_MACOSX

#include <algorithm>
#include <atomic>
#include <iomanip>

#include "base/logging.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/threading/platform_thread.h"
#include "third_party/webrtc/rtc_base/string_utils.h"

// This needs to be included after base/logging.h.
#include "third_party/webrtc_overrides/rtc_base/diagnostic_logging.h"
#include "third_party/webrtc_overrides/rtc_base/logging.h"

#if defined(WEBRTC_MAC)
#include "base/apple/osstatus_logging.h"
#endif

// Disable logging when fuzzing, for performance reasons.
// WEBRTC_UNSAFE_FUZZER_MODE is defined by WebRTC's BUILD.gn when
// built with use_libfuzzer or use_drfuzz.
#if defined(WEBRTC_UNSAFE_FUZZER_MODE)
#define WEBRTC_ENABLE_LOGGING false
#else
#define WEBRTC_ENABLE_LOGGING true
#endif

// From this file we can't use VLOG since it expands into usage of the __FILE__
// macro (for correct filtering). The actual logging call from DIAGNOSTIC_LOG in
// ~DiagnosticLogMessage. Note that the second parameter to the LAZY_STREAM
// macro is not used since the filter check has already been done for
// DIAGNOSTIC_LOG.
#define LOG_LAZY_STREAM_DIRECT(file_name, line_number, sev)              \
  LAZY_STREAM(logging::LogMessage(file_name, line_number, sev).stream(), \
              WEBRTC_ENABLE_LOGGING)

namespace rtc {

void (*g_logging_delegate_function)(const std::string&) = NULL;
void (*g_extra_logging_init_function)(
    void (*logging_delegate_function)(const std::string&)) = NULL;
#ifndef NDEBUG
std::atomic<base::PlatformThreadId> g_init_logging_delegate_thread_id =
    base::kInvalidThreadId;
#endif

/////////////////////////////////////////////////////////////////////////////
// Log helper functions
/////////////////////////////////////////////////////////////////////////////

inline int WebRtcSevToChromeSev(LoggingSeverity sev) {
  switch (sev) {
    case LS_ERROR:
      return ::logging::LOGGING_ERROR;
    case LS_WARNING:
      return ::logging::LOGGING_WARNING;
    case LS_INFO:
      return ::logging::LOGGING_INFO;
    case LS_VERBOSE:
    case LS_SENSITIVE:
      return ::logging::LOGGING_VERBOSE;
    default:
      NOTREACHED_IN_MIGRATION();
      return ::logging::LOGGING_FATAL;
  }
}

inline int WebRtcVerbosityLevel(LoggingSeverity sev) {
  switch (sev) {
    case LS_ERROR:
      return -2;
    case LS_WARNING:
      return -1;
    case LS_INFO:  // We treat 'info' and 'verbose' as the same verbosity level.
    case LS_VERBOSE:
      return 1;
    case LS_SENSITIVE:
      return 2;
    default:
      NOTREACHED_IN_MIGRATION();
      return 0;
  }
}

// Logs extra information for LOG_E.
static void LogExtra(std::ostringstream* print_stream,
                     LogErrorContext err_ctx,
                     int err,
                     const char* module) {
  if (err_ctx == ERRCTX_NONE)
    return;

  (*print_stream) << ": ";
  (*print_stream) << "[0x" << std::setfill('0') << std::hex << std::setw(8)
                  << err << "]";
  switch (err_ctx) {
    case ERRCTX_ERRNO:
      (*print_stream) << " " << strerror(err);
      break;
#if defined(WEBRTC_WIN)
    case ERRCTX_HRESULT: {
      char msgbuf[256];
      DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM;
      HMODULE hmod = GetModuleHandleA(module);
      if (hmod)
        flags |= FORMAT_MESSAGE_FROM_HMODULE;
      if (DWORD len = FormatMessageA(
              flags, hmod, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
              msgbuf, sizeof(msgbuf) / sizeof(msgbuf[0]), NULL)) {
        while ((len > 0) &&
               isspace(static_cast<unsigned char>(msgbuf[len - 1]))) {
          msgbuf[--len] = 0;
        }
        (*print_stream) << " " << msgbuf;
      }
      break;
    }
#elif defined(WEBRTC_IOS)
    case ERRCTX_OSSTATUS:
      (*print_stream) << " "
                      << "Unknown LibJingle error: " << err;
      break;
#elif defined(WEBRTC_MAC)
    case ERRCTX_OSSTATUS: {
      (*print_stream) << " " << logging::DescriptionFromOSStatus(err);
      break;
    }
#endif  // defined(WEBRTC_WIN)
    default:
      break;
  }
}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           LogErrorContext err_ctx,
                                           int err)
    : DiagnosticLogMessage(file, line, severity, err_ctx, err, nullptr) {}

DiagnosticLogMessage::DiagnosticLogMessage(const char* file,
                                           int line,
                                           LoggingSeverity severity,
                                           LogErrorContext err_ctx,
                                           int err,
                                           const char* module)
    : file_name_(file),
      line_(line),
      severity_(severity),
      err_ctx_(err_ctx),
      err_(err),
      module_(module),
      log_to_chrome_(CheckVlogIsOnHelper(severity, file, strlen(file) + 1)) {}

DiagnosticLogMessage::~DiagnosticLogMessage() {
  const bool call_delegate =
      g_logging_delegate_function && severity_ <= LS_INFO;

  if (call_delegate || log_to_chrome_) {
    LogExtra(&print_stream_, err_ctx_, err_, module_);
    const std::string& str = print_stream_.str();
    if (log_to_chrome_) {
      LOG_LAZY_STREAM_DIRECT(file_name_, line_,
                             rtc::WebRtcSevToChromeSev(severity_))
          << str;
    }

    if (g_logging_delegate_function && severity_ <= LS_INFO) {
      g_logging_delegate_function(str);
    }
  }
}

// static
void LogMessage::LogToDebug(int min_sev) {
  logging::SetMinLogLevel(min_sev);
}

void InitDiagnosticLoggingDelegateFunction(
    void (*delegate)(const std::string&)) {
#ifndef NDEBUG
  // Ensure that this function is always called from the same thread.
  base::PlatformThreadId expected_thread_id = base::kInvalidThreadId;
  g_init_logging_delegate_thread_id.compare_exchange_strong(
      expected_thread_id, base::PlatformThread::CurrentId(),
      std::memory_order_relaxed, std::memory_order_relaxed);
  DCHECK_EQ(g_init_logging_delegate_thread_id,
            base::PlatformThread::CurrentId());
#endif
  CHECK(delegate);
  // This function may be called with the same argument several times if the
  // page is reloaded or there are several PeerConnections on one page with
  // logging enabled. This is OK, we simply don't have to do anything.
  if (delegate == g_logging_delegate_function)
    return;
  CHECK(!g_logging_delegate_function);
  g_logging_delegate_function = delegate;

  if (g_extra_logging_init_function)
    g_extra_logging_init_function(delegate);
}

void SetExtraLoggingInit(
    void (*function)(void (*delegate)(const std::string&))) {
  CHECK(function);
  CHECK(!g_extra_logging_init_function);
  g_extra_logging_init_function = function;
}

bool CheckVlogIsOnHelper(rtc::LoggingSeverity severity,
                         const char* file,
                         size_t N) {
  return rtc::WebRtcVerbosityLevel(severity) <=
         ::logging::GetVlogLevelHelper(file, N);
}

}  // namespace rtc
