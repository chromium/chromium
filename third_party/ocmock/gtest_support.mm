// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/ocmock/gtest_support.h"

#import <Foundation/Foundation.h>

#import "third_party/ocmock/OCMock/OCMock.h"

// When C++ exceptions are disabled, the C++ library defines |try| and
// |catch| so as to allow exception-expecting C++ code to build properly when
// language support for exceptions is not present.  These macros interfere
// with the use of |@try| and |@catch| in Objective-C files such as this one.
// Undefine these macros here, after everything has been #included, since
// there will be no C++ uses and only Objective-C uses from this point on.
#undef try
#undef catch

namespace testing {
namespace internal {

bool VerifyOCMock(OCMockObject* mock, const char* file, int line) {
  bool result = true;
  @try {
    [mock verify];
  } @catch (NSException* e) {
    result = false;
    ADD_FAILURE_AT(file, line) << "OCMock validation failed: "
                               << [[e description] UTF8String];
  }
  return result;
}

}  // namespace internal
}  // namespace testing
