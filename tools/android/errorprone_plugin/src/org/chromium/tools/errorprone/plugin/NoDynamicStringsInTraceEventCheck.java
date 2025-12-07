// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.BinaryTree;
import com.sun.source.tree.IdentifierTree;
import com.sun.source.tree.LiteralTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.source.tree.Tree;
import com.sun.source.util.SimpleTreeVisitor;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

import javax.lang.model.element.ElementKind;
import javax.lang.model.element.Modifier;

/** Triggers an error for {@link org.chromium.base.TraceEvent} usages with non string literals. */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoDynamicStringsInTraceEventCheck",
        summary = "Only use of string literals are allowed in trace events.",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://crbug.com/984827")
public class NoDynamicStringsInTraceEventCheck extends BugChecker
        implements BugChecker.MethodInvocationTreeMatcher {
    private static final Set<String> sTracingFunctions =
            new HashSet<>(
                    Arrays.asList(
                            "begin",
                            "end",
                            "scoped",
                            "startAsync",
                            "finishAsync",
                            "instant",
                            "TraceEvent"));

    private static final ParameterVisitor sVisitor = new ParameterVisitor();

    @Override
    public Description matchMethodInvocation(MethodInvocationTree tree, VisitorState visitorState) {
        Symbol.MethodSymbol method = ASTHelpers.getSymbol(tree);
        if (!sTracingFunctions.contains(method.name.toString())) return Description.NO_MATCH;

        String className = method.enclClass().fullname.toString();
        if (!"org.chromium.base.EarlyTraceEvent".equals(className)
                && !"org.chromium.base.TraceEvent".equals(className)) {
            return Description.NO_MATCH;
        }
        // Allow the events added by tracing. Adding SuppressWarning in these files causes all
        // caller warnings to be ignored.
        String filename = visitorState.getPath().getCompilationUnit().getSourceFile().getName();
        if (filename.endsWith("TraceEvent.java")) {
            return Description.NO_MATCH;
        }

        List<? extends Tree> args = tree.getArguments();
        Tree eventName_expr = args.get(0);

        ParameterVisitor.Result r = eventName_expr.accept(sVisitor, null);
        if (r.success) return Description.NO_MATCH;

        return buildDescription(tree)
                .setMessage(
                        "Calling TraceEvent.begin() without a constant String object. "
                                + r.errorMessage)
                .build();
    }

    static class ParameterVisitor extends SimpleTreeVisitor<ParameterVisitor.Result, Void> {
        static class Result {
            public boolean success;
            public String errorMessage;

            private Result(boolean successVal, String error) {
                success = successVal;
                errorMessage = error;
            }

            public static Result createError(String error) {
                return new Result(false, error);
            }

            public static Result createOk() {
                return new Result(true, null);
            }

            public Result append(Result other) {
                success &= other.success;
                if (errorMessage == null) {
                    errorMessage = other.errorMessage;
                } else if (other.errorMessage != null) {
                    errorMessage += " " + other.errorMessage;
                }
                return this;
            }
        }
        ;

        @Override
        protected Result defaultAction(Tree tree, Void p) {
            throw new RuntimeException("Unhandled expression tree type: " + tree.getKind());
        }

        @Override
        public Result visitBinary(BinaryTree tree, Void p) {
            return tree.getLeftOperand()
                    .accept(this, null)
                    .append(tree.getRightOperand().accept(this, null));
        }

        @Override
        public Result visitLiteral(LiteralTree tree, Void p) {
            return Result.createOk();
        }

        @Override
        public Result visitIdentifier(IdentifierTree node, Void p) {
            Symbol eventName = ASTHelpers.getSymbol(node);
            if (eventName == null) {
                return Result.createError("Identifier was not found: " + node + '.');
            }
            if (eventName.getKind() == ElementKind.FIELD) {
                if (!"java.lang.String".equals(eventName.type.toString())) {
                    return Result.createError("Field: " + eventName + " should be of type string.");
                }
                Set<Modifier> modifiers = eventName.getModifiers();
                if (!modifiers.contains(Modifier.FINAL) || !modifiers.contains(Modifier.STATIC)) {
                    return Result.createError(
                            "String literal: " + eventName + " is not static final.");
                }
                return Result.createOk();
            } else if (eventName.getKind() == ElementKind.PARAMETER) {
                return Result.createError(
                        "Passing in event name as parameter: " + eventName + " is not supported.");
            }
            return Result.createError("Unhandled identifier kind: " + node.getKind() + '.');
        }
    }
    ;
}
