/**
 * Copyright (c) 2016-present, Facebook, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
// Portions Copyright (c) Microsoft Corporation

#pragma once

#include <climits>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <numeric>
#include <set>
#include <sstream>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/common/code_location.h"
#include "core/common/exceptions.h"
#include "core/common/make_string.h"
#include "core/common/status.h"

namespace onnxruntime {

using TimePoint = std::chrono::high_resolution_clock::time_point;

#ifdef _WIN32
#define ORT_UNUSED_PARAMETER(x) (x)
#else
#define ORT_UNUSED_PARAMETER(x) (void)(x)
#endif

#ifndef ORT_HAVE_ATTRIBUTE
#ifdef __has_attribute
#define ORT_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define ORT_HAVE_ATTRIBUTE(x) 0
#endif
#endif

// ORT_ATTRIBUTE_UNUSED
//
// Prevents the compiler from complaining about or optimizing away variables
// that appear unused on Linux
#if ORT_HAVE_ATTRIBUTE(unused) || (defined(__GNUC__) && !defined(__clang__))
#undef ORT_ATTRIBUTE_UNUSED
#define ORT_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define ORT_ATTRIBUTE_UNUSED
#endif

#ifdef ORT_NO_EXCEPTIONS
// Print the given final message, the message must be a null terminated char*
// ORT will abort after printing the message.
// For Android, will print to Android system log
// For other platforms, will print to stderr
void PrintFinalMessage(const char* msg);
#endif

// macro to explicitly ignore the return value from a function call so Code Analysis doesn't complain
#define ORT_IGNORE_RETURN_VALUE(fn) \
  static_cast<void>(fn)

std::vector<std::string> GetStackTrace();
// these is a helper function that gets defined by platform/Telemetry
void LogRuntimeError(uint32_t session_id, const common::Status& status, const char* file,
                     const char* function, uint32_t line);

// __PRETTY_FUNCTION__ isn't a macro on gcc, so use a check for _MSC_VER
// so we only define it as one for MSVC
#if (_MSC_VER && !defined(__PRETTY_FUNCTION__))
#define __PRETTY_FUNCTION__ __FUNCTION__
#endif

// Capture where a message is coming from. Use __FUNCTION__ rather than the much longer __PRETTY_FUNCTION__
#define ORT_WHERE ::onnxruntime::CodeLocation(__FILE__, __LINE__, static_cast<const char*>(__FUNCTION__))

#define ORT_WHERE_WITH_STACK \
  ::onnxruntime::CodeLocation(__FILE__, __LINE__, static_cast<const char*>(__PRETTY_FUNCTION__), ::onnxruntime::GetStackTrace())

#ifdef ORT_NO_EXCEPTIONS

#define ORT_TRY if (true)
#define ORT_CATCH(x) else if (false)
#define ORT_RETHROW

// In order to ignore the catch statement when a specific exception (not ... ) is caught and referred
// in the body of the catch statements, it is necessary to wrap the body of the catch statement into
// a lambda function. otherwise the exception referred will be undefined and cause build break
#define ORT_HANDLE_EXCEPTION(func)

// Throw an exception with optional message.
// NOTE: The arguments get streamed into a string via ostringstream::operator<<
// DO NOT use a printf format string, as that will not work as you expect.
#define ORT_THROW(...)                                                    \
  do {                                                                    \
    ::onnxruntime::PrintFinalMessage(                                     \
        ::onnxruntime::OnnxRuntimeException(                              \
            ORT_WHERE_WITH_STACK, ::onnxruntime::MakeString(__VA_ARGS__)) \
            .what());                                                     \
    abort();                                                              \
  } while (false)

// Just in order to mark things as not implemented. Do not use in final code.
#define ORT_NOT_IMPLEMENTED(...)                                                       \
  do {                                                                                 \
    ::onnxruntime::PrintFinalMessage(                                                  \
        ::onnxruntime::NotImplementedException(::onnxruntime::MakeString(__VA_ARGS__)) \
            .what());                                                                  \
    abort();                                                                           \
  } while (false)

// Check condition.
// NOTE: The arguments get streamed into a string via ostringstream::operator<<
// DO NOT use a printf format string, as that will not work as you expect.
#define ORT_ENFORCE(condition, ...)                                                   \
  do {                                                                                \
    if (!(condition)) {                                                               \
      ::onnxruntime::PrintFinalMessage(                                               \
          ::onnxruntime::OnnxRuntimeException(ORT_WHERE_WITH_STACK, #condition,       \
                                              ::onnxruntime::MakeString(__VA_ARGS__)) \
              .what());                                                               \
      abort();                                                                        \
    }                                                                                 \
  } while (false)

#define ORT_THROW_EX(ex, ...)                                                                      \
  do {                                                                                             \
    ::onnxruntime::PrintFinalMessage(                                                              \
        ::onnxruntime::MakeString(#ex, "(", ::onnxruntime::MakeString(__VA_ARGS__), ")").c_str()); \
    abort();                                                                                       \
  } while (false)

#else

#define ORT_TRY try
#define ORT_CATCH(x) catch (x)
#define ORT_RETHROW throw;

#define ORT_HANDLE_EXCEPTION(func) func()

// Throw an exception with optional message.
// NOTE: The arguments get streamed into a string via ostringstream::operator<<
// DO NOT use a printf format string, as that will not work as you expect.
#define ORT_THROW(...) \
  throw ::onnxruntime::OnnxRuntimeException(ORT_WHERE_WITH_STACK, ::onnxruntime::MakeString(__VA_ARGS__))

// Just in order to mark things as not implemented. Do not use in final code.
#define ORT_NOT_IMPLEMENTED(...) \
  throw ::onnxruntime::NotImplementedException(::onnxruntime::MakeString(__VA_ARGS__))

// Check condition.
// NOTE: The arguments get streamed into a string via ostringstream::operator<<
// DO NOT use a printf format string, as that will not work as you expect.
#define ORT_ENFORCE(condition, ...)                                                      \
  do {                                                                                   \
    if (!(condition)) {                                                                  \
      throw ::onnxruntime::OnnxRuntimeException(ORT_WHERE_WITH_STACK, #condition,        \
                                                ::onnxruntime::MakeString(__VA_ARGS__)); \
    }                                                                                    \
  } while (false)

#define ORT_THROW_EX(ex, ...) \
  throw ex(__VA_ARGS__)

#endif

#define ORT_MAKE_STATUS(category, code, ...)                     \
  ::onnxruntime::common::Status(::onnxruntime::common::category, \
                                ::onnxruntime::common::code,     \
                                ::onnxruntime::MakeString(__VA_ARGS__))

// Check condition. if met, return status.
#define ORT_RETURN_IF(condition, ...)                                                                          \
  do {                                                                                                         \
    if (condition) {                                                                                           \
      return ::onnxruntime::common::Status(::onnxruntime::common::ONNXRUNTIME,                                 \
                                           ::onnxruntime::common::FAIL,                                        \
                                           ::onnxruntime::MakeString(ORT_WHERE.ToString(), " ", __VA_ARGS__)); \
    }                                                                                                          \
  } while (false)

// Check condition. if not met, return status.
#define ORT_RETURN_IF_NOT(condition, ...) \
  ORT_RETURN_IF(!(condition), __VA_ARGS__)

// Macros to disable the copy and/or move ctor and assignment methods
// These are usually placed in the private: declarations for a class.

#define ORT_DISALLOW_COPY(TypeName) TypeName(const TypeName&) = delete

#define ORT_DISALLOW_ASSIGNMENT(TypeName) TypeName& operator=(const TypeName&) = delete

#define ORT_DISALLOW_COPY_AND_ASSIGNMENT(TypeName) \
  ORT_DISALLOW_COPY(TypeName);                     \
  ORT_DISALLOW_ASSIGNMENT(TypeName)

#define ORT_DISALLOW_MOVE(TypeName) \
  TypeName(TypeName&&) = delete;    \
  TypeName& operator=(TypeName&&) = delete

#define ORT_DISALLOW_COPY_ASSIGNMENT_AND_MOVE(TypeName) \
  ORT_DISALLOW_COPY_AND_ASSIGNMENT(TypeName);           \
  ORT_DISALLOW_MOVE(TypeName)

#define ORT_RETURN_IF_ERROR_SESSIONID(expr, session_id)                                                                \
  do {                                                                                                                 \
    auto _status = (expr);                                                                                             \
    if ((!_status.IsOK())) {                                                                                           \
      ::onnxruntime::LogRuntimeError(session_id, _status, __FILE__, static_cast<const char*>(__FUNCTION__), __LINE__); \
      return _status;                                                                                                  \
    }                                                                                                                  \
  } while (0)

#define ORT_RETURN_IF_ERROR_SESSIONID_(expr) ORT_RETURN_IF_ERROR_SESSIONID(expr, session_id_)
#define ORT_RETURN_IF_ERROR(expr) ORT_RETURN_IF_ERROR_SESSIONID(expr, 0)

#define ORT_THROW_IF_ERROR(expr)                                                                              \
  do {                                                                                                        \
    auto _status = (expr);                                                                                    \
    if ((!_status.IsOK())) {                                                                                  \
      ::onnxruntime::LogRuntimeError(0, _status, __FILE__, static_cast<const char*>(__FUNCTION__), __LINE__); \
      ORT_THROW(_status);                                                                                     \
    }                                                                                                         \
  } while (0)

// use this macro when cannot early return
#define ORT_CHECK_AND_SET_RETVAL(expr) \
  do {                                 \
    if (retval.IsOK()) {               \
      retval = (expr);                 \
    }                                  \
  } while (0)

inline long long TimeDiffMicroSeconds(TimePoint start_time) {
  auto end_time = std::chrono::high_resolution_clock::now();
  return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

inline long long TimeDiffMicroSeconds(TimePoint start_time, TimePoint end_time) {
  return std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
}

struct null_type {};
inline std::string ToUTF8String(const std::string& s) { return s; }
#ifdef _WIN32
/**
 * Convert a wide character string to a UTF-8 string
 */
std::string ToUTF8String(const std::wstring& s);

std::wstring ToWideString(const std::string& s);
inline std::wstring ToWideString(const std::wstring& s) { return s; }
#else
inline std::string ToWideString(const std::string& s) { return s; }
#endif

constexpr size_t kMaxStrLen = 2048;

// Returns whether `key` is in `container`.
// Like C++20's map/set contains() member function.
template <typename Key, typename... OtherContainerArgs,
          template <typename...> typename AssociativeContainer,
          typename LookupKey>
inline bool Contains(const AssociativeContainer<Key, OtherContainerArgs...>& container, LookupKey&& key) {
  return container.find(std::forward<LookupKey>(key)) != container.end();
}

}  // namespace onnxruntime
