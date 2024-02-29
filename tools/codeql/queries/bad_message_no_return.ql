// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp
import lib.Chromium
import lib.CommonPatterns

/**
 * @name potential ReceivedBadMessage call without return.
 * @description Detects instances where bad_message::ReceivedBadMessage
 *              is called and not immediately followed by a return statement.
 * @kind problem
 * @problem.severity warning
 * @id cpp/report-bad-message-without-return
 */

class BadMessageCall extends FunctionCall {
  BadMessageCall() {

    // bad_message::ReceivedBadMessage
    this.getTarget().hasQualifiedName("bad_message", "ReceivedBadMessage") or

    // content::bad_message::ReceivedBadMessage
    this.getTarget().hasQualifiedName("content::bad_message", "ReceivedBadMessage")
  }
}

from BadMessageCall call
where
  Chromium::isChromiumCode(call) and
  CommonPatterns::isCallNotFollowedByReturn(call)
select call,
  call.getLocation().getFile().getRelativePath() + ":" + call.getLocation().getStartLine().toString()
