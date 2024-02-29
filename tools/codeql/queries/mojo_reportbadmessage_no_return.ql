// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import cpp
import lib.Chromium
import lib.CommonPatterns

/**
 * @name potential ReportBadMessage call without return.
 * @description Detects instances where mojo::ReportBadMessage is called
 *              but not immediately followed by a return statement.
 * @kind problem
 * @problem.severity warning
 * @id cpp/mojo-bad-message-without-return
 */
class ReportBadMessageCall extends FunctionCall {
  ReportBadMessageCall() {
    this.getTarget().hasQualifiedName("mojo", "ReportBadMessage")
  }
}

from ReportBadMessageCall call, Function f
where
  Chromium::isChromiumCode(call) and
  CommonPatterns::isCallNotFollowedByReturn(call)
select call,
  call.getLocation().getFile().getRelativePath() + ":" +
    call.getLocation().getStartLine().toString()
