// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import cpp
import semmle.code.cpp.dataflow.DataFlow

/**
 * @name Use of AlignUp return value in DCHECK macro
 * @description Use of the base::bits::AlignUp return value in DCHECK
 *     which can be problematic because there might not be enough space
 *     for the new aligned bytes after calling AlignUp.
 * @id cpp/alignup-to-dcheck
 */

class AlignUpCall extends FunctionCall {
  AlignUpCall() {
    // base::bits::AlignUp call.
    this.getTarget().hasQualifiedName("base::bits", "AlignUp")
  }
}

module AlignUpDataFlowConfig implements DataFlow::ConfigSig {

  predicate isSource(DataFlow::Node source) {
    source.asExpr() instanceof AlignUpCall
  }

  predicate isSink(DataFlow::Node sink) {
    exists(MacroInvocation dcheckMacro |
      dcheckMacro.getMacroName().regexpMatch("DCHECK_.*") and
      dcheckMacro.getAGeneratedElement() = sink.asExpr()
    )
  }
}

module AlignUpDataFlow = DataFlow::Global<AlignUpDataFlowConfig>;

import AlignUpDataFlow::PathGraph

from AlignUpDataFlow::PathNode source, AlignUpDataFlow::PathNode sink
where AlignUpDataFlow::flowPath(source, sink)
select "base::bits::AlignUp to DCHECK", sink,
  sink.getNode().getLocation().getFile().getRelativePath() + ":" +
    sink.getNode().getLocation().getStartLine().toString(),
  sink.getNode().asExpr().getEnclosingFunction().getDeclaringType().getTemplateArgument(0).toString()
