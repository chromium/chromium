// Copyright 2020 The Crashpad Authors. All rights reserved.
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

// std::unexpected_handler is deprecated starting in C++11, and removed in
// C++17.  But macOS versions we run on still ship it. This define makes
// std::unexpected_handler reappear. If that define ever stops working,
// we hopefully no longer run on macOS versions that still have it.
// (...or we'll have to define it in this file instead of getting it from
// <exception>). This define must before all includes.
#define _LIBCPP_ENABLE_CXX17_REMOVED_UNEXPECTED_FUNCTIONS

#include "client/ios_handler/exception_processor.h"

#include <Availability.h>
#import <Foundation/Foundation.h>
#include <TargetConditionals.h>
#include <cxxabi.h>
#include <dlfcn.h>
#include <libunwind.h>
#include <mach-o/loader.h>
#include <objc/message.h>
#include <objc/objc-exception.h>
#include <objc/objc.h>
#include <objc/runtime.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <unwind.h>

#include <exception>
#include <type_traits>
#include <typeinfo>

#include "base/bit_cast.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/memory/free_deleter.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "build/build_config.h"
#include "client/annotation.h"
#include "client/simulate_crash_ios.h"

namespace {

// From 10.15.0 objc4-779.1/runtime/objc-exception.mm.
struct objc_typeinfo {
  const void* const* vtable;
  const char* name;
  Class cls_unremapped;
};
struct objc_exception {
  id obj;
  objc_typeinfo tinfo;
};

// From 10.15.0 objc4-779.1/runtime/objc-abi.h.
extern "C" const void* const objc_ehtype_vtable[];

// https://github.com/llvm/llvm-project/blob/09dc884eb2e4/libcxxabi/src/cxa_exception.h
static const uint64_t kOurExceptionClass = 0x434c4e47432b2b00;
struct __cxa_exception {
#if defined(ARCH_CPU_64_BITS)
  void* reserve;
  size_t referenceCount;
#endif
  std::type_info* exceptionType;
  void (*exceptionDestructor)(void*);
  std::unexpected_handler unexpectedHandler;
  std::terminate_handler terminateHandler;
  __cxa_exception* nextException;
  int handlerCount;
  int handlerSwitchValue;
  const unsigned char* actionRecord;
  const unsigned char* languageSpecificData;
  void* catchTemp;
  void* adjustedPtr;
#if !defined(ARCH_CPU_64_BITS)
  size_t referenceCount;
#endif
  _Unwind_Exception unwindHeader;
};

int LoggingUnwStep(unw_cursor_t* cursor) {
  int rv = unw_step(cursor);
  if (rv < 0) {
    LOG(ERROR) << "unw_step: " << rv;
  }
  return rv;
}

std::string FormatStackTrace(const std::vector<uint64_t>& addresses,
                             size_t max_length) {
  std::string stack_string;
  for (uint64_t address : addresses) {
    std::string address_string = base::StringPrintf("0x%" PRIx64, address);
    if (stack_string.size() + address_string.size() > max_length)
      break;
    stack_string += address_string + " ";
  }

  if (!stack_string.empty() && stack_string.back() == ' ') {
    stack_string.resize(stack_string.size() - 1);
  }

  return stack_string;
}

std::string GetTraceString() {
  std::vector<uint64_t> addresses;
  unw_context_t context;
  unw_getcontext(&context);
  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);
  while (LoggingUnwStep(&cursor) > 0) {
    unw_word_t ip = 0;
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    addresses.push_back(ip);
  }
  return FormatStackTrace(addresses, 1024);
}

crashpad::ObjcExceptionDelegate* g_exception_delegate;
objc_exception_preprocessor g_next_preprocessor;
NSUncaughtExceptionHandler* g_next_uncaught_exception_handler;

static void SetNSExceptionAnnotations(NSException* exception,
                                      std::string& name,
                                      std::string& reason) {
  @try {
    name = base::SysNSStringToUTF8(exception.name);
    static crashpad::StringAnnotation<256> nameKey("exceptionName");
    nameKey.Set(name);
  } @catch (id name_exception) {
    LOG(ERROR) << "Unable to read uncaught Objective-C exception name.";
  }

  @try {
    reason = base::SysNSStringToUTF8(exception.reason);
    static crashpad::StringAnnotation<512> reasonKey("exceptionReason");
    reasonKey.Set(reason);
  } @catch (id reason_exception) {
    LOG(ERROR) << "Unable to read uncaught Objective-C exception reason.";
  }

  @try {
    if (exception.userInfo) {
      static crashpad::StringAnnotation<512> userInfoKey("exceptionUserInfo");
      userInfoKey.Set(base::SysNSStringToUTF8(
          [NSString stringWithFormat:@"%@", exception.userInfo]));
    }
  } @catch (id user_info_exception) {
    LOG(ERROR) << "Unable to read uncaught Objective-C exception user info.";
  }
}

static void ObjcUncaughtExceptionHandler(NSException* exception) {
  std::string name, reason;
  SetNSExceptionAnnotations(exception, name, reason);
  NSArray<NSNumber*>* addressArray = [exception callStackReturnAddresses];
  if ([addressArray count] > 0) {
    static crashpad::StringAnnotation<256> nameKey("UncaughtNSException");
    nameKey.Set("true");
    std::vector<uint64_t> addresses;
    for (NSNumber* address in addressArray)
      addresses.push_back([address unsignedLongLongValue]);
    g_exception_delegate->HandleUncaughtNSException(&addresses[0],
                                                    addresses.size());
  } else {
    LOG(WARNING) << "Uncaught Objective-C exception name: " << name
                 << " reason: " << reason << " with no "
                 << " -callStackReturnAddresses.";
    crashpad::NativeCPUContext cpu_context;
    crashpad::CaptureContext(&cpu_context);
    g_exception_delegate->HandleUncaughtNSExceptionWithContext(&cpu_context);
  }
}

// This function is used to make it clear to the crash processor that an
// uncaught NSException was recorded here.
static __attribute__((noinline)) id HANDLE_UNCAUGHT_NSEXCEPTION(
    id exception,
    const char* sinkhole) {
  std::string name, reason;
  SetNSExceptionAnnotations(exception, name, reason);
  LOG(WARNING) << "Handling Objective-C exception name: " << name
               << " reason: " << reason << " with sinkhole: " << sinkhole;
  crashpad::NativeCPUContext cpu_context;
  crashpad::CaptureContext(&cpu_context);
  g_exception_delegate->HandleUncaughtNSExceptionWithContext(&cpu_context);

  // Remove the uncaught exception handler so we don't record this twice.
  NSSetUncaughtExceptionHandler(g_next_uncaught_exception_handler);
  g_next_uncaught_exception_handler = nullptr;

  return g_next_preprocessor ? g_next_preprocessor(exception) : exception;
}

// Returns true if |path| equals |sinkhole| on device. Simulator paths prepend
// much of Xcode's internal structure, so check that |path| ends with |sinkhole|
// for simulator.
bool ModulePathMatchesSinkhole(const char* path, const char* sinkhole) {
#if TARGET_OS_SIMULATOR
  size_t path_length = strlen(path);
  size_t sinkhole_length = strlen(sinkhole);
  if (sinkhole_length > path_length)
    return false;
  return strncmp(path + path_length - sinkhole_length,
                 sinkhole,
                 sinkhole_length) == 0;
#else
  return strcmp(path, sinkhole) == 0;
#endif
}

id ObjcExceptionPreprocessor(id exception) {
  static bool seen_first_exception;

  static crashpad::StringAnnotation<256> firstexception("firstexception");
  static crashpad::StringAnnotation<256> lastexception("lastexception");
  static crashpad::StringAnnotation<1024> firstexception_bt(
      "firstexception_bt");
  static crashpad::StringAnnotation<1024> lastexception_bt("lastexception_bt");
  auto* key = seen_first_exception ? &lastexception : &firstexception;
  auto* bt_key = seen_first_exception ? &lastexception_bt : &firstexception_bt;
  NSString* value = [NSString
      stringWithFormat:@"%@ reason %@", [exception name], [exception reason]];
  key->Set(base::SysNSStringToUTF8(value));

  // This exception preprocessor runs prior to the one in libobjc, which sets
  // the -[NSException callStackReturnAddresses].
  bt_key->Set(GetTraceString());
  seen_first_exception = true;

  // Unwind the stack looking for any exception handlers. If an exception
  // handler is encountered, test to see if it is a function known to catch-
  // and-rethrow as a "top-level" exception handler. Various routines in
  // Cocoa/UIKit do this, and it obscures the crashing stack, since the original
  // throw location is no longer present on the stack (just the re-throw) when
  // Crashpad captures the crash report.
  unw_context_t context;
  unw_getcontext(&context);

  unw_cursor_t cursor;
  unw_init_local(&cursor, &context);

  static const void* this_base_address = []() -> const void* {
    Dl_info dl_info;
    if (!dladdr(reinterpret_cast<const void*>(&ObjcExceptionPreprocessor),
                &dl_info)) {
      LOG(ERROR) << "dladdr: " << dlerror();
      return nullptr;
    }
    return dl_info.dli_fbase;
  }();

  // Generate an exception_header for the __personality_routine.
  // From 10.15.0 objc4-779.1/runtime/objc-exception.mm objc_exception_throw.
  objc_exception* exception_objc = reinterpret_cast<objc_exception*>(
      __cxxabiv1::__cxa_allocate_exception(sizeof(objc_exception)));
  exception_objc->obj = exception;
  exception_objc->tinfo.vtable = objc_ehtype_vtable + 2;
  exception_objc->tinfo.name = object_getClassName(exception);
  exception_objc->tinfo.cls_unremapped = object_getClass(exception);

  // https://github.com/llvm/llvm-project/blob/c5d2746fbea7/libcxxabi/src/cxa_exception.cpp
  // __cxa_throw
  __cxa_exception* exception_header =
      reinterpret_cast<__cxa_exception*>(exception_objc) - 1;
  exception_header->unexpectedHandler = std::get_unexpected();
  exception_header->terminateHandler = std::get_terminate();
  exception_header->exceptionType =
      reinterpret_cast<std::type_info*>(&exception_objc->tinfo);
  exception_header->unwindHeader.exception_class = kOurExceptionClass;

  bool handler_found = false;
  while (LoggingUnwStep(&cursor) > 0) {
    unw_proc_info_t frame_info;
    if (unw_get_proc_info(&cursor, &frame_info) != UNW_ESUCCESS) {
      continue;
    }

    if (frame_info.handler == 0) {
      continue;
    }

    // Check to see if the handler is really an exception handler.
#if defined(__IPHONE_14_5) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_14_5
    using personality_routine = _Unwind_Personality_Fn;
#else
    using personality_routine = __personality_routine;
#endif
    personality_routine p =
        reinterpret_cast<personality_routine>(frame_info.handler);

    // From 10.15.0 libunwind-35.4/src/UnwindLevel1.c.
    _Unwind_Reason_Code personalityResult = (*p)(
        1,
        _UA_SEARCH_PHASE,
        exception_header->unwindHeader.exception_class,
        reinterpret_cast<_Unwind_Exception*>(&exception_header->unwindHeader),
        reinterpret_cast<_Unwind_Context*>(&cursor));
    switch (personalityResult) {
      case _URC_HANDLER_FOUND:
        break;
      case _URC_CONTINUE_UNWIND:
        continue;
      default:
        break;
    }

    char proc_name[512];
    unw_word_t offset;
    if (unw_get_proc_name(&cursor, proc_name, sizeof(proc_name), &offset) !=
        UNW_ESUCCESS) {
      // The symbol has no name, so see if it belongs to the same image as
      // this function.
      Dl_info dl_info;
      if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip),
                 &dl_info)) {
        if (dl_info.dli_fbase == this_base_address) {
          // This is a handler in our image, so allow it to run.
          handler_found = true;
          break;
        }
      }

      // This handler does not belong to us, so continue the search.
      continue;
    }

    // Check if the function is one that is known to obscure (by way of
    // catch-and-rethrow) exception stack traces. If it is, sinkhole it
    // by crashing here at the point of throw.
    static constexpr const char* kExceptionSymbolNameSinkholes[] = {
        // The two CF symbol names will also be captured by the CoreFoundation
        // library path check below, but for completeness they are listed here,
        // since they appear unredacted.
        "CFRunLoopRunSpecific",
        "_CFXNotificationPost",
        "__NSFireDelayedPerform",
        // If this exception is going to end up at EHFrame, record the uncaught
        // exception instead.
        "_ZN4base3mac15CallWithEHFrameEU13block_pointerFvvE",
    };
    for (const char* sinkhole : kExceptionSymbolNameSinkholes) {
      if (strcmp(sinkhole, proc_name) == 0) {
        return HANDLE_UNCAUGHT_NSEXCEPTION(exception, sinkhole);
      }
    }

    // On iOS, function names are often reported as "<redacted>", although they
    // do appear when attached to the debugger.  When this happens, use the path
    // of the image to determine if the handler is an exception sinkhole.
    static constexpr const char* kExceptionLibraryPathSinkholes[] = {
        // Everything in this library is a sinkhole, specifically
        // _dispatch_client_callout.  Both are needed here depending on whether
        // the debugger is attached (introspection only appears when a simulator
        // is attached to a debugger).
        "/usr/lib/system/introspection/libdispatch.dylib",
        "/usr/lib/system/libdispatch.dylib",

        // __CFRunLoopDoTimers and __CFRunLoopRun are sinkholes. Consider also
        // checking that a few frames up is CFRunLoopRunSpecific().
        "/System/Library/Frameworks/CoreFoundation.framework/CoreFoundation",
    };

    Dl_info dl_info;
    if (dladdr(reinterpret_cast<const void*>(frame_info.start_ip), &dl_info) !=
        0) {
      for (const char* sinkhole : kExceptionLibraryPathSinkholes) {
        if (ModulePathMatchesSinkhole(dl_info.dli_fname, sinkhole)) {
          return HANDLE_UNCAUGHT_NSEXCEPTION(exception, sinkhole);
        }
      }

      // Another set of iOS redacted sinkholes appear in CoreAutoLayout.
      // However, this is often called by client code, so it's unsafe to simply
      // handle an uncaught nsexception here. Instead, skip the frame and
      // continue searching for either a handler that belongs to us, or another
      // sinkhole. See:
      //    -[NSISEngine
      //    performModifications:withUnsatisfiableConstraintsHandler:]:
      //    -[NSISEngine withBehaviors:performModifications:]
      //    +[NSLayoutConstraintParser
      //    constraintsWithVisualFormat:options:metrics:views:]:
      static constexpr const char* kCoreAutoLayoutSinkhole =
          "/System/Library/PrivateFrameworks/CoreAutoLayout.framework/"
          "CoreAutoLayout";
      if (ModulePathMatchesSinkhole(dl_info.dli_fname,
                                    kCoreAutoLayoutSinkhole)) {
        continue;
      }
    }

    // Some <redacted> sinkholes are harder to find. _UIGestureEnvironmentUpdate
    // in UIKitCore is an example. UIKitCore can't be added to
    // kExceptionLibraryPathSinkholes because it uses Objective-C exceptions
    // internally and also has has non-sinkhole handlers. While all the
    // calling methods in UIKit are marked <redacted> starting in iOS14, it's
    // currently true that all callers to _UIGestureEnvironmentUpdate are within
    // UIWindow sendEvent -> UIGestureEnvironment.  That means a very hacky way
    // to detect this is to check if the calling (2x) method IMP is within the
    // range of all UIWindow methods.
    static constexpr const char kUIKitCorePath[] =
        "/System/Library/PrivateFrameworks/UIKitCore.framework/UIKitCore";
    if (ModulePathMatchesSinkhole(dl_info.dli_fname, kUIKitCorePath)) {
      unw_proc_info_t caller_frame_info;
      if (LoggingUnwStep(&cursor) > 0 &&
          unw_get_proc_info(&cursor, &caller_frame_info) == UNW_ESUCCESS &&
          LoggingUnwStep(&cursor) > 0 &&
          unw_get_proc_info(&cursor, &caller_frame_info) == UNW_ESUCCESS) {
        auto uiwindowimp_lambda = [](IMP* max) {
          IMP min = *max = bit_cast<IMP>(nullptr);
          unsigned int method_count = 0;
          std::unique_ptr<Method[], base::FreeDeleter> method_list(
              class_copyMethodList(NSClassFromString(@"UIWindow"),
                                   &method_count));
          if (method_count > 0) {
            min = *max = method_getImplementation(method_list[0]);
            for (unsigned int method_index = 1; method_index < method_count;
                 method_index++) {
              IMP method_imp =
                  method_getImplementation(method_list[method_index]);
              *max = std::max(method_imp, *max);
              min = std::min(method_imp, min);
            }
          }
          return min;
        };

        static IMP uiwindow_max_imp;
        static IMP uiwindow_min_imp = uiwindowimp_lambda(&uiwindow_max_imp);

        if (uiwindow_min_imp && uiwindow_max_imp &&
            caller_frame_info.start_ip >=
                reinterpret_cast<unw_word_t>(uiwindow_min_imp) &&
            caller_frame_info.start_ip <=
                reinterpret_cast<unw_word_t>(uiwindow_max_imp)) {
          return HANDLE_UNCAUGHT_NSEXCEPTION(exception,
                                             "_UIGestureEnvironmentUpdate");
        }
      }
    }

    handler_found = true;

    break;
  }

  // If no handler is found, __cxa_throw would call failed_throw and terminate.
  // See:
  // https://github.com/llvm/llvm-project/blob/c5d2746fbea7/libcxxabi/src/cxa_exception.cpp
  // __cxa_throw. Instead, call HANDLE_UNCAUGHT_NSEXCEPTION so the exception
  // name and reason are properly recorded.
  if (!handler_found) {
    return HANDLE_UNCAUGHT_NSEXCEPTION(exception, "__cxa_throw");
  }

  // Forward to the next preprocessor.
  return g_next_preprocessor ? g_next_preprocessor(exception) : exception;
}

}  // namespace

namespace crashpad {

void InstallObjcExceptionPreprocessor(ObjcExceptionDelegate* delegate) {
  DCHECK(!g_next_preprocessor);

  // Preprocessor.
  g_next_preprocessor =
      objc_setExceptionPreprocessor(&ObjcExceptionPreprocessor);

  // Uncaught processor.
  g_exception_delegate = delegate;
  g_next_uncaught_exception_handler = NSGetUncaughtExceptionHandler();
  NSSetUncaughtExceptionHandler(&ObjcUncaughtExceptionHandler);
}

void UninstallObjcExceptionPreprocessor() {
  DCHECK(g_next_preprocessor);

  objc_setExceptionPreprocessor(g_next_preprocessor);
  g_exception_delegate = nullptr;

  NSSetUncaughtExceptionHandler(g_next_uncaught_exception_handler);
  g_next_uncaught_exception_handler = nullptr;

  g_next_preprocessor = nullptr;
}

}  // namespace crashpad
