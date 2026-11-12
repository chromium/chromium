/*
 * Copyright (C) 2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ART_LIBARTBASE_BASE_LOGGING_H_
#define ART_LIBARTBASE_BASE_LOGGING_H_

#include <sstream>
#include <variant>

#include "android-base/logging.h"
#include "macros.h"

namespace art {

// Make libbase's LogSeverity more easily available.
using ::android::base::LogSeverity;
using ::android::base::ScopedLogSeverity;

// Abort function.
using AbortFunction = void(const char*);

// The members of this struct are the valid arguments to VLOG and VLOG_IS_ON in code,
// and the "-verbose:" command line argument.
struct LogVerbosity {
  bool class_linker;  // Enabled with "-verbose:class".
  bool collector;
  bool compiler;
  bool deopt;
  bool gc;
  bool heap;
  bool interpreter;  // Enabled with "-verbose:interpreter".
  bool jdwp;
  bool jit;
  bool jni;
  bool monitor;
  bool oat;
  bool profiler;
  bool signals;
  bool simulator;
  bool startup;
  bool third_party_jni;  // Enabled with "-verbose:third-party-jni".
  bool threads;
  bool verifier;
  bool verifier_debug;   // Only works in debug builds.
  bool image;
  bool systrace_lock_logging;  // Enabled with "-verbose:systrace-locks".
  bool agents;
  bool dex;  // Some dex access output etc.
  bool plugin;  // Used by some plugins.
};

// Global log verbosity setting, initialized by InitLogging.
extern LogVerbosity gLogVerbosity;

// Configure logging based on ANDROID_LOG_TAGS environment variable.
// We need to parse a string that looks like
//
//      *:v jdwp:d dalvikvm:d dalvikvm-gc:i dalvikvmi:i
//
// The tag (or '*' for the global level) comes first, followed by a colon
// and a letter indicating the minimum priority level we're expected to log.
// This can be used to reveal or conceal logs with specific tags.
extern void InitLogging(char* argv[], AbortFunction& default_aborter);

// Returns the command line used to invoke the current tool or null if InitLogging hasn't been
// performed.
extern const char* GetCmdLine();

// The command used to start the ART runtime, such as "/apex/com.android.art/bin/dalvikvm". If
// InitLogging hasn't been performed then just returns "art".
extern const char* ProgramInvocationName();

// A short version of the command used to start the ART runtime, such as "dalvikvm". If InitLogging
// hasn't been performed then just returns "art".
extern const char* ProgramInvocationShortName();

class LogHelper {
 public:
  // A logging helper for logging a single line. Can be used with little stack.
  static void LogLineLowStack(const char* file,
                              unsigned int line,
                              android::base::LogSeverity severity,
                              const char* msg);

 private:
  DISALLOW_ALLOCATION();
  DISALLOW_COPY_AND_ASSIGN(LogHelper);
};

// Copy the contents of file_name to the log stream for level.
bool PrintFileToLog(const std::string& file_name, android::base::LogSeverity level);

// Is verbose logging enabled for the given module? Where the module is defined in LogVerbosity.
#define VLOG_IS_ON(module) UNLIKELY(::art::gLogVerbosity.module)

// Variant of LOG that logs when verbose logging is enabled for a module. For example,
// VLOG(jni) << "A JNI operation was performed";
#define VLOG(module) if (VLOG_IS_ON(module)) LOG(INFO)

// Holder to implement VLOG_STREAM.
class VlogMessage {
 public:
  // TODO Taken from android_base.
  VlogMessage(bool enable,
              const char* file,
              unsigned int line,
              ::android::base::LogSeverity severity,
              const char* tag,
              int error)
      : msg_(std::in_place_type<std::ostringstream>) {
    if (enable) {
      msg_.emplace<::android::base::LogMessage>(file, line, severity, tag, error);
    }
  }

  std::ostream& stream() {
    if (std::holds_alternative<std::ostringstream>(msg_)) {
      return std::get<std::ostringstream>(msg_);
    } else {
      return std::get<::android::base::LogMessage>(msg_).stream();
    }
  }

 private:
  std::variant<::android::base::LogMessage, std::ostringstream> msg_;
};

// Return the stream associated with logging for the given module. NB Unlike VLOG function calls
// will still be performed. Output will be suppressed if the module is not on.
#define VLOG_STREAM(module)                    \
  ::art::VlogMessage(VLOG_IS_ON(module),       \
                     __FILE__,                 \
                     __LINE__,                 \
                     ::android::base::INFO,    \
                     _LOG_TAG_INTERNAL,        \
                     -1)                       \
      .stream()

// Check whether an implication holds between x and y, LOG(FATAL) if not. The value
// of the expressions x and y is evaluated once. Extra logging can be appended
// using << after. For example:
//
//     CHECK_IMPLIES(1==1, 0==1) results in
//       "Check failed: 1==1 (true) implies 0==1 (false) ".
// clang-format off
#define CHECK_IMPLIES(LHS, RHS)                                                                  \
  LIKELY(!(LHS) || (RHS)) || ABORT_AFTER_LOG_FATAL_EXPR(false) ||                                \
      ::android::base::LogMessage(__FILE__, __LINE__, ::android::base::FATAL, _LOG_TAG_INTERNAL, \
                                  -1)                                                            \
              .stream()                                                                          \
      << "Check failed: " #LHS << " (true) implies " #RHS << " (false)"
// clang-format on

#define DCHECK_IMPLIES(a, b) \
  if (::android::base::kEnableDChecks) CHECK_IMPLIES(a, b)

}  // namespace art

#endif  // ART_LIBARTBASE_BASE_LOGGING_H_
