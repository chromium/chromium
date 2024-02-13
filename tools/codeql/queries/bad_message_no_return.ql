// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp
import lib.Chromium

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

from BadMessageCall call, Function f
where
  Chromium::isChromiumCode(call) and

  call.getEnclosingFunction() = f and

  // Ignore any calls with a returnStatement in the same enclosing block.
  not exists(ReturnStmt returnStmt |
    call.getEnclosingBlock() = returnStmt.getEnclosingBlock()
  )  and

  // Ignore any calls with a returnStatement immediately after in the block
  exists(Stmt stmtAfterCall |
    stmtAfterCall.getEnclosingFunction() = f and
    stmtAfterCall.getLocation().getStartLine() > call.getLocation().getStartLine() and
    not stmtAfterCall instanceof ReturnStmt and
    not exists(ReturnStmt returnBetween |
      returnBetween.getEnclosingFunction() = f and
      returnBetween.getLocation().getStartLine() > call.getLocation().getStartLine() and
      returnBetween.getLocation().getStartLine() < stmtAfterCall.getLocation().getStartLine()
    )
  )
select call,
  call.getLocation().getFile().getRelativePath() + ":" + call.getLocation().getStartLine().toString()
