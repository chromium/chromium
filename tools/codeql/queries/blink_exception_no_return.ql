// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp
import lib.Chromium
import lib.CommonPatterns

/**
 * @name Throw*Exception call without return.
 * @description Detects instances where Throw*Exception is called
 *              but not immediately followed by a return statement.
 * @kind problem
 * @problem.severity warning
 * @id cpp/throw-exception-without-return
 */

class ThrowExceptionCall extends FunctionCall {
  ThrowExceptionCall() {
    this.getTarget().hasName("ThrowDOMException") or
    this.getTarget().hasName("ThrowSecurityError") or
    this.getTarget().hasName("ThrowRangeError") or
    this.getTarget().hasName("ThrowTypeError")
  }
}

from ThrowExceptionCall call, Function f
where
  Chromium::isBlinkCode(call) and
  CommonPatterns::isCallNotFollowedByReturn(call)
select call,
  call.getLocation().getFile().getRelativePath() + ":" + call.getLocation().getStartLine().toString()
