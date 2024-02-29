// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp

module CommonPatterns {
  /**
  * Predicate to check if a function call is not followed by a return statement
  * within the same or immediately after in any block.
  */
  pragma[inline]
  predicate isCallNotFollowedByReturn(FunctionCall call) {
    exists(Function f |
      call.getEnclosingFunction() = f and

      // Ignore any calls with a returnStatement in the same enclosing block.
      not exists(ReturnStmt returnStmt |
        call.getEnclosingBlock() = returnStmt.getEnclosingBlock()
      ) and

      // Ignore any calls with a returnStatement immediately after in the block.
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
    )
  }
}
