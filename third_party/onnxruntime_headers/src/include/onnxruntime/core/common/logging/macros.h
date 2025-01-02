// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once
// NOTE: Don't include this file directly. Include logging.h

#define CREATE_MESSAGE(logger, severity, category, datatype) \
  ::onnxruntime::logging::Capture(logger, ::onnxruntime::logging::Severity::k##severity, category, datatype, ORT_WHERE)

/*
  Both printf and stream style logging are supported.
  Not that printf currently has a 2K limit to the message size.

  LOGS_* macros are for stream style
  LOGF_* macros are for printf style

  The Message class captures the log input, and pushes it through the logger in its destructor.

  Use the *FATAL* macros if you want a Severity::kFatal message to also throw.

  There are a few variants to minimize the length of the macro name required in the calling code.
  They are optimized so the shortest names are for the (expected) most common usage. This can be
  tweaked if needed.

  Explicit logger vs LoggingManager::DefaulLogger()
  Default is for a logger instance to be explicitly passed in.
  The logger instance provides an identifier so that log messages from different runs can be separated.

  Variants with DEFAULT in the macro name use the default logger provided by logging manager. This is
  static so accessible from any code, provided a LoggingManager instance created with InstanceType::Default
  exists somewhere. See logging.h for further explanation of the expected setup.

  DataType
  Default uses DataType::SYSTEM.

  Variants with USER in the macro name use DataType::USER. This is data that could be PII, and may need to
  be filtered from output. LoggingManager applies this filtering.

  Category
  Default category is ::onnxruntime::Logging::Category::onnxruntime.

  If you wish to provide a different category, use variants with CATEGORY in the macro name

*/

/**
 * Note:
 * The stream style logging macros (something like `LOGS() << message`) are designed to be appended to.
 * Normally, we can isolate macro code in a separate scope (e.g., `do {...} while(0)`), but here we need the macro code
 * to interact with subsequent code (i.e., the values to log).
 *
 * When an unisolated conditional is involved, extra care needs to be taken to avoid unexpected parsing behavior.
 * For example:
 *
 * if (enabled)
 *   Capture().Stream()
 *
 * is more direct, but
 *
 * if (!enabled) {
 * } else Capture().Stream()
 *
 * ensures that the `if` does not unintentionally associate with a subsequent `else`.
 */

// Logging with explicit category

// iostream style logging. Capture log info in Message, and push to the logger in ~Message.
#define LOGS_CATEGORY(logger, severity, category)                              \
  if (!(logger).OutputIsEnabled(::onnxruntime::logging::Severity::k##severity, \
                                ::onnxruntime::logging::DataType::SYSTEM)) {   \
    /* do nothing */                                                           \
  } else                                                                       \
    CREATE_MESSAGE(logger, severity, category, ::onnxruntime::logging::DataType::SYSTEM).Stream()

#define LOGS_USER_CATEGORY(logger, severity, category)                         \
  if (!(logger).OutputIsEnabled(::onnxruntime::logging::Severity::k##severity, \
                                ::onnxruntime::logging::DataType::USER)) {     \
    /* do nothing */                                                           \
  } else                                                                       \
    CREATE_MESSAGE(logger, severity, category, ::onnxruntime::logging::DataType::USER).Stream()

// printf style logging. Capture log info in Message, and push to the logger in ~Message.
#define LOGF_CATEGORY(logger, severity, category, format_str, ...)                         \
  do {                                                                                     \
    if ((logger).OutputIsEnabled(::onnxruntime::logging::Severity::k##severity,            \
                                 ::onnxruntime::logging::DataType::SYSTEM))                \
      CREATE_MESSAGE(logger, severity, category, ::onnxruntime::logging::DataType::SYSTEM) \
          .CapturePrintf(format_str, ##__VA_ARGS__);                                       \
  } while (0)

#define LOGF_USER_CATEGORY(logger, severity, category, format_str, ...)                  \
  do {                                                                                   \
    if ((logger).OutputIsEnabled(::onnxruntime::logging::Severity::k##severity,          \
                                 ::onnxruntime::logging::DataType::USER))                \
      CREATE_MESSAGE(logger, severity, category, ::onnxruntime::logging::DataType::USER) \
          .CapturePrintf(format_str, ##__VA_ARGS__);                                     \
  } while (0)

// Logging with category of "onnxruntime"

#define LOGS(logger, severity) \
  LOGS_CATEGORY(logger, severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGS_USER(logger, severity) \
  LOGS_USER_CATEGORY(logger, severity, ::onnxruntime::logging::Category::onnxruntime)

// printf style logging. Capture log info in Message, and push to the logger in ~Message.
#define LOGF(logger, severity, format_str, ...) \
  LOGF_CATEGORY(logger, severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

#define LOGF_USER(logger, severity, format_str, ...) \
  LOGF_USER_CATEGORY(logger, severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

/*
  Macros that use the default logger.
  A LoggingManager instance must be currently valid for the default logger to be available.
*/

// Logging with explicit category

#define LOGS_DEFAULT_CATEGORY(severity, category) \
  LOGS_CATEGORY(::onnxruntime::logging::LoggingManager::DefaultLogger(), severity, category)

#define LOGS_USER_DEFAULT_CATEGORY(severity, category) \
  LOGS_USER_CATEGORY(::onnxruntime::logging::LoggingManager::DefaultLogger(), severity, category)

#define LOGF_DEFAULT_CATEGORY(severity, category, format_str, ...) \
  LOGF_CATEGORY(::onnxruntime::logging::LoggingManager::DefaultLogger(), severity, category, format_str, ##__VA_ARGS__)

#define LOGF_USER_DEFAULT_CATEGORY(severity, category, format_str, ...) \
  LOGF_USER_CATEGORY(::onnxruntime::logging::LoggingManager::DefaultLogger(), severity, category, format_str, ##__VA_ARGS__)

// Logging with category of "onnxruntime"

#define LOGS_DEFAULT(severity) \
  LOGS_DEFAULT_CATEGORY(severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGS_USER_DEFAULT(severity) \
  LOGS_USER_DEFAULT_CATEGORY(severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGF_DEFAULT(severity, format_str, ...) \
  LOGF_DEFAULT_CATEGORY(severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

#define LOGF_USER_DEFAULT(severity, format_str, ...) \
  LOGF_USER_DEFAULT_CATEGORY(severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

/*
  Conditional logging
*/

// Logging with explicit category

#define LOGS_CATEGORY_IF(boolean_expression, logger, severity, category) \
  if (!((boolean_expression) == true)) {                                 \
    /* do nothing */                                                     \
  } else                                                                 \
    LOGS_CATEGORY(logger, severity, category)

#define LOGS_DEFAULT_CATEGORY_IF(boolean_expression, severity, category) \
  if (!((boolean_expression) == true)) {                                 \
    /* do nothing */                                                     \
  } else                                                                 \
    LOGS_DEFAULT_CATEGORY(severity, category)

#define LOGS_USER_CATEGORY_IF(boolean_expression, logger, severity, category) \
  if (!((boolean_expression) == true)) {                                      \
    /* do nothing */                                                          \
  } else                                                                      \
    LOGS_USER_CATEGORY(logger, severity, category)

#define LOGS_USER_DEFAULT_CATEGORY_IF(boolean_expression, severity, category) \
  if (!((boolean_expression) == true)) {                                      \
    /* do nothing */                                                          \
  } else                                                                      \
    LOGS_USER_DEFAULT_CATEGORY(severity, category)

#define LOGF_CATEGORY_IF(boolean_expression, logger, severity, category, format_str, ...)                   \
  do {                                                                                                      \
    if ((boolean_expression) == true) LOGF_CATEGORY(logger, severity, category, format_str, ##__VA_ARGS__); \
  } while (0)

#define LOGF_DEFAULT_CATEGORY_IF(boolean_expression, severity, category, format_str, ...)                   \
  do {                                                                                                      \
    if ((boolean_expression) == true) LOGF_DEFAULT_CATEGORY(severity, category, format_str, ##__VA_ARGS__); \
  } while (0)

#define LOGF_USER_CATEGORY_IF(boolean_expression, logger, severity, category, format_str, ...)                   \
  do {                                                                                                           \
    if ((boolean_expression) == true) LOGF_USER_CATEGORY(logger, severity, category, format_str, ##__VA_ARGS__); \
  } while (0)

#define LOGF_USER_DEFAULT_CATEGORY_IF(boolean_expression, severity, category, format_str, ...)                   \
  do {                                                                                                           \
    if ((boolean_expression) == true) LOGF_USER_DEFAULT_CATEGORY(severity, category, format_str, ##__VA_ARGS__); \
  } while (0)

// Logging with category of "onnxruntime"

#define LOGS_IF(boolean_expression, logger, severity) \
  LOGS_CATEGORY_IF(boolean_expression, logger, severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGS_DEFAULT_IF(boolean_expression, severity) \
  LOGS_DEFAULT_CATEGORY_IF(boolean_expression, severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGS_USER_IF(boolean_expression, logger, severity) \
  LOGS_USER_CATEGORY_IF(boolean_expression, logger, severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGS_USER_DEFAULT_IF(boolean_expression, severity) \
  LOGS_USER_DEFAULT_CATEGORY_IF(boolean_expression, severity, ::onnxruntime::logging::Category::onnxruntime)

#define LOGF_IF(boolean_expression, logger, severity, format_str, ...) \
  LOGF_CATEGORY_IF(boolean_expression, logger, severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

#define LOGF_DEFAULT_IF(boolean_expression, severity, format_str, ...) \
  LOGF_DEFAULT_CATEGORY_IF(boolean_expression, severity, ::onnxruntime::logging::Category::onnxruntime, format_str, ##__VA_ARGS__)

#define LOGF_USER_IF(boolean_expression, logger, severity, format_str, ...)                                  \
  LOGF_USER_CATEGORY_IF(boolean_expression, logger, severity, ::onnxruntime::logging::Category::onnxruntime, \
                        format_str, ##__VA_ARGS__)

#define LOGF_USER_DEFAULT_IF(boolean_expression, severity, format_str, ...)                                  \
  LOGF_USER_DEFAULT_CATEGORY_IF(boolean_expression, severity, ::onnxruntime::logging::Category::onnxruntime, \
                                format_str, ##__VA_ARGS__)

/*
  Debug verbose logging of caller provided level.
  Disabled in Release builds.
  Use the _USER variants for VLOG statements involving user data that may need to be filtered.
*/
#ifndef NDEBUG
#define VLOGS(logger, level)                                                         \
  if (!(::onnxruntime::logging::vlog_enabled && level <= (logger).VLOGMaxLevel())) { \
    /* do nothing */                                                                 \
  } else                                                                             \
    LOGS_CATEGORY(logger, VERBOSE, "VLOG" #level)

#define VLOGS_USER(logger, level)                                                    \
  if (!(::onnxruntime::logging::vlog_enabled && level <= (logger).VLOGMaxLevel())) { \
    /* do nothing */                                                                 \
  } else                                                                             \
    LOGS_USER_CATEGORY(logger, VERBOSE, "VLOG" #level)

#define VLOGF(logger, level, format_str, ...)                                     \
  do {                                                                            \
    if (::onnxruntime::logging::vlog_enabled && level <= (logger).VLOGMaxLevel()) \
      LOGF_CATEGORY(logger, VERBOSE, "VLOG" #level, format_str, ##__VA_ARGS__);   \
  } while (0)

#define VLOGF_USER(logger, level, format_str, ...)                                   \
  do {                                                                               \
    if (::onnxruntime::logging::vlog_enabled && level <= (logger).VLOGMaxLevel())    \
      LOGF_USER_CATEGORY(logger, VERBOSE, "VLOG" #level, format_str, ##__VA_ARGS__); \
  } while (0)
#else
// Disabled in Release builds.
#define VLOGS(logger, level) \
  if constexpr (true) {      \
  } else                     \
    LOGS_CATEGORY(logger, VERBOSE, "VLOG" #level)
#define VLOGS_USER(logger, level) \
  if constexpr (true) {           \
  } else                          \
    LOGS_USER_CATEGORY(logger, VERBOSE, "VLOG" #level)
#define VLOGF(logger, level, format_str, ...)
#define VLOGF_USER(logger, level, format_str, ...)
#endif

// Default logger variants
#define VLOGS_DEFAULT(level) \
  VLOGS(::onnxruntime::logging::LoggingManager::DefaultLogger(), level)

#define VLOGS_USER_DEFAULT(level) \
  VLOGS_USER(::onnxruntime::logging::LoggingManager::DefaultLogger(), level)

#define VLOGF_DEFAULT(level, format_str, ...) \
  VLOGF(::onnxruntime::logging::LoggingManager::DefaultLogger(), level, format_str, ##__VA_ARGS__)

#define VLOGF_USER_DEFAULT(level, format_str, ...) \
  VLOGF_USER(::onnxruntime::logging::LoggingManager::DefaultLogger(), level, format_str, ##__VA_ARGS__)
