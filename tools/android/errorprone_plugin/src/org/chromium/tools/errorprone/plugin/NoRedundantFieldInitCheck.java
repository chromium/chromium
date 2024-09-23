// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import com.google.errorprone.BugPattern;
import com.google.errorprone.VisitorState;
import com.google.errorprone.bugpatterns.BugChecker;
import com.google.errorprone.fixes.SuggestedFix;
import com.google.errorprone.matchers.Description;
import com.google.errorprone.matchers.Matcher;
import com.google.errorprone.matchers.Matchers;
import com.google.errorprone.util.ASTHelpers;
import com.sun.source.tree.ClassTree;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.LiteralTree;
import com.sun.source.tree.ModifiersTree;
import com.sun.source.tree.Tree;
import com.sun.source.tree.VariableTree;
import com.sun.tools.javac.code.Symbol;

import org.chromium.build.annotations.ServiceImpl;

import javax.lang.model.element.Modifier;

/** Detects when non-final fields are explicitly initialized to default values */
@ServiceImpl(BugChecker.class)
@BugPattern(
        name = "NoRedundantFieldInit",
        summary = "Do not explicitly initialize a non-final field with a default value",
        severity = BugPattern.SeverityLevel.ERROR,
        linkType = BugPattern.LinkType.CUSTOM,
        link = "https://issuetracker.google.com/issues/37124982")
public class NoRedundantFieldInitCheck extends BugChecker
        implements BugChecker.VariableTreeMatcher {
    private static final Matcher<ClassTree> SUBTYPE_OF_IINTERFACE =
            Matchers.isSubtypeOf("android.os.IInterface");

    @Override
    public Description matchVariable(VariableTree variableTree, VisitorState visitorState) {
        // Only match on fields.
        if (!Matchers.isField().matches(variableTree, visitorState)) {
            return Description.NO_MATCH;
        }

        Symbol.VarSymbol variableSymbol = ASTHelpers.getSymbol(variableTree);
        Symbol.ClassSymbol enclosingClass = ASTHelpers.enclosingClass(variableSymbol);

        // Temporarily turn off checks if the enclosing class is a subclass of IInterface.
        if (SUBTYPE_OF_IINTERFACE.matches(
                ASTHelpers.findClass(enclosingClass, visitorState), visitorState)) {
            return Description.NO_MATCH;
        }

        // Skip fields that are final.
        ModifiersTree modifiers = variableTree.getModifiers();
        if (modifiers.getFlags().contains(Modifier.FINAL)) {
            return Description.NO_MATCH;
        }

        // Skip fields in an @interface / any annotation type since these
        // are implicitly final.
        if (enclosingClass.isAnnotationType()) {
            return Description.NO_MATCH;
        }

        // Fields in interfaces are also final implicitly so skip those.
        if (enclosingClass.isInterface()) {
            return Description.NO_MATCH;
        }

        // Check if field declaration is initialized to a literal default value.
        if (isInitializedWithDefaultValue(variableTree)) {
            // Generate fix string from original source e.g.
            // public static int x = 0; --> public static int x;.
            String source = visitorState.getSourceForNode(variableTree);
            String suggestedSource = source.substring(0, source.indexOf('=')).trim() + ";";
            return describeMatch(variableTree, SuggestedFix.replace(variableTree, suggestedSource));
        }
        return Description.NO_MATCH;
    }

    private boolean isInitializedWithDefaultValue(VariableTree variableTree) {
        ExpressionTree initializer = variableTree.getInitializer();
        // Only match on literals.
        if (!(initializer instanceof LiteralTree)) {
            return false;
        }
        // Match on declarations with literal initializers that have default values.
        if (variableTree.getType().getKind() == Tree.Kind.PRIMITIVE_TYPE) {
            LiteralTree literal = (LiteralTree) initializer;
            if (literal.getKind() == Tree.Kind.BOOLEAN_LITERAL) {
                if (!(boolean) literal.getValue()) {
                    return true;
                }
            } else if (literal.getKind() == Tree.Kind.LONG_LITERAL) {
                if ((long) literal.getValue() == 0L) {
                    return true;
                }
            } else if (literal.getKind() == Tree.Kind.CHAR_LITERAL) {
                if ((char) literal.getValue() == '\u0000') {
                    return true;
                }
            } else if (literal.getKind() == Tree.Kind.DOUBLE_LITERAL) {
                if ((double) literal.getValue() == 0.0d) {
                    return true;
                }
            } else if (literal.getKind() == Tree.Kind.FLOAT_LITERAL) {
                if ((float) literal.getValue() == 0.0f) {
                    return true;
                }
            } else if ((int) literal.getValue() == 0) {
                // Int/short/byte.
                return true;
            }
        } else if (initializer.getKind() == Tree.Kind.NULL_LITERAL) {
            // Non-primitive type default value is null.
            return true;
        }
        return false;
    }
}
