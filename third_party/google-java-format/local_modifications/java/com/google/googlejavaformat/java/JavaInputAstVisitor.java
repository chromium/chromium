/*
 * Copyright 2015 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 * in compliance with the License. You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License
 * is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
 * or implied. See the License for the specific language governing permissions and limitations under
 * the License.
 */

package com.google.googlejavaformat.java;

import static com.google.common.collect.Iterables.getLast;
import static com.google.common.collect.Iterables.getOnlyElement;
import static com.google.googlejavaformat.Doc.FillMode.INDEPENDENT;
import static com.google.googlejavaformat.Doc.FillMode.UNIFIED;
import static com.google.googlejavaformat.Indent.If.make;
import static com.google.googlejavaformat.OpsBuilder.BlankLineWanted.PRESERVE;
import static com.google.googlejavaformat.OpsBuilder.BlankLineWanted.YES;
import static com.google.googlejavaformat.java.Trees.getEndPosition;
import static com.google.googlejavaformat.java.Trees.getLength;
import static com.google.googlejavaformat.java.Trees.getMethodName;
import static com.google.googlejavaformat.java.Trees.getSourceForNode;
import static com.google.googlejavaformat.java.Trees.getStartPosition;
import static com.google.googlejavaformat.java.Trees.operatorName;
import static com.google.googlejavaformat.java.Trees.precedence;
import static com.google.googlejavaformat.java.Trees.skipParen;
import static com.sun.source.tree.Tree.Kind.ANNOTATION;
import static com.sun.source.tree.Tree.Kind.ARRAY_ACCESS;
import static com.sun.source.tree.Tree.Kind.ASSIGNMENT;
import static com.sun.source.tree.Tree.Kind.BLOCK;
import static com.sun.source.tree.Tree.Kind.EXTENDS_WILDCARD;
import static com.sun.source.tree.Tree.Kind.IF;
import static com.sun.source.tree.Tree.Kind.METHOD_INVOCATION;
import static com.sun.source.tree.Tree.Kind.NEW_ARRAY;
import static com.sun.source.tree.Tree.Kind.NEW_CLASS;
import static com.sun.source.tree.Tree.Kind.STRING_LITERAL;
import static com.sun.source.tree.Tree.Kind.UNION_TYPE;
import static com.sun.source.tree.Tree.Kind.VARIABLE;
import static java.util.stream.Collectors.toList;

import com.google.auto.value.AutoOneOf;
import com.google.auto.value.AutoValue;
import com.google.common.base.MoreObjects;
import com.google.common.base.Predicate;
import com.google.common.base.Throwables;
import com.google.common.base.Verify;
import com.google.common.collect.HashMultiset;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableMultimap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.ImmutableSetMultimap;
import com.google.common.collect.ImmutableSortedSet;
import com.google.common.collect.Iterables;
import com.google.common.collect.Iterators;
import com.google.common.collect.Multiset;
import com.google.common.collect.PeekingIterator;
import com.google.common.collect.Range;
import com.google.common.collect.RangeSet;
import com.google.common.collect.Streams;
import com.google.common.collect.TreeRangeSet;
import com.google.errorprone.annotations.CheckReturnValue;
import com.google.googlejavaformat.CloseOp;
import com.google.googlejavaformat.Doc;
import com.google.googlejavaformat.Doc.FillMode;
import com.google.googlejavaformat.FormattingError;
import com.google.googlejavaformat.Indent;
import com.google.googlejavaformat.Input;
import com.google.googlejavaformat.Op;
import com.google.googlejavaformat.OpenOp;
import com.google.googlejavaformat.OpsBuilder;
import com.google.googlejavaformat.OpsBuilder.BlankLineWanted;
import com.google.googlejavaformat.Output.BreakTag;
import com.google.googlejavaformat.java.DimensionHelpers.SortedDims;
import com.google.googlejavaformat.java.DimensionHelpers.TypeWithDims;
import com.sun.source.tree.AnnotatedTypeTree;
import com.sun.source.tree.AnnotationTree;
import com.sun.source.tree.ArrayAccessTree;
import com.sun.source.tree.ArrayTypeTree;
import com.sun.source.tree.AssertTree;
import com.sun.source.tree.AssignmentTree;
import com.sun.source.tree.BinaryTree;
import com.sun.source.tree.BlockTree;
import com.sun.source.tree.BreakTree;
import com.sun.source.tree.CaseTree;
import com.sun.source.tree.CatchTree;
import com.sun.source.tree.ClassTree;
import com.sun.source.tree.CompilationUnitTree;
import com.sun.source.tree.CompoundAssignmentTree;
import com.sun.source.tree.ConditionalExpressionTree;
import com.sun.source.tree.ContinueTree;
import com.sun.source.tree.DirectiveTree;
import com.sun.source.tree.DoWhileLoopTree;
import com.sun.source.tree.EmptyStatementTree;
import com.sun.source.tree.EnhancedForLoopTree;
import com.sun.source.tree.ExportsTree;
import com.sun.source.tree.ExpressionStatementTree;
import com.sun.source.tree.ExpressionTree;
import com.sun.source.tree.ForLoopTree;
import com.sun.source.tree.IdentifierTree;
import com.sun.source.tree.IfTree;
import com.sun.source.tree.ImportTree;
import com.sun.source.tree.InstanceOfTree;
import com.sun.source.tree.IntersectionTypeTree;
import com.sun.source.tree.LabeledStatementTree;
import com.sun.source.tree.LambdaExpressionTree;
import com.sun.source.tree.LiteralTree;
import com.sun.source.tree.MemberReferenceTree;
import com.sun.source.tree.MemberSelectTree;
import com.sun.source.tree.MethodInvocationTree;
import com.sun.source.tree.MethodTree;
import com.sun.source.tree.ModifiersTree;
import com.sun.source.tree.ModuleTree;
import com.sun.source.tree.NewArrayTree;
import com.sun.source.tree.NewClassTree;
import com.sun.source.tree.OpensTree;
import com.sun.source.tree.ParameterizedTypeTree;
import com.sun.source.tree.ParenthesizedTree;
import com.sun.source.tree.PrimitiveTypeTree;
import com.sun.source.tree.ProvidesTree;
import com.sun.source.tree.RequiresTree;
import com.sun.source.tree.ReturnTree;
import com.sun.source.tree.StatementTree;
import com.sun.source.tree.SwitchTree;
import com.sun.source.tree.SynchronizedTree;
import com.sun.source.tree.ThrowTree;
import com.sun.source.tree.Tree;
import com.sun.source.tree.TryTree;
import com.sun.source.tree.TypeCastTree;
import com.sun.source.tree.TypeParameterTree;
import com.sun.source.tree.UnaryTree;
import com.sun.source.tree.UnionTypeTree;
import com.sun.source.tree.UsesTree;
import com.sun.source.tree.VariableTree;
import com.sun.source.tree.WhileLoopTree;
import com.sun.source.tree.WildcardTree;
import com.sun.source.util.TreePath;
import com.sun.source.util.TreePathScanner;
import com.sun.tools.javac.code.Flags;
import com.sun.tools.javac.tree.JCTree;
import com.sun.tools.javac.tree.JCTree.JCMethodDecl;
import com.sun.tools.javac.tree.TreeScanner;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Comparator;
import java.util.Deque;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.regex.Pattern;
import java.util.stream.Stream;
import javax.lang.model.element.Name;
import org.checkerframework.checker.nullness.qual.Nullable;

/**
 * An AST visitor that builds a stream of {@link Op}s to format from the given {@link
 * CompilationUnitTree}.
 */
public class JavaInputAstVisitor extends TreePathScanner<Void, Void> {

  /** Direction for Annotations (usually VERTICAL). */
  protected enum Direction {
    VERTICAL,
    HORIZONTAL;

    boolean isVertical() {
      return this == VERTICAL;
    }
  }

  /** Whether to break or not. */
  protected enum BreakOrNot {
    YES,
    NO;

    boolean isYes() {
      return this == YES;
    }
  }

  /** Whether to collapse empty blocks. */
  protected enum CollapseEmptyOrNot {
    YES,
    NO;

    static CollapseEmptyOrNot valueOf(boolean b) {
      return b ? YES : NO;
    }

    boolean isYes() {
      return this == YES;
    }
  }

  /** Whether to allow leading blank lines in blocks. */
  protected enum AllowLeadingBlankLine {
    YES,
    NO;

    static AllowLeadingBlankLine valueOf(boolean b) {
      return b ? YES : NO;
    }
  }

  /** Whether to allow trailing blank lines in blocks. */
  protected enum AllowTrailingBlankLine {
    YES,
    NO;

    static AllowTrailingBlankLine valueOf(boolean b) {
      return b ? YES : NO;
    }
  }

  /** Whether to include braces. */
  protected enum BracesOrNot {
    YES,
    NO;

    boolean isYes() {
      return this == YES;
    }
  }

  /** Whether or not to include dimensions. */
  enum DimensionsOrNot {
    YES,
    NO;

    boolean isYes() {
      return this == YES;
    }
  }

  /** Whether or not the declaration is Varargs. */
  enum VarArgsOrNot {
    YES,
    NO;

    static VarArgsOrNot valueOf(boolean b) {
      return b ? YES : NO;
    }

    boolean isYes() {
      return this == YES;
    }

    static VarArgsOrNot fromVariable(VariableTree node) {
      return valueOf((((JCTree.JCVariableDecl) node).mods.flags & Flags.VARARGS) == Flags.VARARGS);
    }
  }

  /** Whether the formal parameter declaration is a receiver. */
  enum ReceiverParameter {
    YES,
    NO;

    boolean isYes() {
      return this == YES;
    }
  }

  /** Whether these declarations are the first in the block. */
  protected enum FirstDeclarationsOrNot {
    YES,
    NO;

    boolean isYes() {
      return this == YES;
    }
  }

  // TODO(cushon): generalize this
  private static final ImmutableMultimap<String, String> TYPE_ANNOTATIONS = typeAnnotations();

  private static ImmutableSetMultimap<String, String> typeAnnotations() {
    ImmutableSetMultimap.Builder<String, String> result = ImmutableSetMultimap.builder();
    for (String annotation :
        ImmutableList.of(
            "org.chromium.build.annotations.Nullable",
            "org.jspecify.annotations.NonNull",
            "org.jspecify.annotations.Nullable",
            "org.jspecify.nullness.Nullable",
            "org.checkerframework.checker.nullness.qual.NonNull",
            "org.checkerframework.checker.nullness.qual.Nullable")) {
      String simpleName = annotation.substring(annotation.lastIndexOf('.') + 1);
      result.put(simpleName, annotation);
    }
    return result.build();
  }

  protected final OpsBuilder builder;

  protected static final Indent.Const ZERO = Indent.Const.ZERO;
  protected final int indentMultiplier;
  protected final Indent.Const minusTwo;
  protected final Indent.Const minusFour;
  protected final Indent.Const plusTwo;
  protected final Indent.Const plusFour;

  private final Set<Name> typeAnnotationSimpleNames = new HashSet<>();

  private static final ImmutableList<Op> breakList(Optional<BreakTag> breakTag) {
    return ImmutableList.of(Doc.Break.make(Doc.FillMode.UNIFIED, " ", ZERO, breakTag));
  }

  private static final ImmutableList<Op> breakFillList(Optional<BreakTag> breakTag) {
    return ImmutableList.of(
        OpenOp.make(ZERO),
        Doc.Break.make(Doc.FillMode.INDEPENDENT, " ", ZERO, breakTag),
        CloseOp.make());
  }

  private static final ImmutableList<Op> forceBreakList(Optional<BreakTag> breakTag) {
    return ImmutableList.of(Doc.Break.make(FillMode.FORCED, "", Indent.Const.ZERO, breakTag));
  }

  /**
   * Allow multi-line filling (of array initializers, argument lists, and boolean expressions) for
   * items with length less than or equal to this threshold.
   */
  private static final int MAX_ITEM_LENGTH_FOR_FILLING = 10;

  /**
   * The {@code Visitor} constructor.
   *
   * @param builder the {@link OpsBuilder}
   */
  public JavaInputAstVisitor(OpsBuilder builder, int indentMultiplier) {
    this.builder = builder;
    this.indentMultiplier = indentMultiplier;
    minusTwo = Indent.Const.make(-2, indentMultiplier);
    minusFour = Indent.Const.make(-4, indentMultiplier);
    plusTwo = Indent.Const.make(+2, indentMultiplier);
    plusFour = Indent.Const.make(+4, indentMultiplier);
  }

  /** A record of whether we have visited into an expression. */
  private final Deque<Boolean> inExpression = new ArrayDeque<>(ImmutableList.of(false));

  private boolean inExpression() {
    return inExpression.peekLast();
  }

  @Override
  public Void scan(Tree tree, Void unused) {
    inExpression.addLast(tree instanceof ExpressionTree || inExpression.peekLast());
    int previous = builder.depth();
    try {
      super.scan(tree, null);
    } catch (FormattingError e) {
      throw e;
    } catch (Throwable t) {
      throw new FormattingError(builder.diagnostic(Throwables.getStackTraceAsString(t)));
    } finally {
      inExpression.removeLast();
    }
    builder.checkClosed(previous);
    return null;
  }

  @Override
  public Void visitCompilationUnit(CompilationUnitTree node, Void unused) {
    boolean first = true;
    if (node.getPackageName() != null) {
      markForPartialFormat();
      visitPackage(node.getPackageName(), node.getPackageAnnotations());
      builder.forcedBreak();
      first = false;
    }
    dropEmptyDeclarations();
    if (!node.getImports().isEmpty()) {
      if (!first) {
        builder.blankLineWanted(BlankLineWanted.YES);
      }
      for (ImportTree importDeclaration : node.getImports()) {
        markForPartialFormat();
        builder.blankLineWanted(PRESERVE);
        scan(importDeclaration, null);
        builder.forcedBreak();
      }
      first = false;
    }
    dropEmptyDeclarations();
    for (Tree type : node.getTypeDecls()) {
      if (type.getKind() == Tree.Kind.IMPORT) {
        // javac treats extra semicolons in the import list as type declarations
        // TODO(cushon): remove this if https://bugs.openjdk.java.net/browse/JDK-8027682 is fixed
        continue;
      }
      if (!first) {
        builder.blankLineWanted(BlankLineWanted.YES);
      }
      markForPartialFormat();
      scan(type, null);
      builder.forcedBreak();
      first = false;
      dropEmptyDeclarations();
    }
    handleModule(first, node);
    // set a partial format marker at EOF to make sure we can format the entire file
    markForPartialFormat();
    return null;
  }

  protected void handleModule(boolean first, CompilationUnitTree node) {}

  /** Skips over extra semi-colons at the top-level, or in a class member declaration lists. */
  protected void dropEmptyDeclarations() {
    if (builder.peekToken().equals(Optional.of(";"))) {
      while (builder.peekToken().equals(Optional.of(";"))) {
        builder.forcedBreak();
        markForPartialFormat();
        token(";");
      }
    }
  }

  @Override
  public Void visitClass(ClassTree tree, Void unused) {
    switch (tree.getKind()) {
      case ANNOTATION_TYPE:
        visitAnnotationType(tree);
        break;
      case CLASS:
      case INTERFACE:
        visitClassDeclaration(tree);
        break;
      case ENUM:
        visitEnumDeclaration(tree);
        break;
      default:
        throw new AssertionError(tree.getKind());
    }
    return null;
  }

  public void visitAnnotationType(ClassTree node) {
    sync(node);
    builder.open(ZERO);
    typeDeclarationModifiers(node.getModifiers());
    builder.open(ZERO);
    token("@");
    token("interface");
    builder.breakOp(" ");
    visit(node.getSimpleName());
    builder.close();
    builder.close();
    if (node.getMembers() == null) {
      builder.open(plusFour);
      token(";");
      builder.close();
    } else {
      addBodyDeclarations(node.getMembers(), BracesOrNot.YES, FirstDeclarationsOrNot.YES);
    }
    builder.guessToken(";");
  }

  @Override
  public Void visitArrayAccess(ArrayAccessTree node, Void unused) {
    sync(node);
    visitDot(node);
    return null;
  }

  @Override
  public Void visitNewArray(NewArrayTree node, Void unused) {
    if (node.getType() != null) {
      builder.open(plusFour);
      token("new");
      builder.space();

      TypeWithDims extractedDims = DimensionHelpers.extractDims(node.getType(), SortedDims.YES);
      Tree base = extractedDims.node;

      Deque<ExpressionTree> dimExpressions = new ArrayDeque<>(node.getDimensions());

      Deque<List<? extends AnnotationTree>> annotations = new ArrayDeque<>();
      annotations.add(ImmutableList.copyOf(node.getAnnotations()));
      annotations.addAll(node.getDimAnnotations());
      annotations.addAll(extractedDims.dims);

      scan(base, null);
      builder.open(ZERO);
      maybeAddDims(dimExpressions, annotations);
      builder.close();
      builder.close();
    }
    if (node.getInitializers() != null) {
      if (node.getType() != null) {
        builder.space();
      }
      visitArrayInitializer(node.getInitializers());
    }
    return null;
  }

  public boolean visitArrayInitializer(List<? extends ExpressionTree> expressions) {
    int cols;
    if (expressions.isEmpty()) {
      tokenBreakTrailingComment("{", plusTwo);
      if (builder.peekToken().equals(Optional.of(","))) {
        token(",");
      }
      token("}", plusTwo);
    } else if ((cols = argumentsAreTabular(expressions)) != -1) {
      builder.open(plusTwo);
      token("{");
      builder.forcedBreak();
      boolean first = true;
      for (Iterable<? extends ExpressionTree> row : Iterables.partition(expressions, cols)) {
        if (!first) {
          builder.forcedBreak();
        }
        builder.open(row.iterator().next().getKind() == NEW_ARRAY || cols == 1 ? ZERO : plusFour);
        boolean firstInRow = true;
        for (ExpressionTree item : row) {
          if (!firstInRow) {
            token(",");
            builder.breakToFill(" ");
          }
          scan(item, null);
          firstInRow = false;
        }
        builder.guessToken(",");
        builder.close();
        first = false;
      }
      builder.breakOp(minusTwo);
      builder.close();
      token("}", plusTwo);
    } else {
      // Special-case the formatting of array initializers inside annotations
      // to more eagerly use a one-per-line layout.
      boolean inMemberValuePair = false;
      // walk up past the enclosing NewArrayTree (and maybe an enclosing AssignmentTree)
      TreePath path = getCurrentPath();
      for (int i = 0; i < 2; i++) {
        if (path == null) {
          break;
        }
        if (path.getLeaf().getKind() == ANNOTATION) {
          inMemberValuePair = true;
          break;
        }
        path = path.getParentPath();
      }
      boolean shortItems = hasOnlyShortItems(expressions);
      boolean allowFilledElementsOnOwnLine = shortItems || !inMemberValuePair;

      builder.open(plusTwo);
      tokenBreakTrailingComment("{", plusTwo);
      boolean hasTrailingComma = hasTrailingToken(builder.getInput(), expressions, ",");
      builder.breakOp(hasTrailingComma ? FillMode.FORCED : FillMode.UNIFIED, "", ZERO);
      if (allowFilledElementsOnOwnLine) {
        builder.open(ZERO);
      }
      boolean first = true;
      FillMode fillMode = shortItems ? FillMode.INDEPENDENT : FillMode.UNIFIED;
      for (ExpressionTree expression : expressions) {
        if (!first) {
          token(",");
          builder.breakOp(fillMode, " ", ZERO);
        }
        scan(expression, null);
        first = false;
      }
      builder.guessToken(",");
      if (allowFilledElementsOnOwnLine) {
        builder.close();
      }
      builder.breakOp(minusTwo);
      builder.close();
      token("}", plusTwo);
    }
    return false;
  }

  private boolean hasOnlyShortItems(List<? extends ExpressionTree> expressions) {
    for (ExpressionTree expression : expressions) {
      int startPosition = getStartPosition(expression);
      if (builder.actualSize(
              startPosition, getEndPosition(expression, getCurrentPath()) - startPosition)
          >= MAX_ITEM_LENGTH_FOR_FILLING) {
        return false;
      }
    }
    return true;
  }

  @Override
  public Void visitArrayType(ArrayTypeTree node, Void unused) {
    sync(node);
    visitAnnotatedArrayType(node);
    return null;
  }

  private void visitAnnotatedArrayType(Tree node) {
    TypeWithDims extractedDims = DimensionHelpers.extractDims(node, SortedDims.YES);
    builder.open(plusFour);
    scan(extractedDims.node, null);
    Deque<List<? extends AnnotationTree>> dims = new ArrayDeque<>(extractedDims.dims);
    maybeAddDims(dims);
    Verify.verify(dims.isEmpty());
    builder.close();
  }

  @Override
  public Void visitAssert(AssertTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    token("assert");
    builder.space();
    builder.open(node.getDetail() == null ? ZERO : plusFour);
    scan(node.getCondition(), null);
    if (node.getDetail() != null) {
      builder.breakOp(" ");
      token(":");
      builder.space();
      scan(node.getDetail(), null);
    }
    builder.close();
    builder.close();
    token(";");
    return null;
  }

  @Override
  public Void visitAssignment(AssignmentTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    scan(node.getVariable(), null);
    builder.space();
    splitToken(operatorName(node));
    builder.breakOp(" ");
    scan(node.getExpression(), null);
    builder.close();
    return null;
  }

  @Override
  public Void visitBlock(BlockTree node, Void unused) {
    visitBlock(node, CollapseEmptyOrNot.NO, AllowLeadingBlankLine.NO, AllowTrailingBlankLine.NO);
    return null;
  }

  @Override
  public Void visitCompoundAssignment(CompoundAssignmentTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    scan(node.getVariable(), null);
    builder.space();
    splitToken(operatorName(node));
    builder.breakOp(" ");
    scan(node.getExpression(), null);
    builder.close();
    return null;
  }

  @Override
  public Void visitBreak(BreakTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    token("break");
    if (node.getLabel() != null) {
      builder.breakOp(" ");
      visit(node.getLabel());
    }
    builder.close();
    token(";");
    return null;
  }

  @Override
  public Void visitTypeCast(TypeCastTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    token("(");
    scan(node.getType(), null);
    token(")");
    builder.breakOp(" ");
    scan(node.getExpression(), null);
    builder.close();
    return null;
  }

  @Override
  public Void visitNewClass(NewClassTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    if (node.getEnclosingExpression() != null) {
      scan(node.getEnclosingExpression(), null);
      builder.breakOp();
      token(".");
    }
    token("new");
    builder.space();
    addTypeArguments(node.getTypeArguments(), plusFour);
    if (node.getClassBody() != null) {
      List<AnnotationTree> annotations =
          visitModifiers(
              node.getClassBody().getModifiers(), Direction.HORIZONTAL, Optional.empty());
      visitAnnotations(annotations, BreakOrNot.NO, BreakOrNot.YES);
    }
    scan(node.getIdentifier(), null);
    addArguments(node.getArguments(), plusFour);
    builder.close();
    if (node.getClassBody() != null) {
      addBodyDeclarations(
          node.getClassBody().getMembers(), BracesOrNot.YES, FirstDeclarationsOrNot.YES);
    }
    return null;
  }

  @Override
  public Void visitConditionalExpression(ConditionalExpressionTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    scan(node.getCondition(), null);
    builder.breakOp(" ");
    token("?");
    builder.space();
    scan(node.getTrueExpression(), null);
    builder.breakOp(" ");
    token(":");
    builder.space();
    scan(node.getFalseExpression(), null);
    builder.close();
    return null;
  }

  @Override
  public Void visitContinue(ContinueTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    token("continue");
    if (node.getLabel() != null) {
      builder.breakOp(" ");
      visit(node.getLabel());
    }
    token(";");
    builder.close();
    return null;
  }

  @Override
  public Void visitDoWhileLoop(DoWhileLoopTree node, Void unused) {
    sync(node);
    token("do");
    visitStatement(
        node.getStatement(),
        CollapseEmptyOrNot.YES,
        AllowLeadingBlankLine.YES,
        AllowTrailingBlankLine.YES);
    if (node.getStatement().getKind() == BLOCK) {
      builder.space();
    } else {
      builder.breakOp(" ");
    }
    token("while");
    builder.space();
    token("(");
    scan(skipParen(node.getCondition()), null);
    token(")");
    token(";");
    return null;
  }

  @Override
  public Void visitEmptyStatement(EmptyStatementTree node, Void unused) {
    sync(node);
    dropEmptyDeclarations();
    return null;
  }

  @Override
  public Void visitEnhancedForLoop(EnhancedForLoopTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    token("for");
    builder.space();
    token("(");
    builder.open(ZERO);
    visitToDeclare(
        DeclarationKind.NONE,
        Direction.HORIZONTAL,
        node.getVariable(),
        Optional.of(node.getExpression()),
        ":",
        /* trailing= */ Optional.empty());
    builder.close();
    token(")");
    builder.close();
    visitStatement(
        node.getStatement(),
        CollapseEmptyOrNot.YES,
        AllowLeadingBlankLine.YES,
        AllowTrailingBlankLine.NO);
    return null;
  }

  private void visitEnumConstantDeclaration(VariableTree enumConstant) {
    for (AnnotationTree annotation : enumConstant.getModifiers().getAnnotations()) {
      scan(annotation, null);
      builder.forcedBreak();
    }
    visit(enumConstant.getName());
    NewClassTree init = ((NewClassTree) enumConstant.getInitializer());
    if (init.getArguments().isEmpty()) {
      builder.guessToken("(");
      builder.guessToken(")");
    } else {
      addArguments(init.getArguments(), plusFour);
    }
    if (init.getClassBody() != null) {
      addBodyDeclarations(
          init.getClassBody().getMembers(), BracesOrNot.YES, FirstDeclarationsOrNot.YES);
    }
  }

  public boolean visitEnumDeclaration(ClassTree node) {
    sync(node);
    builder.open(ZERO);
    typeDeclarationModifiers(node.getModifiers());
    builder.open(plusFour);
    token("enum");
    builder.breakOp(" ");
    visit(node.getSimpleName());
    builder.close();
    builder.close();
    if (!node.getImplementsClause().isEmpty()) {
      builder.open(plusFour);
      builder.breakOp(" ");
      builder.open(plusFour);
      token("implements");
      builder.breakOp(" ");
      builder.open(ZERO);
      boolean first = true;
      for (Tree superInterfaceType : node.getImplementsClause()) {
        if (!first) {
          token(",");
          builder.breakToFill(" ");
        }
        scan(superInterfaceType, null);
        first = false;
      }
      builder.close();
      builder.close();
      builder.close();
    }
    builder.space();
    tokenBreakTrailingComment("{", plusTwo);
    ArrayList<VariableTree> enumConstants = new ArrayList<>();
    ArrayList<Tree> members = new ArrayList<>();
    for (Tree member : node.getMembers()) {
      if (member instanceof JCTree.JCVariableDecl) {
        JCTree.JCVariableDecl variableDecl = (JCTree.JCVariableDecl) member;
        if ((variableDecl.mods.flags & Flags.ENUM) == Flags.ENUM) {
          enumConstants.add(variableDecl);
          continue;
        }
      }
      members.add(member);
    }
    if (enumConstants.isEmpty() && members.isEmpty()) {
      if (builder.peekToken().equals(Optional.of(";"))) {
        builder.open(plusTwo);
        builder.forcedBreak();
        token(";");
        builder.forcedBreak();
        dropEmptyDeclarations();
        builder.close();
        builder.open(ZERO);
        builder.forcedBreak();
        builder.blankLineWanted(BlankLineWanted.NO);
        token("}", plusTwo);
        builder.close();
      } else {
        builder.open(ZERO);
        builder.blankLineWanted(BlankLineWanted.NO);
        token("}");
        builder.close();
      }
    } else {
      builder.open(plusTwo);
      builder.blankLineWanted(BlankLineWanted.NO);
      builder.forcedBreak();
      builder.open(ZERO);
      boolean first = true;
      for (VariableTree enumConstant : enumConstants) {
        if (!first) {
          token(",");
          builder.forcedBreak();
          builder.blankLineWanted(BlankLineWanted.PRESERVE);
        }
        markForPartialFormat();
        visitEnumConstantDeclaration(enumConstant);
        first = false;
      }
      if (builder.peekToken().orElse("").equals(",")) {
        token(",");
        builder.forcedBreak(); // The ";" goes on its own line.
      }
      builder.close();
      builder.close();
      if (builder.peekToken().equals(Optional.of(";"))) {
        builder.open(plusTwo);
        token(";");
        builder.forcedBreak();
        dropEmptyDeclarations();
        builder.close();
      }
      builder.open(ZERO);
      addBodyDeclarations(members, BracesOrNot.NO, FirstDeclarationsOrNot.NO);
      builder.forcedBreak();
      builder.blankLineWanted(BlankLineWanted.NO);
      token("}", plusTwo);
      builder.close();
    }
    builder.guessToken(";");
    return false;
  }

  @Override
  public Void visitMemberReference(MemberReferenceTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    scan(node.getQualifierExpression(), null);
    builder.breakOp();
    builder.op("::");
    addTypeArguments(node.getTypeArguments(), plusFour);
    switch (node.getMode()) {
      case INVOKE:
        visit(node.getName());
        break;
      case NEW:
        token("new");
        break;
      default:
        throw new AssertionError(node.getMode());
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitExpressionStatement(ExpressionStatementTree node, Void unused) {
    sync(node);
    scan(node.getExpression(), null);
    token(";");
    return null;
  }

  @Override
  public Void visitVariable(VariableTree node, Void unused) {
    sync(node);
    visitVariables(
        ImmutableList.of(node),
        DeclarationKind.NONE,
        fieldAnnotationDirection(node.getModifiers()));
    return null;
  }

  void visitVariables(
      List<VariableTree> fragments,
      DeclarationKind declarationKind,
      Direction annotationDirection) {
    if (fragments.size() == 1) {
      VariableTree fragment = fragments.get(0);
      declareOne(
          declarationKind,
          annotationDirection,
          Optional.of(fragment.getModifiers()),
          fragment.getType(),
          /* name= */ fragment.getName(),
          "",
          "=",
          Optional.ofNullable(fragment.getInitializer()),
          Optional.of(";"),
          /* receiverExpression= */ Optional.empty(),
          Optional.ofNullable(variableFragmentDims(true, 0, fragment.getType())));

    } else {
      declareMany(fragments, annotationDirection);
    }
  }

  private static TypeWithDims variableFragmentDims(boolean first, int leadingDims, Tree type) {
    if (type == null) {
      return null;
    }
    if (first) {
      return DimensionHelpers.extractDims(type, SortedDims.YES);
    }
    TypeWithDims dims = DimensionHelpers.extractDims(type, SortedDims.NO);
    return new TypeWithDims(
        null, leadingDims > 0 ? dims.dims.subList(0, dims.dims.size() - leadingDims) : dims.dims);
  }

  @Override
  public Void visitForLoop(ForLoopTree node, Void unused) {
    sync(node);
    token("for");
    builder.space();
    token("(");
    builder.open(plusFour);
    builder.open(
        node.getInitializer().size() > 1
                && node.getInitializer().get(0).getKind() == Tree.Kind.EXPRESSION_STATEMENT
            ? plusFour
            : ZERO);
    if (!node.getInitializer().isEmpty()) {
      if (node.getInitializer().get(0).getKind() == VARIABLE) {
        PeekingIterator<StatementTree> it =
            Iterators.peekingIterator(node.getInitializer().iterator());
        visitVariables(
            variableFragments(it, it.next()), DeclarationKind.NONE, Direction.HORIZONTAL);
      } else {
        boolean first = true;
        builder.open(ZERO);
        for (StatementTree t : node.getInitializer()) {
          if (!first) {
            token(",");
            builder.breakOp(" ");
          }
          scan(((ExpressionStatementTree) t).getExpression(), null);
          first = false;
        }
        token(";");
        builder.close();
      }
    } else {
      token(";");
    }
    builder.close();
    builder.breakOp(" ");
    if (node.getCondition() != null) {
      scan(node.getCondition(), null);
    }
    token(";");
    if (!node.getUpdate().isEmpty()) {
      builder.breakOp(" ");
      builder.open(node.getUpdate().size() <= 1 ? ZERO : plusFour);
      boolean firstUpdater = true;
      for (ExpressionStatementTree updater : node.getUpdate()) {
        if (!firstUpdater) {
          token(",");
          builder.breakToFill(" ");
        }
        scan(updater.getExpression(), null);
        firstUpdater = false;
      }
      builder.guessToken(";");
      builder.close();
    } else {
      builder.space();
    }
    builder.close();
    token(")");
    visitStatement(
        node.getStatement(),
        CollapseEmptyOrNot.YES,
        AllowLeadingBlankLine.YES,
        AllowTrailingBlankLine.NO);
    return null;
  }

  @Override
  public Void visitIf(IfTree node, Void unused) {
    sync(node);
    // Collapse chains of else-ifs.
    List<ExpressionTree> expressions = new ArrayList<>();
    List<StatementTree> statements = new ArrayList<>();
    while (true) {
      expressions.add(node.getCondition());
      statements.add(node.getThenStatement());
      if (node.getElseStatement() != null && node.getElseStatement().getKind() == IF) {
        node = (IfTree) node.getElseStatement();
      } else {
        break;
      }
    }
    builder.open(ZERO);
    boolean first = true;
    boolean followingBlock = false;
    int expressionsN = expressions.size();
    for (int i = 0; i < expressionsN; i++) {
      if (!first) {
        if (followingBlock) {
          builder.space();
        } else {
          builder.forcedBreak();
        }
        token("else");
        builder.space();
      }
      token("if");
      builder.space();
      token("(");
      scan(skipParen(expressions.get(i)), null);
      token(")");
      // An empty block can collapse to "{}" if there are no if/else or else clauses
      boolean onlyClause = expressionsN == 1 && node.getElseStatement() == null;
      // Trailing blank lines are permitted if this isn't the last clause
      boolean trailingClauses = i < expressionsN - 1 || node.getElseStatement() != null;
      visitStatement(
          statements.get(i),
          CollapseEmptyOrNot.valueOf(onlyClause),
          AllowLeadingBlankLine.YES,
          AllowTrailingBlankLine.valueOf(trailingClauses));
      followingBlock = statements.get(i).getKind() == BLOCK;
      first = false;
    }
    if (node.getElseStatement() != null) {
      if (followingBlock) {
        builder.space();
      } else {
        builder.forcedBreak();
      }
      token("else");
      visitStatement(
          node.getElseStatement(),
          CollapseEmptyOrNot.NO,
          AllowLeadingBlankLine.YES,
          AllowTrailingBlankLine.NO);
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitImport(ImportTree node, Void unused) {
    checkForTypeAnnotation(node);
    sync(node);
    token("import");
    builder.space();
    if (node.isStatic()) {
      token("static");
      builder.space();
    }
    visitName(node.getQualifiedIdentifier());
    token(";");
    // TODO(cushon): remove this if https://bugs.openjdk.java.net/browse/JDK-8027682 is fixed
    dropEmptyDeclarations();
    return null;
  }

  private void checkForTypeAnnotation(ImportTree node) {
    Name simpleName = getSimpleName(node);
    Collection<String> wellKnownAnnotations = TYPE_ANNOTATIONS.get(simpleName.toString());
    if (!wellKnownAnnotations.isEmpty()
        && wellKnownAnnotations.contains(node.getQualifiedIdentifier().toString())) {
      typeAnnotationSimpleNames.add(simpleName);
    }
  }

  private static Name getSimpleName(ImportTree importTree) {
    return importTree.getQualifiedIdentifier() instanceof IdentifierTree
        ? ((IdentifierTree) importTree.getQualifiedIdentifier()).getName()
        : ((MemberSelectTree) importTree.getQualifiedIdentifier()).getIdentifier();
  }

  @Override
  public Void visitBinary(BinaryTree node, Void unused) {
    sync(node);
    /*
     * Collect together all operators with same precedence to clean up indentation.
     */
    List<ExpressionTree> operands = new ArrayList<>();
    List<String> operators = new ArrayList<>();
    walkInfix(precedence(node), node, operands, operators);
    FillMode fillMode = hasOnlyShortItems(operands) ? INDEPENDENT : UNIFIED;
    builder.open(plusFour);
    scan(operands.get(0), null);
    int operatorsN = operators.size();
    for (int i = 0; i < operatorsN; i++) {
      builder.breakOp(fillMode, " ", ZERO);
      builder.op(operators.get(i));
      builder.space();
      scan(operands.get(i + 1), null);
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitInstanceOf(InstanceOfTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    scan(node.getExpression(), null);
    builder.breakOp(" ");
    builder.open(ZERO);
    token("instanceof");
    builder.breakOp(" ");
    scan(node.getType(), null);
    builder.close();
    builder.close();
    return null;
  }

  @Override
  public Void visitIntersectionType(IntersectionTypeTree node, Void unused) {
    sync(node);
    builder.open(plusFour);
    boolean first = true;
    for (Tree type : node.getBounds()) {
      if (!first) {
        builder.breakToFill(" ");
        token("&");
        builder.space();
      }
      scan(type, null);
      first = false;
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitLabeledStatement(LabeledStatementTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    visit(node.getLabel());
    token(":");
    builder.forcedBreak();
    builder.close();
    scan(node.getStatement(), null);
    return null;
  }

  @Override
  public Void visitLambdaExpression(LambdaExpressionTree node, Void unused) {
    sync(node);
    boolean statementBody = node.getBodyKind() == LambdaExpressionTree.BodyKind.STATEMENT;
    boolean parens = builder.peekToken().equals(Optional.of("("));
    builder.open(parens ? plusFour : ZERO);
    if (parens) {
      token("(");
    }
    boolean first = true;
    for (VariableTree parameter : node.getParameters()) {
      if (!first) {
        token(",");
        builder.breakOp(" ");
      }
      scan(parameter, null);
      first = false;
    }
    if (parens) {
      token(")");
    }
    builder.close();
    builder.space();
    builder.op("->");
    builder.open(statementBody ? ZERO : plusFour);
    if (statementBody) {
      builder.space();
    } else {
      builder.breakOp(" ");
    }
    if (node.getBody().getKind() == Tree.Kind.BLOCK) {
      visitBlock(
          (BlockTree) node.getBody(),
          CollapseEmptyOrNot.YES,
          AllowLeadingBlankLine.NO,
          AllowTrailingBlankLine.NO);
    } else {
      scan(node.getBody(), null);
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitAnnotation(AnnotationTree node, Void unused) {
    sync(node);

    if (visitSingleMemberAnnotation(node)) {
      return null;
    }

    builder.open(ZERO);
    token("@");
    scan(node.getAnnotationType(), null);
    if (!node.getArguments().isEmpty()) {
      builder.open(plusFour);
      token("(");
      builder.breakOp();
      boolean first = true;

      // Format the member value pairs one-per-line if any of them are
      // initialized with arrays.
      boolean hasArrayInitializer =
          Iterables.any(node.getArguments(), JavaInputAstVisitor::isArrayValue);
      for (ExpressionTree argument : node.getArguments()) {
        if (!first) {
          token(",");
          if (hasArrayInitializer) {
            builder.forcedBreak();
          } else {
            builder.breakOp(" ");
          }
        }
        if (argument instanceof AssignmentTree) {
          visitAnnotationArgument((AssignmentTree) argument);
        } else {
          scan(argument, null);
        }
        first = false;
      }
      token(")");
      builder.close();
      builder.close();
      return null;

    } else if (builder.peekToken().equals(Optional.of("("))) {
      token("(");
      token(")");
    }
    builder.close();
    return null;
  }

  private static boolean isArrayValue(ExpressionTree argument) {
    if (!(argument instanceof AssignmentTree)) {
      return false;
    }
    ExpressionTree expression = ((AssignmentTree) argument).getExpression();
    return expression instanceof NewArrayTree && ((NewArrayTree) expression).getType() == null;
  }

  public void visitAnnotationArgument(AssignmentTree node) {
    boolean isArrayInitializer = node.getExpression().getKind() == NEW_ARRAY;
    sync(node);
    builder.open(isArrayInitializer ? ZERO : plusFour);
    scan(node.getVariable(), null);
    builder.space();
    token("=");
    if (isArrayInitializer) {
      builder.space();
    } else {
      builder.breakOp(" ");
    }
    scan(node.getExpression(), null);
    builder.close();
  }

  @Override
  public Void visitAnnotatedType(AnnotatedTypeTree node, Void unused) {
    sync(node);
    ExpressionTree base = node.getUnderlyingType();
    if (base instanceof MemberSelectTree) {
      MemberSelectTree selectTree = (MemberSelectTree) base;
      scan(selectTree.getExpression(), null);
      token(".");
      visitAnnotations(node.getAnnotations(), BreakOrNot.NO, BreakOrNot.NO);
      builder.breakToFill(" ");
      visit(selectTree.getIdentifier());
    } else if (base instanceof ArrayTypeTree) {
      visitAnnotatedArrayType(node);
    } else {
      visitAnnotations(node.getAnnotations(), BreakOrNot.NO, BreakOrNot.NO);
      builder.breakToFill(" ");
      scan(base, null);
    }
    return null;
  }

  // TODO(cushon): Use Flags if/when we drop support for Java 11

  protected static final long COMPACT_RECORD_CONSTRUCTOR = 1L << 51;

  protected static final long RECORD = 1L << 61;

  @Override
  public Void visitMethod(MethodTree node, Void unused) {
    sync(node);
    List<? extends AnnotationTree> annotations = node.getModifiers().getAnnotations();
    List<? extends AnnotationTree> returnTypeAnnotations = ImmutableList.of();

    boolean isRecordConstructor =
        (((JCMethodDecl) node).mods.flags & COMPACT_RECORD_CONSTRUCTOR)
            == COMPACT_RECORD_CONSTRUCTOR;

    if (!node.getTypeParameters().isEmpty() && !annotations.isEmpty()) {
      int typeParameterStart = getStartPosition(node.getTypeParameters().get(0));
      for (int i = 0; i < annotations.size(); i++) {
        if (getStartPosition(annotations.get(i)) > typeParameterStart) {
          returnTypeAnnotations = annotations.subList(i, annotations.size());
          annotations = annotations.subList(0, i);
          break;
        }
      }
    }
    List<AnnotationTree> typeAnnotations =
        visitModifiers(
            node.getModifiers(),
            annotations,
            Direction.VERTICAL,
            /* declarationAnnotationBreak= */ Optional.empty());

    Tree baseReturnType = null;
    Deque<List<? extends AnnotationTree>> dims = null;
    if (node.getReturnType() != null) {
      TypeWithDims extractedDims =
          DimensionHelpers.extractDims(node.getReturnType(), SortedDims.YES);
      baseReturnType = extractedDims.node;
      dims = new ArrayDeque<>(extractedDims.dims);
    } else {
      verticalAnnotations(typeAnnotations);
      typeAnnotations = ImmutableList.of();
    }

    builder.open(plusFour);
    BreakTag breakBeforeName = genSym();
    BreakTag breakBeforeType = genSym();
    builder.open(ZERO);
    {
      boolean first = true;
      if (!typeAnnotations.isEmpty()) {
        visitAnnotations(typeAnnotations, BreakOrNot.NO, BreakOrNot.NO);
        first = false;
      }
      if (!node.getTypeParameters().isEmpty()) {
        if (!first) {
          builder.breakToFill(" ");
        }
        token("<");
        typeParametersRest(node.getTypeParameters(), plusFour);
        if (!returnTypeAnnotations.isEmpty()) {
          builder.breakToFill(" ");
          visitAnnotations(returnTypeAnnotations, BreakOrNot.NO, BreakOrNot.NO);
        }
        first = false;
      }

      boolean openedNameAndTypeScope = false;
      // constructor-like declarations that don't match the name of the enclosing class are
      // parsed as method declarations with a null return type
      if (baseReturnType != null) {
        if (!first) {
          builder.breakOp(INDEPENDENT, " ", ZERO, Optional.of(breakBeforeType));
        } else {
          first = false;
        }
        if (!openedNameAndTypeScope) {
          builder.open(make(breakBeforeType, plusFour, ZERO));
          openedNameAndTypeScope = true;
        }
        scan(baseReturnType, null);
        maybeAddDims(dims);
      }
      if (!first) {
        builder.breakOp(Doc.FillMode.INDEPENDENT, " ", ZERO, Optional.of(breakBeforeName));
      } else {
        first = false;
      }
      if (!openedNameAndTypeScope) {
        builder.open(ZERO);
        openedNameAndTypeScope = true;
      }
      String name = node.getName().toString();
      if (name.equals("<init>")) {
        name = builder.peekToken().get();
      }
      token(name);
      if (!isRecordConstructor) {
        token("(");
      }
      // end of name and type scope
      builder.close();
    }
    builder.close();

    builder.open(Indent.If.make(breakBeforeName, plusFour, ZERO));
    builder.open(Indent.If.make(breakBeforeType, plusFour, ZERO));
    builder.open(ZERO);
    {
      if (!isRecordConstructor) {
        if (!node.getParameters().isEmpty() || node.getReceiverParameter() != null) {
          // Break before args.
          builder.breakToFill("");
          visitFormals(Optional.ofNullable(node.getReceiverParameter()), node.getParameters());
        }
        token(")");
      }
      if (dims != null) {
        maybeAddDims(dims);
      }
      if (!node.getThrows().isEmpty()) {
        builder.breakToFill(" ");
        builder.open(plusFour);
        {
          visitThrowsClause(node.getThrows());
        }
        builder.close();
      }
      if (node.getDefaultValue() != null) {
        builder.space();
        token("default");
        if (node.getDefaultValue().getKind() == Tree.Kind.NEW_ARRAY) {
          builder.open(minusFour);
          {
            builder.space();
            scan(node.getDefaultValue(), null);
          }
          builder.close();
        } else {
          builder.open(ZERO);
          {
            builder.breakToFill(" ");
            scan(node.getDefaultValue(), null);
          }
          builder.close();
        }
      }
    }
    builder.close();
    builder.close();
    builder.close();
    if (node.getBody() == null) {
      token(";");
    } else {
      builder.space();
      builder.token("{", Doc.Token.RealOrImaginary.REAL, plusTwo, Optional.of(plusTwo));
    }
    builder.close();

    if (node.getBody() != null) {
      methodBody(node);
    }

    return null;
  }

  private void methodBody(MethodTree node) {
    if (node.getBody().getStatements().isEmpty()) {
      builder.blankLineWanted(BlankLineWanted.NO);
    } else {
      builder.open(plusTwo);
      builder.forcedBreak();
      builder.blankLineWanted(BlankLineWanted.PRESERVE);
      visitStatements(node.getBody().getStatements());
      builder.close();
      builder.forcedBreak();
      builder.blankLineWanted(BlankLineWanted.NO);
      markForPartialFormat();
    }
    token("}", plusTwo);
  }

  @Override
  public Void visitMethodInvocation(MethodInvocationTree node, Void unused) {
    sync(node);
    if (handleLogStatement(node)) {
      return null;
    }
    visitDot(node);
    return null;
  }

  /**
   * Special-cases log statements, to output:
   *
   * <pre>{@code
   * logger.atInfo().log(
   *     "Number of foos: %d, foos.size());
   * }</pre>
   *
   * <p>Instead of:
   *
   * <pre>{@code
   * logger
   *     .atInfo()
   *     .log(
   *         "Number of foos: %d, foos.size());
   * }</pre>
   */
  private boolean handleLogStatement(MethodInvocationTree node) {
    if (!getMethodName(node).contentEquals("log")) {
      return false;
    }
    Deque<ExpressionTree> parts = new ArrayDeque<>();
    ExpressionTree curr = node;
    while (curr instanceof MethodInvocationTree) {
      MethodInvocationTree method = (MethodInvocationTree) curr;
      parts.addFirst(method);
      if (!LOG_METHODS.contains(getMethodName(method).toString())) {
        return false;
      }
      curr = Trees.getMethodReceiver(method);
    }
    if (!(curr instanceof IdentifierTree)) {
      return false;
    }
    parts.addFirst(curr);
    visitDotWithPrefix(
        ImmutableList.copyOf(parts), false, ImmutableList.of(parts.size() - 1), INDEPENDENT);
    return true;
  }

  static final ImmutableSet<String> LOG_METHODS =
      ImmutableSet.of(
          "at",
          "atConfig",
          "atDebug",
          "atFine",
          "atFiner",
          "atFinest",
          "atInfo",
          "atMostEvery",
          "atSevere",
          "atWarning",
          "every",
          "log",
          "logVarargs",
          "perUnique",
          "withCause",
          "withStackTrace");

  private static List<Long> handleStream(List<ExpressionTree> parts) {
    return indexes(
            parts.stream(),
            p -> {
              if (!(p instanceof MethodInvocationTree)) {
                return false;
              }
              Name name = getMethodName((MethodInvocationTree) p);
              return Stream.of("stream", "parallelStream", "toBuilder")
                  .anyMatch(name::contentEquals);
            })
        .collect(toList());
  }

  private static <T> Stream<Long> indexes(Stream<T> stream, Predicate<T> predicate) {
    return Streams.mapWithIndex(stream, (x, i) -> predicate.apply(x) ? i : -1).filter(x -> x != -1);
  }

  @Override
  public Void visitMemberSelect(MemberSelectTree node, Void unused) {
    sync(node);
    visitDot(node);
    return null;
  }

  @Override
  public Void visitLiteral(LiteralTree node, Void unused) {
    sync(node);
    String sourceForNode = getSourceForNode(node, getCurrentPath());
    if (isUnaryMinusLiteral(sourceForNode)) {
      token("-");
      sourceForNode = sourceForNode.substring(1).trim();
    }
    token(sourceForNode);
    return null;
  }

  // A negative numeric literal -n is usually represented as unary minus on n,
  // but that doesn't work for integer or long MIN_VALUE. The parser works
  // around that by representing it directly as a signed literal (with no
  // unary minus), but the lexer still expects two tokens.
  private static boolean isUnaryMinusLiteral(String literalTreeSource) {
    return literalTreeSource.startsWith("-");
  }

  private void visitPackage(
      ExpressionTree packageName, List<? extends AnnotationTree> packageAnnotations) {
    if (!packageAnnotations.isEmpty()) {
      for (AnnotationTree annotation : packageAnnotations) {
        builder.forcedBreak();
        scan(annotation, null);
      }
      builder.forcedBreak();
    }
    builder.open(plusFour);
    token("package");
    builder.space();
    visitName(packageName);
    builder.close();
    token(";");
  }

  @Override
  public Void visitParameterizedType(ParameterizedTypeTree node, Void unused) {
    sync(node);
    if (node.getTypeArguments().isEmpty()) {
      scan(node.getType(), null);
      token("<");
      token(">");
    } else {
      builder.open(plusFour);
      scan(node.getType(), null);
      token("<");
      builder.breakOp();
      builder.open(ZERO);
      boolean first = true;
      for (Tree typeArgument : node.getTypeArguments()) {
        if (!first) {
          token(",");
          builder.breakOp(" ");
        }
        scan(typeArgument, null);
        first = false;
      }
      builder.close();
      builder.close();
      token(">");
    }
    return null;
  }

  @Override
  public Void visitParenthesized(ParenthesizedTree node, Void unused) {
    token("(");
    scan(node.getExpression(), null);
    token(")");
    return null;
  }

  @Override
  public Void visitUnary(UnaryTree node, Void unused) {
    sync(node);
    String operatorName = operatorName(node);
    if (((JCTree) node).getTag().isPostUnaryOp()) {
      scan(node.getExpression(), null);
      splitToken(operatorName);
    } else {
      splitToken(operatorName);
      if (ambiguousUnaryOperator(node, operatorName)) {
        builder.space();
      }
      scan(node.getExpression(), null);
    }
    return null;
  }

  private void splitToken(String operatorName) {
    for (int i = 0; i < operatorName.length(); i++) {
      token(String.valueOf(operatorName.charAt(i)));
    }
  }

  private boolean ambiguousUnaryOperator(UnaryTree node, String operatorName) {
    switch (node.getKind()) {
      case UNARY_MINUS:
      case UNARY_PLUS:
        break;
      default:
        return false;
    }
    JCTree.Tag tag = unaryTag(node.getExpression());
    if (tag == null) {
      return false;
    }
    if (tag.isPostUnaryOp()) {
      return false;
    }
    if (!operatorName(node).startsWith(operatorName)) {
      return false;
    }
    return true;
  }

  private JCTree.Tag unaryTag(ExpressionTree expression) {
    if (expression instanceof UnaryTree) {
      return ((JCTree) expression).getTag();
    }
    if (expression instanceof LiteralTree
        && isUnaryMinusLiteral(getSourceForNode(expression, getCurrentPath()))) {
      return JCTree.Tag.MINUS;
    }
    return null;
  }

  @Override
  public Void visitPrimitiveType(PrimitiveTypeTree node, Void unused) {
    sync(node);
    switch (node.getPrimitiveTypeKind()) {
      case BOOLEAN:
        token("boolean");
        break;
      case BYTE:
        token("byte");
        break;
      case SHORT:
        token("short");
        break;
      case INT:
        token("int");
        break;
      case LONG:
        token("long");
        break;
      case CHAR:
        token("char");
        break;
      case FLOAT:
        token("float");
        break;
      case DOUBLE:
        token("double");
        break;
      case VOID:
        token("void");
        break;
      default:
        throw new AssertionError(node.getPrimitiveTypeKind());
    }
    return null;
  }

  public boolean visit(Name name) {
    token(name.toString());
    return false;
  }

  @Override
  public Void visitReturn(ReturnTree node, Void unused) {
    sync(node);
    token("return");
    if (node.getExpression() != null) {
      builder.space();
      scan(node.getExpression(), null);
    }
    token(";");
    return null;
  }

  // TODO(cushon): is this worth special-casing?
  boolean visitSingleMemberAnnotation(AnnotationTree node) {
    if (node.getArguments().size() != 1) {
      return false;
    }
    ExpressionTree value = getOnlyElement(node.getArguments());
    if (value.getKind() == ASSIGNMENT) {
      return false;
    }
    boolean isArrayInitializer = value.getKind() == NEW_ARRAY;
    builder.open(isArrayInitializer ? ZERO : plusFour);
    token("@");
    scan(node.getAnnotationType(), null);
    token("(");
    if (!isArrayInitializer) {
      builder.breakOp();
    }
    scan(value, null);
    builder.close();
    token(")");
    return true;
  }

  @Override
  public Void visitCase(CaseTree node, Void unused) {
    sync(node);
    markForPartialFormat();
    builder.forcedBreak();
    if (node.getExpression() == null) {
      token("default", plusTwo);
      token(":");
    } else {
      token("case", plusTwo);
      builder.space();
      scan(node.getExpression(), null);
      token(":");
    }
    builder.open(plusTwo);
    visitStatements(node.getStatements());
    builder.close();
    return null;
  }

  @Override
  public Void visitSwitch(SwitchTree node, Void unused) {
    sync(node);
    visitSwitch(node.getExpression(), node.getCases());
    return null;
  }

  protected void visitSwitch(ExpressionTree expression, List<? extends CaseTree> cases) {
    token("switch");
    builder.space();
    token("(");
    scan(skipParen(expression), null);
    token(")");
    builder.space();
    tokenBreakTrailingComment("{", plusTwo);
    builder.blankLineWanted(BlankLineWanted.NO);
    builder.open(plusTwo);
    boolean first = true;
    for (CaseTree caseTree : cases) {
      if (!first) {
        builder.blankLineWanted(BlankLineWanted.PRESERVE);
      }
      scan(caseTree, null);
      first = false;
    }
    builder.close();
    builder.forcedBreak();
    builder.blankLineWanted(BlankLineWanted.NO);
    token("}", plusFour);
  }

  @Override
  public Void visitSynchronized(SynchronizedTree node, Void unused) {
    sync(node);
    token("synchronized");
    builder.space();
    token("(");
    builder.open(plusFour);
    builder.breakOp();
    scan(skipParen(node.getExpression()), null);
    builder.close();
    token(")");
    builder.space();
    scan(node.getBlock(), null);
    return null;
  }

  @Override
  public Void visitThrow(ThrowTree node, Void unused) {
    sync(node);
    token("throw");
    builder.space();
    scan(node.getExpression(), null);
    token(";");
    return null;
  }

  @Override
  public Void visitTry(TryTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    token("try");
    builder.space();
    if (!node.getResources().isEmpty()) {
      token("(");
      builder.open(node.getResources().size() > 1 ? plusFour : ZERO);
      boolean first = true;
      for (Tree resource : node.getResources()) {
        if (!first) {
          builder.forcedBreak();
        }
        if (resource instanceof VariableTree) {
          VariableTree variableTree = (VariableTree) resource;
          declareOne(
              DeclarationKind.PARAMETER,
              fieldAnnotationDirection(variableTree.getModifiers()),
              Optional.of(variableTree.getModifiers()),
              variableTree.getType(),
              /* name= */ variableTree.getName(),
              "",
              "=",
              Optional.ofNullable(variableTree.getInitializer()),
              /* trailing= */ Optional.empty(),
              /* receiverExpression= */ Optional.empty(),
              /* typeWithDims= */ Optional.empty());
        } else {
          // TODO(cushon): think harder about what to do with `try (resource1; resource2) {}`
          scan(resource, null);
        }
        if (builder.peekToken().equals(Optional.of(";"))) {
          token(";");
          builder.space();
        }
        first = false;
      }
      if (builder.peekToken().equals(Optional.of(";"))) {
        token(";");
        builder.space();
      }
      token(")");
      builder.close();
      builder.space();
    }
    // An empty try-with-resources body can collapse to "{}" if there are no trailing catch or
    // finally blocks.
    boolean trailingClauses = !node.getCatches().isEmpty() || node.getFinallyBlock() != null;
    visitBlock(
        node.getBlock(),
        CollapseEmptyOrNot.valueOf(!trailingClauses),
        AllowLeadingBlankLine.YES,
        AllowTrailingBlankLine.valueOf(trailingClauses));
    for (int i = 0; i < node.getCatches().size(); i++) {
      CatchTree catchClause = node.getCatches().get(i);
      trailingClauses = i < node.getCatches().size() - 1 || node.getFinallyBlock() != null;
      visitCatchClause(catchClause, AllowTrailingBlankLine.valueOf(trailingClauses));
    }
    if (node.getFinallyBlock() != null) {
      builder.space();
      token("finally");
      builder.space();
      visitBlock(
          node.getFinallyBlock(),
          CollapseEmptyOrNot.NO,
          AllowLeadingBlankLine.YES,
          AllowTrailingBlankLine.NO);
    }
    builder.close();
    return null;
  }

  public void visitClassDeclaration(ClassTree node) {
    sync(node);
    typeDeclarationModifiers(node.getModifiers());
    List<? extends Tree> permitsTypes = getPermitsClause(node);
    boolean hasSuperclassType = node.getExtendsClause() != null;
    boolean hasSuperInterfaceTypes = !node.getImplementsClause().isEmpty();
    boolean hasPermitsTypes = !permitsTypes.isEmpty();
    token(node.getKind() == Tree.Kind.INTERFACE ? "interface" : "class");
    builder.space();
    visit(node.getSimpleName());
    if (!node.getTypeParameters().isEmpty()) {
      token("<");
    }
    builder.open(plusFour);
    {
      if (!node.getTypeParameters().isEmpty()) {
        typeParametersRest(
            node.getTypeParameters(),
            hasSuperclassType || hasSuperInterfaceTypes || hasPermitsTypes ? plusFour : ZERO);
      }
      if (hasSuperclassType) {
        builder.breakToFill(" ");
        token("extends");
        builder.space();
        scan(node.getExtendsClause(), null);
      }
      classDeclarationTypeList(
          node.getKind() == Tree.Kind.INTERFACE ? "extends" : "implements",
          node.getImplementsClause());
      classDeclarationTypeList("permits", permitsTypes);
    }
    builder.close();
    if (node.getMembers() == null) {
      token(";");
    } else {
      addBodyDeclarations(node.getMembers(), BracesOrNot.YES, FirstDeclarationsOrNot.YES);
    }
    dropEmptyDeclarations();
  }

  @Override
  public Void visitTypeParameter(TypeParameterTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    visitAnnotations(node.getAnnotations(), BreakOrNot.NO, BreakOrNot.YES);
    visit(node.getName());
    if (!node.getBounds().isEmpty()) {
      builder.space();
      token("extends");
      builder.open(plusFour);
      builder.breakOp(" ");
      builder.open(plusFour);
      boolean first = true;
      for (Tree typeBound : node.getBounds()) {
        if (!first) {
          builder.breakToFill(" ");
          token("&");
          builder.space();
        }
        scan(typeBound, null);
        first = false;
      }
      builder.close();
      builder.close();
    }
    builder.close();
    return null;
  }

  @Override
  public Void visitUnionType(UnionTypeTree node, Void unused) {
    throw new IllegalStateException("expected manual descent into union types");
  }

  @Override
  public Void visitWhileLoop(WhileLoopTree node, Void unused) {
    sync(node);
    token("while");
    builder.space();
    token("(");
    scan(skipParen(node.getCondition()), null);
    token(")");
    visitStatement(
        node.getStatement(),
        CollapseEmptyOrNot.YES,
        AllowLeadingBlankLine.YES,
        AllowTrailingBlankLine.NO);
    return null;
  }

  @Override
  public Void visitWildcard(WildcardTree node, Void unused) {
    sync(node);
    builder.open(ZERO);
    token("?");
    if (node.getBound() != null) {
      builder.open(plusFour);
      builder.space();
      token(node.getKind() == EXTENDS_WILDCARD ? "extends" : "super");
      builder.breakOp(" ");
      scan(node.getBound(), null);
      builder.close();
    }
    builder.close();
    return null;
  }

  // Helper methods.

  /** Helper method for annotations. */
  protected void visitAnnotations(
      List<? extends AnnotationTree> annotations, BreakOrNot breakBefore, BreakOrNot breakAfter) {
    if (!annotations.isEmpty()) {
      if (breakBefore.isYes()) {
        builder.breakToFill(" ");
      }
      boolean first = true;
      for (AnnotationTree annotation : annotations) {
        if (!first) {
          builder.breakToFill(" ");
        }
        scan(annotation, null);
        first = false;
      }
      if (breakAfter.isYes()) {
        builder.breakToFill(" ");
      }
    }
  }

  void verticalAnnotations(List<AnnotationTree> annotations) {
    for (AnnotationTree annotation : annotations) {
      builder.forcedBreak();
      scan(annotation, null);
      builder.forcedBreak();
    }
  }

  /** Helper method for blocks. */
  protected void visitBlock(
      BlockTree node,
      CollapseEmptyOrNot collapseEmptyOrNot,
      AllowLeadingBlankLine allowLeadingBlankLine,
      AllowTrailingBlankLine allowTrailingBlankLine) {
    sync(node);
    if (node.isStatic()) {
      token("static");
      builder.space();
    }
    if (collapseEmptyOrNot.isYes() && node.getStatements().isEmpty()) {
      if (builder.peekToken().equals(Optional.of(";"))) {
        // TODO(cushon): is this needed?
        token(";");
      } else {
        tokenBreakTrailingComment("{", plusTwo);
        builder.blankLineWanted(BlankLineWanted.NO);
        token("}", plusTwo);
      }
    } else {
      builder.open(ZERO);
      builder.open(plusTwo);
      tokenBreakTrailingComment("{", plusTwo);
      if (allowLeadingBlankLine == AllowLeadingBlankLine.NO) {
        builder.blankLineWanted(BlankLineWanted.NO);
      } else {
        builder.blankLineWanted(BlankLineWanted.PRESERVE);
      }
      visitStatements(node.getStatements());
      builder.close();
      builder.forcedBreak();
      builder.close();
      if (allowTrailingBlankLine == AllowTrailingBlankLine.NO) {
        builder.blankLineWanted(BlankLineWanted.NO);
      } else {
        builder.blankLineWanted(BlankLineWanted.PRESERVE);
      }
      markForPartialFormat();
      token("}", plusTwo);
    }
  }

  /** Helper method for statements. */
  private void visitStatement(
      StatementTree node,
      CollapseEmptyOrNot collapseEmptyOrNot,
      AllowLeadingBlankLine allowLeadingBlank,
      AllowTrailingBlankLine allowTrailingBlank) {
    sync(node);
    switch (node.getKind()) {
      case BLOCK:
        builder.space();
        visitBlock((BlockTree) node, collapseEmptyOrNot, allowLeadingBlank, allowTrailingBlank);
        break;
      default:
        builder.open(plusTwo);
        builder.breakOp(" ");
        scan(node, null);
        builder.close();
    }
  }

  protected void visitStatements(List<? extends StatementTree> statements) {
    boolean first = true;
    PeekingIterator<StatementTree> it = Iterators.peekingIterator(statements.iterator());
    dropEmptyDeclarations();
    while (it.hasNext()) {
      StatementTree tree = it.next();
      builder.forcedBreak();
      if (!first) {
        builder.blankLineWanted(BlankLineWanted.PRESERVE);
      }
      markForPartialFormat();
      first = false;
      List<VariableTree> fragments = variableFragments(it, tree);
      if (!fragments.isEmpty()) {
        visitVariables(
            fragments,
            DeclarationKind.NONE,
            canLocalHaveHorizontalAnnotations(fragments.get(0).getModifiers()));
      } else {
        scan(tree, null);
      }
    }
  }

  protected void typeDeclarationModifiers(ModifiersTree modifiers) {
    List<AnnotationTree> typeAnnotations =
        visitModifiers(
            modifiers, Direction.VERTICAL, /* declarationAnnotationBreak= */ Optional.empty());
    verticalAnnotations(typeAnnotations);
  }

  /** Output combined modifiers and annotations and the trailing break. */
  void visitAndBreakModifiers(
      ModifiersTree modifiers,
      Direction annotationDirection,
      Optional<BreakTag> declarationAnnotationBreak) {
    List<AnnotationTree> typeAnnotations =
        visitModifiers(modifiers, annotationDirection, declarationAnnotationBreak);
    visitAnnotations(typeAnnotations, BreakOrNot.NO, BreakOrNot.YES);
  }

  @Override
  public Void visitModifiers(ModifiersTree node, Void unused) {
    throw new IllegalStateException("expected manual descent into modifiers");
  }

  /** Output combined modifiers and annotations and returns the trailing break. */
  @CheckReturnValue
  protected ImmutableList<AnnotationTree> visitModifiers(
      ModifiersTree modifiersTree,
      Direction annotationsDirection,
      Optional<BreakTag> declarationAnnotationBreak) {
    return visitModifiers(
        modifiersTree,
        modifiersTree.getAnnotations(),
        annotationsDirection,
        declarationAnnotationBreak);
  }

  @CheckReturnValue
  protected ImmutableList<AnnotationTree> visitModifiers(
      ModifiersTree modifiersTree,
      List<? extends AnnotationTree> annotationTrees,
      Direction annotationsDirection,
      Optional<BreakTag> declarationAnnotationBreak) {
    DeclarationModifiersAndTypeAnnotations splitModifiers =
        splitModifiers(modifiersTree, annotationTrees);
    return visitModifiers(splitModifiers, annotationsDirection, declarationAnnotationBreak);
  }

  @CheckReturnValue
  private ImmutableList<AnnotationTree> visitModifiers(
      DeclarationModifiersAndTypeAnnotations splitModifiers,
      Direction annotationsDirection,
      Optional<BreakTag> declarationAnnotationBreak) {
    if (splitModifiers.declarationModifiers().isEmpty()) {
      return splitModifiers.typeAnnotations();
    }
    Deque<AnnotationOrModifier> declarationModifiers =
        new ArrayDeque<>(splitModifiers.declarationModifiers());
    builder.open(ZERO);
    boolean first = true;
    boolean lastWasAnnotation = false;
    while (!declarationModifiers.isEmpty() && !declarationModifiers.peekFirst().isModifier()) {
      if (!first) {
        builder.addAll(
            annotationsDirection.isVertical()
                ? forceBreakList(declarationAnnotationBreak)
                : breakList(declarationAnnotationBreak));
      }
      formatAnnotationOrModifier(declarationModifiers);
      first = false;
      lastWasAnnotation = true;
    }
    builder.close();
    ImmutableList<Op> trailingBreak =
        annotationsDirection.isVertical()
            ? forceBreakList(declarationAnnotationBreak)
            : breakList(declarationAnnotationBreak);
    if (declarationModifiers.isEmpty()) {
      builder.addAll(trailingBreak);
      return splitModifiers.typeAnnotations();
    }
    if (lastWasAnnotation) {
      builder.addAll(trailingBreak);
    }

    builder.open(ZERO);
    first = true;
    while (!declarationModifiers.isEmpty()) {
      if (!first) {
        builder.addAll(breakFillList(Optional.empty()));
      }
      formatAnnotationOrModifier(declarationModifiers);
      first = false;
    }
    builder.close();
    builder.addAll(breakFillList(Optional.empty()));
    return splitModifiers.typeAnnotations();
  }

  /** Represents an annotation or a modifier in a {@link ModifiersTree}. */
  @AutoOneOf(AnnotationOrModifier.Kind.class)
  abstract static class AnnotationOrModifier implements Comparable<AnnotationOrModifier> {
    enum Kind {
      MODIFIER,
      ANNOTATION
    }

    abstract Kind getKind();

    abstract AnnotationTree annotation();

    abstract Input.Tok modifier();

    static AnnotationOrModifier ofModifier(Input.Tok m) {
      return AutoOneOf_JavaInputAstVisitor_AnnotationOrModifier.modifier(m);
    }

    static AnnotationOrModifier ofAnnotation(AnnotationTree a) {
      return AutoOneOf_JavaInputAstVisitor_AnnotationOrModifier.annotation(a);
    }

    boolean isModifier() {
      return getKind().equals(Kind.MODIFIER);
    }

    boolean isAnnotation() {
      return getKind().equals(Kind.ANNOTATION);
    }

    int position() {
      switch (getKind()) {
        case MODIFIER:
          return modifier().getPosition();
        case ANNOTATION:
          return getStartPosition(annotation());
      }
      throw new AssertionError();
    }

    private static final Comparator<AnnotationOrModifier> COMPARATOR =
        Comparator.comparingInt(AnnotationOrModifier::position);

    @Override
    public int compareTo(AnnotationOrModifier o) {
      return COMPARATOR.compare(this, o);
    }
  }

  /**
   * The modifiers annotations for a declaration, grouped in to a prefix that contains all of the
   * declaration annotations and modifiers, and a suffix of type annotations.
   *
   * <p>For examples like {@code @Deprecated public @Nullable Foo foo();}, this allows us to format
   * {@code @Deprecated public} as declaration modifiers, and {@code @Nullable} as a type annotation
   * on the return type.
   */
  @AutoValue
  abstract static class DeclarationModifiersAndTypeAnnotations {
    abstract ImmutableList<AnnotationOrModifier> declarationModifiers();

    abstract ImmutableList<AnnotationTree> typeAnnotations();

    static DeclarationModifiersAndTypeAnnotations create(
        ImmutableList<AnnotationOrModifier> declarationModifiers,
        ImmutableList<AnnotationTree> typeAnnotations) {
      return new AutoValue_JavaInputAstVisitor_DeclarationModifiersAndTypeAnnotations(
          declarationModifiers, typeAnnotations);
    }

    static DeclarationModifiersAndTypeAnnotations empty() {
      return create(ImmutableList.of(), ImmutableList.of());
    }

    boolean hasDeclarationAnnotation() {
      return declarationModifiers().stream().anyMatch(AnnotationOrModifier::isAnnotation);
    }
  }

  /**
   * Examines the token stream to convert the modifiers for a declaration into a {@link
   * DeclarationModifiersAndTypeAnnotations}.
   */
  DeclarationModifiersAndTypeAnnotations splitModifiers(
      ModifiersTree modifiersTree, List<? extends AnnotationTree> annotations) {
    if (annotations.isEmpty() && !isModifier(builder.peekToken().get())) {
      return DeclarationModifiersAndTypeAnnotations.empty();
    }
    RangeSet<Integer> annotationRanges = TreeRangeSet.create();
    for (AnnotationTree annotationTree : annotations) {
      annotationRanges.add(
          Range.closedOpen(
              getStartPosition(annotationTree), getEndPosition(annotationTree, getCurrentPath())));
    }
    ImmutableList<Input.Tok> toks =
        builder.peekTokens(
            getStartPosition(modifiersTree),
            (Input.Tok tok) ->
                // ModifiersTree end position information isn't reliable, so scan tokens as long as
                // we're seeing annotations or modifiers
                annotationRanges.contains(tok.getPosition()) || isModifier(tok.getText()));
    ImmutableList<AnnotationOrModifier> modifiers =
        ImmutableList.copyOf(
            Streams.concat(
                    toks.stream()
                        // reject tokens from inside AnnotationTrees, we only want modifiers
                        .filter(t -> !annotationRanges.contains(t.getPosition()))
                        .map(AnnotationOrModifier::ofModifier),
                    annotations.stream().map(AnnotationOrModifier::ofAnnotation))
                .sorted()
                .collect(toList()));
    // Take a suffix of annotations that are well-known type annotations, and which appear after any
    // declaration annotations or modifiers
    ImmutableList.Builder<AnnotationTree> typeAnnotations = ImmutableList.builder();
    int idx = modifiers.size() - 1;
    while (idx >= 0) {
      AnnotationOrModifier modifier = modifiers.get(idx);
      if (!modifier.isAnnotation() || !isTypeAnnotation(modifier.annotation())) {
        break;
      }
      typeAnnotations.add(modifier.annotation());
      idx--;
    }
    return DeclarationModifiersAndTypeAnnotations.create(
        modifiers.subList(0, idx + 1), typeAnnotations.build().reverse());
  }

  private void formatAnnotationOrModifier(Deque<AnnotationOrModifier> modifiers) {
    AnnotationOrModifier modifier = modifiers.removeFirst();
    switch (modifier.getKind()) {
      case MODIFIER:
        token(modifier.modifier().getText());
        if (modifier.modifier().getText().equals("non")) {
          token(modifiers.removeFirst().modifier().getText());
          token(modifiers.removeFirst().modifier().getText());
        }
        break;
      case ANNOTATION:
        scan(modifier.annotation(), null);
        break;
    }
  }

  boolean isTypeAnnotation(AnnotationTree annotationTree) {
    Tree annotationType = annotationTree.getAnnotationType();
    if (!(annotationType instanceof IdentifierTree)) {
      return false;
    }
    return typeAnnotationSimpleNames.contains(((IdentifierTree) annotationType).getName());
  }

  private static boolean isModifier(String token) {
    switch (token) {
      case "public":
      case "protected":
      case "private":
      case "abstract":
      case "static":
      case "final":
      case "transient":
      case "volatile":
      case "synchronized":
      case "native":
      case "strictfp":
      case "default":
      case "sealed":
      case "non":
      case "-":
        return true;
      default:
        return false;
    }
  }

  @Override
  public Void visitCatch(CatchTree node, Void unused) {
    throw new IllegalStateException("expected manual descent into catch trees");
  }

  /** Helper method for {@link CatchTree}s. */
  private void visitCatchClause(CatchTree node, AllowTrailingBlankLine allowTrailingBlankLine) {
    sync(node);
    builder.space();
    token("catch");
    builder.space();
    token("(");
    builder.open(plusFour);
    VariableTree ex = node.getParameter();
    if (ex.getType().getKind() == UNION_TYPE) {
      builder.open(ZERO);
      visitUnionType(ex);
      builder.close();
    } else {
      // TODO(cushon): don't break after here for consistency with for, while, etc.
      builder.breakToFill();
      builder.open(ZERO);
      scan(ex, null);
      builder.close();
    }
    builder.close();
    token(")");
    builder.space();
    visitBlock(
        node.getBlock(), CollapseEmptyOrNot.NO, AllowLeadingBlankLine.YES, allowTrailingBlankLine);
  }

  /** Formats a union type declaration in a catch clause. */
  private void visitUnionType(VariableTree declaration) {
    UnionTypeTree type = (UnionTypeTree) declaration.getType();
    builder.open(ZERO);
    sync(declaration);
    visitAndBreakModifiers(
        declaration.getModifiers(),
        Direction.HORIZONTAL,
        /* declarationAnnotationBreak= */ Optional.empty());
    List<? extends Tree> union = type.getTypeAlternatives();
    boolean first = true;
    for (int i = 0; i < union.size() - 1; i++) {
      if (!first) {
        builder.breakOp(" ");
        token("|");
        builder.space();
      } else {
        first = false;
      }
      scan(union.get(i), null);
    }
    builder.breakOp(" ");
    token("|");
    builder.space();
    Tree last = union.get(union.size() - 1);
    declareOne(
        DeclarationKind.NONE,
        Direction.HORIZONTAL,
        /* modifiers= */ Optional.empty(),
        last,
        /* name= */ declaration.getName(),
        /* op= */ "",
        "=",
        Optional.ofNullable(declaration.getInitializer()),
        /* trailing= */ Optional.empty(),
        /* receiverExpression= */ Optional.empty(),
        /* typeWithDims= */ Optional.empty());
    builder.close();
  }

  /** Accumulate the operands and operators. */
  private static void walkInfix(
      int precedence,
      ExpressionTree expression,
      List<ExpressionTree> operands,
      List<String> operators) {
    if (expression instanceof BinaryTree) {
      BinaryTree binaryTree = (BinaryTree) expression;
      if (precedence(binaryTree) == precedence) {
        walkInfix(precedence, binaryTree.getLeftOperand(), operands, operators);
        operators.add(operatorName(expression));
        walkInfix(precedence, binaryTree.getRightOperand(), operands, operators);
      } else {
        operands.add(expression);
      }
    } else {
      operands.add(expression);
    }
  }

  protected void visitFormals(
      Optional<VariableTree> receiver, List<? extends VariableTree> parameters) {
    if (!receiver.isPresent() && parameters.isEmpty()) {
      return;
    }
    builder.open(ZERO);
    boolean first = true;
    if (receiver.isPresent()) {
      // TODO(user): Use builders.
      declareOne(
          DeclarationKind.PARAMETER,
          Direction.HORIZONTAL,
          Optional.of(receiver.get().getModifiers()),
          receiver.get().getType(),
          /* name= */ receiver.get().getName(),
          "",
          "",
          /* initializer= */ Optional.empty(),
          !parameters.isEmpty() ? Optional.of(",") : Optional.empty(),
          Optional.of(receiver.get().getNameExpression()),
          /* typeWithDims= */ Optional.empty());
      first = false;
    }
    for (int i = 0; i < parameters.size(); i++) {
      VariableTree parameter = parameters.get(i);
      if (!first) {
        builder.breakOp(" ");
      }
      visitToDeclare(
          DeclarationKind.PARAMETER,
          Direction.HORIZONTAL,
          parameter,
          /* initializer= */ Optional.empty(),
          "=",
          i < parameters.size() - 1 ? Optional.of(",") : /* a= */ Optional.empty());
      first = false;
    }
    builder.close();
  }

  //  /** Helper method for {@link MethodDeclaration}s. */
  private void visitThrowsClause(List<? extends ExpressionTree> thrownExceptionTypes) {
    token("throws");
    builder.breakToFill(" ");
    boolean first = true;
    for (ExpressionTree thrownExceptionType : thrownExceptionTypes) {
      if (!first) {
        token(",");
        builder.breakOp(" ");
      }
      scan(thrownExceptionType, null);
      first = false;
    }
  }

  @Override
  public Void visitIdentifier(IdentifierTree node, Void unused) {
    sync(node);
    token(node.getName().toString());
    return null;
  }

  @Override
  public Void visitModule(ModuleTree node, Void unused) {
    for (AnnotationTree annotation : node.getAnnotations()) {
      scan(annotation, null);
      builder.forcedBreak();
    }
    if (node.getModuleType() == ModuleTree.ModuleKind.OPEN) {
      token("open");
      builder.space();
    }
    token("module");
    builder.space();
    scan(node.getName(), null);
    builder.space();
    if (node.getDirectives().isEmpty()) {
      tokenBreakTrailingComment("{", plusTwo);
      builder.blankLineWanted(BlankLineWanted.NO);
      token("}", plusTwo);
    } else {
      builder.open(plusTwo);
      token("{");
      builder.forcedBreak();
      Optional<Tree.Kind> previousDirective = Optional.empty();
      for (DirectiveTree directiveTree : node.getDirectives()) {
        markForPartialFormat();
        builder.blankLineWanted(
            previousDirective.map(k -> !k.equals(directiveTree.getKind())).orElse(false)
                ? BlankLineWanted.YES
                : BlankLineWanted.NO);
        builder.forcedBreak();
        scan(directiveTree, null);
        previousDirective = Optional.of(directiveTree.getKind());
      }
      builder.close();
      builder.forcedBreak();
      token("}");
    }
    return null;
  }

  private void visitDirective(
      String name,
      String separator,
      ExpressionTree nameExpression,
      @Nullable List<? extends ExpressionTree> items) {
    token(name);
    builder.space();
    scan(nameExpression, null);
    if (items != null) {
      builder.open(plusFour);
      builder.space();
      token(separator);
      builder.forcedBreak();
      boolean first = true;
      for (ExpressionTree item : items) {
        if (!first) {
          token(",");
          builder.forcedBreak();
        }
        scan(item, null);
        first = false;
      }
      token(";");
      builder.close();
    } else {
      token(";");
    }
  }

  @Override
  public Void visitExports(ExportsTree node, Void unused) {
    visitDirective("exports", "to", node.getPackageName(), node.getModuleNames());
    return null;
  }

  @Override
  public Void visitOpens(OpensTree node, Void unused) {
    visitDirective("opens", "to", node.getPackageName(), node.getModuleNames());
    return null;
  }

  @Override
  public Void visitProvides(ProvidesTree node, Void unused) {
    visitDirective("provides", "with", node.getServiceName(), node.getImplementationNames());
    return null;
  }

  @Override
  public Void visitRequires(RequiresTree node, Void unused) {
    token("requires");
    builder.space();
    while (true) {
      if (builder.peekToken().equals(Optional.of("static"))) {
        token("static");
        builder.space();
      } else if (builder.peekToken().equals(Optional.of("transitive"))) {
        token("transitive");
        builder.space();
      } else {
        break;
      }
    }
    scan(node.getModuleName(), null);
    token(";");
    return null;
  }

  @Override
  public Void visitUses(UsesTree node, Void unused) {
    token("uses");
    builder.space();
    scan(node.getServiceName(), null);
    token(";");
    return null;
  }

  /** Helper method for import declarations, names, and qualified names. */
  private void visitName(Tree node) {
    Deque<Name> stack = new ArrayDeque<>();
    for (; node instanceof MemberSelectTree; node = ((MemberSelectTree) node).getExpression()) {
      stack.addFirst(((MemberSelectTree) node).getIdentifier());
    }
    stack.addFirst(((IdentifierTree) node).getName());
    boolean first = true;
    for (Name name : stack) {
      if (!first) {
        token(".");
      }
      token(name.toString());
      first = false;
    }
  }

  private void visitToDeclare(
      DeclarationKind kind,
      Direction annotationsDirection,
      VariableTree node,
      Optional<ExpressionTree> initializer,
      String equals,
      Optional<String> trailing) {
    sync(node);
    Optional<TypeWithDims> typeWithDims;
    Tree type;
    if (node.getType() != null) {
      TypeWithDims extractedDims = DimensionHelpers.extractDims(node.getType(), SortedDims.YES);
      typeWithDims = Optional.of(extractedDims);
      type = extractedDims.node;
    } else {
      typeWithDims = Optional.empty();
      type = null;
    }
    declareOne(
        kind,
        annotationsDirection,
        Optional.of(node.getModifiers()),
        type,
        node.getName(),
        "",
        equals,
        initializer,
        trailing,
        /* receiverExpression= */ Optional.empty(),
        typeWithDims);
  }

  /** Does not omit the leading {@code "<"}, which should be associated with the type name. */
  protected void typeParametersRest(
      List<? extends TypeParameterTree> typeParameters, Indent plusIndent) {
    builder.open(plusIndent);
    builder.breakOp();
    builder.open(ZERO);
    boolean first = true;
    for (TypeParameterTree typeParameter : typeParameters) {
      if (!first) {
        token(",");
        builder.breakOp(" ");
      }
      scan(typeParameter, null);
      first = false;
    }
    token(">");
    builder.close();
    builder.close();
  }

  /** Collapse chains of {@code .} operators, across multiple {@link ASTNode} types. */

  /**
   * Output a "." node.
   *
   * @param node0 the "." node
   */
  void visitDot(ExpressionTree node0) {
    ExpressionTree node = node0;

    // collect a flattened list of "."-separated items
    // e.g. ImmutableList.builder().add(1).build() -> [ImmutableList, builder(), add(1), build()]
    Deque<ExpressionTree> stack = new ArrayDeque<>();
    LOOP:
    do {
      stack.addFirst(node);
      if (node.getKind() == ARRAY_ACCESS) {
        node = getArrayBase(node);
      }
      switch (node.getKind()) {
        case MEMBER_SELECT:
          node = ((MemberSelectTree) node).getExpression();
          break;
        case METHOD_INVOCATION:
          node = getMethodReceiver((MethodInvocationTree) node);
          break;
        case IDENTIFIER:
          node = null;
          break LOOP;
        default:
          // If the dot chain starts with a primary expression
          // (e.g. a class instance creation, or a conditional expression)
          // then remove it from the list and deal with it first.
          node = stack.removeFirst();
          break LOOP;
      }
    } while (node != null);
    List<ExpressionTree> items = new ArrayList<>(stack);

    boolean needDot = false;

    // The dot chain started with a primary expression: output it normally, and indent
    // the rest of the chain +4.
    if (node != null) {
      // Exception: if it's an anonymous class declaration, we don't need to
      // break and indent after the trailing '}'.
      if (node.getKind() == NEW_CLASS && ((NewClassTree) node).getClassBody() != null) {
        builder.open(ZERO);
        scan(getArrayBase(node), null);
        token(".");
      } else {
        builder.open(plusFour);
        scan(getArrayBase(node), null);
        builder.breakOp();
        needDot = true;
      }
      formatArrayIndices(getArrayIndices(node));
      if (stack.isEmpty()) {
        builder.close();
        return;
      }
    }

    Set<Integer> prefixes = new LinkedHashSet<>();

    // Check if the dot chain has a prefix that looks like a type name, so we can
    // treat the type name-shaped part as a single syntactic unit.
    TypeNameClassifier.typePrefixLength(simpleNames(stack)).ifPresent(prefixes::add);

    int invocationCount = 0;
    int firstInvocationIndex = -1;
    {
      for (int i = 0; i < items.size(); i++) {
        ExpressionTree expression = items.get(i);
        if (expression.getKind() == METHOD_INVOCATION) {
          if (i > 0 || node != null) {
            // we only want dereference invocations
            invocationCount++;
          }
          if (firstInvocationIndex < 0) {
            firstInvocationIndex = i;
          }
        }
      }
    }

    // If there's only one invocation, treat leading field accesses as a single
    // unit. In the normal case we want to preserve the alignment of subsequent
    // method calls, and would emit e.g.:
    //
    // myField
    //     .foo()
    //     .bar();
    //
    // But if there's no 'bar()' to worry about the alignment of we prefer:
    //
    // myField.foo();
    //
    // to:
    //
    // myField
    //     .foo();
    //
    if (invocationCount == 1 && firstInvocationIndex > 0) {
      prefixes.add(firstInvocationIndex);
    }

    if (prefixes.isEmpty() && items.get(0) instanceof IdentifierTree) {
      switch (((IdentifierTree) items.get(0)).getName().toString()) {
        case "this":
        case "super":
          prefixes.add(1);
          break;
        default:
          break;
      }
    }

    List<Long> streamPrefixes = handleStream(items);
    streamPrefixes.forEach(x -> prefixes.add(x.intValue()));
    if (!prefixes.isEmpty()) {
      visitDotWithPrefix(
          items, needDot, prefixes, streamPrefixes.isEmpty() ? INDEPENDENT : UNIFIED);
    } else {
      visitRegularDot(items, needDot);
    }

    if (node != null) {
      builder.close();
    }
  }

  /**
   * Output a "regular" chain of dereferences, possibly in builder-style. Break before every dot.
   *
   * @param items in the chain
   * @param needDot whether a leading dot is needed
   */
  private void visitRegularDot(List<ExpressionTree> items, boolean needDot) {
    boolean trailingDereferences = items.size() > 1;
    boolean needDot0 = needDot;
    if (!needDot0) {
      builder.open(plusFour);
    }
    // don't break after the first element if it is every small, unless the
    // chain starts with another expression
    int minLength = indentMultiplier * 4;
    int length = needDot0 ? minLength : 0;
    for (ExpressionTree e : items) {
      if (needDot) {
        if (length > minLength) {
          builder.breakOp(FillMode.UNIFIED, "", ZERO);
        }
        token(".");
        length++;
      }
      if (!fillFirstArgument(e, items, trailingDereferences ? ZERO : minusFour)) {
        BreakTag tyargTag = genSym();
        dotExpressionUpToArgs(e, Optional.of(tyargTag));
        Indent tyargIndent = Indent.If.make(tyargTag, plusFour, ZERO);
        dotExpressionArgsAndParen(
            e, tyargIndent, (trailingDereferences || needDot) ? plusFour : ZERO);
      }
      length += getLength(e, getCurrentPath());
      needDot = true;
    }
    if (!needDot0) {
      builder.close();
    }
  }

  // avoid formattings like:
  //
  // when(
  //         something
  //             .happens())
  //     .thenReturn(result);
  //
  private boolean fillFirstArgument(ExpressionTree e, List<ExpressionTree> items, Indent indent) {
    // is there a trailing dereference?
    if (items.size() < 2) {
      return false;
    }
    // don't special-case calls nested inside expressions
    if (e.getKind() != METHOD_INVOCATION) {
      return false;
    }
    MethodInvocationTree methodInvocation = (MethodInvocationTree) e;
    Name name = getMethodName(methodInvocation);
    if (!(methodInvocation.getMethodSelect() instanceof IdentifierTree)
        || name.length() > 4
        || !methodInvocation.getTypeArguments().isEmpty()
        || methodInvocation.getArguments().size() != 1) {
      return false;
    }
    builder.open(ZERO);
    builder.open(indent);
    visit(name);
    token("(");
    ExpressionTree arg = getOnlyElement(methodInvocation.getArguments());
    scan(arg, null);
    builder.close();
    token(")");
    builder.close();
    return true;
  }

  /**
   * Output a chain of dereferences where some prefix should be treated as a single syntactic unit,
   * either because it looks like a type name or because there is only a single method invocation in
   * the chain.
   *
   * @param items in the chain
   * @param needDot whether a leading dot is needed
   * @param prefixes the terminal indices of 'prefixes' of the expression that should be treated as
   *     a syntactic unit
   */
  private void visitDotWithPrefix(
      List<ExpressionTree> items,
      boolean needDot,
      Collection<Integer> prefixes,
      FillMode prefixFillMode) {
    // Are there method invocations or field accesses after the prefix?
    boolean trailingDereferences = !prefixes.isEmpty() && getLast(prefixes) < items.size() - 1;

    builder.open(plusFour);
    for (int times = 0; times < prefixes.size(); times++) {
      builder.open(ZERO);
    }

    Deque<Integer> unconsumedPrefixes = new ArrayDeque<>(ImmutableSortedSet.copyOf(prefixes));
    BreakTag nameTag = genSym();
    for (int i = 0; i < items.size(); i++) {
      ExpressionTree e = items.get(i);
      if (needDot) {
        FillMode fillMode;
        if (!unconsumedPrefixes.isEmpty() && i <= unconsumedPrefixes.peekFirst()) {
          fillMode = prefixFillMode;
        } else {
          fillMode = FillMode.UNIFIED;
        }

        builder.breakOp(fillMode, "", ZERO, Optional.of(nameTag));
        token(".");
      }
      BreakTag tyargTag = genSym();
      dotExpressionUpToArgs(e, Optional.of(tyargTag));
      if (!unconsumedPrefixes.isEmpty() && i == unconsumedPrefixes.peekFirst()) {
        builder.close();
        unconsumedPrefixes.removeFirst();
      }

      Indent tyargIndent = Indent.If.make(tyargTag, plusFour, ZERO);
      Indent argsIndent = Indent.If.make(nameTag, plusFour, trailingDereferences ? plusFour : ZERO);
      dotExpressionArgsAndParen(e, tyargIndent, argsIndent);

      needDot = true;
    }

    builder.close();
  }

  /** Returns the simple names of expressions in a "." chain. */
  private static ImmutableList<String> simpleNames(Deque<ExpressionTree> stack) {
    ImmutableList.Builder<String> simpleNames = ImmutableList.builder();
    OUTER:
    for (ExpressionTree expression : stack) {
      boolean isArray = expression.getKind() == ARRAY_ACCESS;
      expression = getArrayBase(expression);
      switch (expression.getKind()) {
        case MEMBER_SELECT:
          simpleNames.add(((MemberSelectTree) expression).getIdentifier().toString());
          break;
        case IDENTIFIER:
          simpleNames.add(((IdentifierTree) expression).getName().toString());
          break;
        case METHOD_INVOCATION:
          simpleNames.add(getMethodName((MethodInvocationTree) expression).toString());
          break OUTER;
        default:
          break OUTER;
      }
      if (isArray) {
        break OUTER;
      }
    }
    return simpleNames.build();
  }

  private void dotExpressionUpToArgs(ExpressionTree expression, Optional<BreakTag> tyargTag) {
    expression = getArrayBase(expression);
    switch (expression.getKind()) {
      case MEMBER_SELECT:
        MemberSelectTree fieldAccess = (MemberSelectTree) expression;
        visit(fieldAccess.getIdentifier());
        break;
      case METHOD_INVOCATION:
        MethodInvocationTree methodInvocation = (MethodInvocationTree) expression;
        if (!methodInvocation.getTypeArguments().isEmpty()) {
          builder.open(plusFour);
          addTypeArguments(methodInvocation.getTypeArguments(), ZERO);
          // TODO(user): Should indent the name -4.
          builder.breakOp(Doc.FillMode.UNIFIED, "", ZERO, tyargTag);
          builder.close();
        }
        visit(getMethodName(methodInvocation));
        break;
      case IDENTIFIER:
        visit(((IdentifierTree) expression).getName());
        break;
      default:
        scan(expression, null);
        break;
    }
  }

  /**
   * Returns the base expression of an erray access, e.g. given {@code foo[0][0]} returns {@code
   * foo}.
   */
  private static ExpressionTree getArrayBase(ExpressionTree node) {
    while (node instanceof ArrayAccessTree) {
      node = ((ArrayAccessTree) node).getExpression();
    }
    return node;
  }

  private static ExpressionTree getMethodReceiver(MethodInvocationTree methodInvocation) {
    ExpressionTree select = methodInvocation.getMethodSelect();
    return select instanceof MemberSelectTree ? ((MemberSelectTree) select).getExpression() : null;
  }

  private void dotExpressionArgsAndParen(
      ExpressionTree expression, Indent tyargIndent, Indent indent) {
    Deque<ExpressionTree> indices = getArrayIndices(expression);
    expression = getArrayBase(expression);
    switch (expression.getKind()) {
      case METHOD_INVOCATION:
        builder.open(tyargIndent);
        MethodInvocationTree methodInvocation = (MethodInvocationTree) expression;
        addArguments(methodInvocation.getArguments(), indent);
        builder.close();
        break;
      default:
        break;
    }
    formatArrayIndices(indices);
  }

  /** Lays out one or more array indices. Does not output the expression for the array itself. */
  private void formatArrayIndices(Deque<ExpressionTree> indices) {
    if (indices.isEmpty()) {
      return;
    }
    builder.open(ZERO);
    do {
      token("[");
      builder.breakToFill();
      scan(indices.removeLast(), null);
      token("]");
    } while (!indices.isEmpty());
    builder.close();
  }

  /**
   * Returns all array indices for the given expression, e.g. given {@code foo[0][0]} returns the
   * expressions for {@code [0][0]}.
   */
  private static Deque<ExpressionTree> getArrayIndices(ExpressionTree expression) {
    Deque<ExpressionTree> indices = new ArrayDeque<>();
    while (expression instanceof ArrayAccessTree) {
      ArrayAccessTree array = (ArrayAccessTree) expression;
      indices.addLast(array.getIndex());
      expression = array.getExpression();
    }
    return indices;
  }

  /** Helper methods for method invocations. */
  void addTypeArguments(List<? extends Tree> typeArguments, Indent plusIndent) {
    if (typeArguments == null || typeArguments.isEmpty()) {
      return;
    }
    token("<");
    builder.open(plusIndent);
    boolean first = true;
    for (Tree typeArgument : typeArguments) {
      if (!first) {
        token(",");
        builder.breakToFill(" ");
      }
      scan(typeArgument, null);
      first = false;
    }
    builder.close();
    token(">");
  }

  /**
   * Add arguments to a method invocation, etc. The arguments indented {@code plusFour}, filled,
   * from the current indent. The arguments may be output two at a time if they seem to be arguments
   * to a map constructor, etc.
   *
   * @param arguments the arguments
   * @param plusIndent the extra indent for the arguments
   */
  void addArguments(List<? extends ExpressionTree> arguments, Indent plusIndent) {
    builder.open(plusIndent);
    token("(");
    if (!arguments.isEmpty()) {
      if (arguments.size() % 2 == 0 && argumentsAreTabular(arguments) == 2) {
        builder.forcedBreak();
        builder.open(ZERO);
        boolean first = true;
        for (int i = 0; i < arguments.size() - 1; i += 2) {
          ExpressionTree argument0 = arguments.get(i);
          ExpressionTree argument1 = arguments.get(i + 1);
          if (!first) {
            token(",");
            builder.forcedBreak();
          }
          builder.open(plusFour);
          scan(argument0, null);
          token(",");
          builder.breakOp(" ");
          scan(argument1, null);
          builder.close();
          first = false;
        }
        builder.close();
      } else if (isFormatMethod(arguments)) {
        builder.breakOp();
        builder.open(ZERO);
        scan(arguments.get(0), null);
        token(",");
        builder.breakOp(" ");
        builder.open(ZERO);
        argList(arguments.subList(1, arguments.size()));
        builder.close();
        builder.close();
      } else {
        builder.breakOp();
        argList(arguments);
      }
    }
    token(")");
    builder.close();
  }

  private void argList(List<? extends ExpressionTree> arguments) {
    builder.open(ZERO);
    boolean first = true;
    FillMode fillMode = hasOnlyShortItems(arguments) ? FillMode.INDEPENDENT : FillMode.UNIFIED;
    for (ExpressionTree argument : arguments) {
      if (!first) {
        token(",");
        builder.breakOp(fillMode, " ", ZERO);
      }
      scan(argument, null);
      first = false;
    }
    builder.close();
  }

  /**
   * Identifies String formatting methods like {@link String#format} which we prefer to format as:
   *
   * <pre>{@code
   * String.format(
   *     "the format string: %s %s %s",
   *     arg, arg, arg);
   * }</pre>
   *
   * <p>And not:
   *
   * <pre>{@code
   * String.format(
   *     "the format string: %s %s %s",
   *     arg,
   *     arg,
   *     arg);
   * }</pre>
   */
  private boolean isFormatMethod(List<? extends ExpressionTree> arguments) {
    if (arguments.size() < 2) {
      return false;
    }
    return isStringConcat(arguments.get(0));
  }

  private static final Pattern FORMAT_SPECIFIER = Pattern.compile("%|\\{[0-9]\\}");

  private boolean isStringConcat(ExpressionTree first) {
    final boolean[] stringLiteral = {true};
    final boolean[] formatString = {false};
    new TreeScanner() {
      @Override
      public void scan(JCTree tree) {
        if (tree == null) {
          return;
        }
        switch (tree.getKind()) {
          case STRING_LITERAL:
            break;
          case PLUS:
            super.scan(tree);
            break;
          default:
            stringLiteral[0] = false;
            break;
        }
        if (tree.getKind() == STRING_LITERAL) {
          Object value = ((LiteralTree) tree).getValue();
          if (value instanceof String && FORMAT_SPECIFIER.matcher(value.toString()).find()) {
            formatString[0] = true;
          }
        }
      }
    }.scan((JCTree) first);
    return stringLiteral[0] && formatString[0];
  }

  /** Returns the number of columns if the arguments arg laid out in a grid, or else {@code -1}. */
  private int argumentsAreTabular(List<? extends ExpressionTree> arguments) {
    if (arguments.isEmpty()) {
      return -1;
    }
    List<List<ExpressionTree>> rows = new ArrayList<>();
    PeekingIterator<ExpressionTree> it = Iterators.peekingIterator(arguments.iterator());
    int start0 = actualColumn(it.peek());
    {
      List<ExpressionTree> row = new ArrayList<>();
      row.add(it.next());
      while (it.hasNext() && actualColumn(it.peek()) > start0) {
        row.add(it.next());
      }
      if (!it.hasNext()) {
        return -1;
      }
      if (rowLength(row) <= 1) {
        return -1;
      }
      rows.add(row);
    }
    while (it.hasNext()) {
      List<ExpressionTree> row = new ArrayList<>();
      int start = actualColumn(it.peek());
      if (start != start0) {
        return -1;
      }
      row.add(it.next());
      while (it.hasNext() && actualColumn(it.peek()) > start0) {
        row.add(it.next());
      }
      rows.add(row);
    }
    int size0 = rows.get(0).size();
    if (!expressionsAreParallel(rows, 0, rows.size())) {
      return -1;
    }
    for (int i = 1; i < size0; i++) {
      if (!expressionsAreParallel(rows, i, rows.size() / 2 + 1)) {
        return -1;
      }
    }
    // if there are only two rows, they must be the same length
    if (rows.size() == 2) {
      if (size0 == rows.get(1).size()) {
        return size0;
      }
      return -1;
    }
    // allow a ragged trailing row for >= 3 columns
    for (int i = 1; i < rows.size() - 1; i++) {
      if (size0 != rows.get(i).size()) {
        return -1;
      }
    }
    if (size0 < getLast(rows).size()) {
      return -1;
    }
    return size0;
  }

  static int rowLength(List<? extends ExpressionTree> row) {
    int size = 0;
    for (ExpressionTree tree : row) {
      if (tree.getKind() != NEW_ARRAY) {
        size++;
        continue;
      }
      NewArrayTree array = (NewArrayTree) tree;
      if (array.getInitializers() == null) {
        size++;
        continue;
      }
      size += rowLength(array.getInitializers());
    }
    return size;
  }

  private Integer actualColumn(ExpressionTree expression) {
    Map<Integer, Integer> positionToColumnMap = builder.getInput().getPositionToColumnMap();
    return positionToColumnMap.get(builder.actualStartColumn(getStartPosition(expression)));
  }

  /** Returns true if {@code atLeastM} of the expressions in the given column are the same kind. */
  private static boolean expressionsAreParallel(
      List<List<ExpressionTree>> rows, int column, int atLeastM) {
    Multiset<Tree.Kind> nodeTypes = HashMultiset.create();
    for (List<? extends ExpressionTree> row : rows) {
      if (column >= row.size()) {
        continue;
      }
      // Treat UnaryTree expressions as their underlying type for the comparison (so, for example
      // -ve and +ve numeric literals are considered the same).
      if (row.get(column) instanceof UnaryTree) {
        nodeTypes.add(((UnaryTree) row.get(column)).getExpression().getKind());
      } else {
        nodeTypes.add(row.get(column).getKind());
      }
    }
    for (Multiset.Entry<Tree.Kind> nodeType : nodeTypes.entrySet()) {
      if (nodeType.getCount() >= atLeastM) {
        return true;
      }
    }
    return false;
  }

  // General helper functions.

  enum DeclarationKind {
    NONE,
    FIELD,
    PARAMETER
  }

  /** Declare one variable or variable-like thing. */
  int declareOne(
      DeclarationKind kind,
      Direction annotationsDirection,
      Optional<ModifiersTree> modifiers,
      Tree type,
      Name name,
      String op,
      String equals,
      Optional<ExpressionTree> initializer,
      Optional<String> trailing,
      Optional<ExpressionTree> receiverExpression,
      Optional<TypeWithDims> typeWithDims) {

    BreakTag typeBreak = genSym();
    BreakTag verticalAnnotationBreak = genSym();

    // If the node is a field declaration, try to output any declaration
    // annotations in-line. If the entire declaration doesn't fit on a single
    // line, fall back to one-per-line.
    boolean isField = kind == DeclarationKind.FIELD;

    if (isField) {
      builder.blankLineWanted(BlankLineWanted.conditional(verticalAnnotationBreak));
    }

    Deque<List<? extends AnnotationTree>> dims =
        new ArrayDeque<>(typeWithDims.isPresent() ? typeWithDims.get().dims : ImmutableList.of());
    int baseDims = 0;

    // preprocess to separate declaration annotations + modifiers, type annotations

    DeclarationModifiersAndTypeAnnotations declarationAndTypeModifiers =
        modifiers
            .map(m -> splitModifiers(m, m.getAnnotations()))
            .orElse(DeclarationModifiersAndTypeAnnotations.empty());
    builder.open(
        kind == DeclarationKind.PARAMETER && declarationAndTypeModifiers.hasDeclarationAnnotation()
            ? plusFour
            : ZERO);
    {
      List<AnnotationTree> annotations =
          visitModifiers(
              declarationAndTypeModifiers,
              annotationsDirection,
              Optional.of(verticalAnnotationBreak));
      boolean isVar =
          builder.peekToken().get().equals("var")
              && (!name.contentEquals("var") || builder.peekToken(1).get().equals("var"));
      boolean hasType = type != null || isVar;
      builder.open(hasType ? plusFour : ZERO);
      {
        builder.open(ZERO);
        {
          builder.open(ZERO);
          {
            visitAnnotations(annotations, BreakOrNot.NO, BreakOrNot.YES);
            if (typeWithDims.isPresent() && typeWithDims.get().node != null) {
              scan(typeWithDims.get().node, null);
              int totalDims = dims.size();
              builder.open(plusFour);
              maybeAddDims(dims);
              builder.close();
              baseDims = totalDims - dims.size();
            } else if (isVar) {
              token("var");
            } else {
              scan(type, null);
            }
          }
          builder.close();

          if (hasType) {
            builder.breakOp(Doc.FillMode.INDEPENDENT, " ", ZERO, Optional.of(typeBreak));
          }

          // conditionally ident the name and initializer +4 if the type spans
          // multiple lines
          builder.open(Indent.If.make(typeBreak, plusFour, ZERO));
          if (receiverExpression.isPresent()) {
            scan(receiverExpression.get(), null);
          } else {
            visit(name);
          }
          builder.op(op);
        }
        maybeAddDims(dims);
        builder.close();
      }
      builder.close();

      if (initializer.isPresent()) {
        builder.space();
        token(equals);
        if (initializer.get().getKind() == Tree.Kind.NEW_ARRAY
            && ((NewArrayTree) initializer.get()).getType() == null) {
          builder.open(minusFour);
          builder.space();
          initializer.get().accept(this, null);
          builder.close();
        } else {
          builder.open(Indent.If.make(typeBreak, plusFour, ZERO));
          {
            builder.breakToFill(" ");
            scan(initializer.get(), null);
          }
          builder.close();
        }
      }
      if (trailing.isPresent() && builder.peekToken().equals(trailing)) {
        builder.guessToken(trailing.get());
      }

      // end of conditional name and initializer indent
      builder.close();
    }
    builder.close();

    if (isField) {
      builder.blankLineWanted(BlankLineWanted.conditional(verticalAnnotationBreak));
    }

    return baseDims;
  }

  private void maybeAddDims(Deque<List<? extends AnnotationTree>> annotations) {
    maybeAddDims(new ArrayDeque<>(), annotations);
  }

  /**
   * The compiler does not always preserve the concrete syntax of annotated array dimensions, and
   * mixed-notation array dimensions. Use look-ahead to preserve the original syntax.
   *
   * <p>It is assumed that any number of regular dimension specifiers ({@code []} with no
   * annotations) may be present in the input.
   *
   * @param dimExpressions an ordered list of dimension expressions (e.g. the {@code 0} in {@code
   *     new int[0]}
   * @param annotations an ordered list of type annotations grouped by dimension (e.g. {@code
   *     [[@A, @B], [@C]]} for {@code int @A [] @B @C []}
   */
  private void maybeAddDims(
      Deque<ExpressionTree> dimExpressions, Deque<List<? extends AnnotationTree>> annotations) {
    boolean lastWasAnnotation = false;
    while (builder.peekToken().isPresent()) {
      switch (builder.peekToken().get()) {
        case "@":
          if (annotations.isEmpty()) {
            return;
          }
          List<? extends AnnotationTree> dimAnnotations = annotations.removeFirst();
          if (dimAnnotations.isEmpty()) {
            continue;
          }
          builder.breakToFill(" ");
          visitAnnotations(dimAnnotations, BreakOrNot.NO, BreakOrNot.NO);
          lastWasAnnotation = true;
          break;
        case "[":
          if (lastWasAnnotation) {
            builder.breakToFill(" ");
          } else {
            builder.breakToFill();
          }
          token("[");
          if (!builder.peekToken().get().equals("]")) {
            scan(dimExpressions.removeFirst(), null);
          }
          token("]");
          lastWasAnnotation = false;
          break;
        case ".":
          if (!builder.peekToken().get().equals(".") || !builder.peekToken(1).get().equals(".")) {
            return;
          }
          if (lastWasAnnotation) {
            builder.breakToFill(" ");
          } else {
            builder.breakToFill();
          }
          builder.op("...");
          lastWasAnnotation = false;
          break;
        default:
          return;
      }
    }
  }

  private void declareMany(List<VariableTree> fragments, Direction annotationDirection) {
    builder.open(ZERO);

    ModifiersTree modifiers = fragments.get(0).getModifiers();
    Tree type = fragments.get(0).getType();

    visitAndBreakModifiers(
        modifiers, annotationDirection, /* declarationAnnotationBreak= */ Optional.empty());
    builder.open(plusFour);
    builder.open(ZERO);
    TypeWithDims extractedDims = DimensionHelpers.extractDims(type, SortedDims.YES);
    Deque<List<? extends AnnotationTree>> dims = new ArrayDeque<>(extractedDims.dims);
    scan(extractedDims.node, null);
    int baseDims = dims.size();
    maybeAddDims(dims);
    baseDims = baseDims - dims.size();
    boolean first = true;
    for (VariableTree fragment : fragments) {
      if (!first) {
        token(",");
      }
      TypeWithDims fragmentDims = variableFragmentDims(first, baseDims, fragment.getType());
      dims = new ArrayDeque<>(fragmentDims.dims);
      builder.breakOp(" ");
      builder.open(ZERO);
      maybeAddDims(dims);
      visit(fragment.getName());
      maybeAddDims(dims);
      ExpressionTree initializer = fragment.getInitializer();
      if (initializer != null) {
        builder.space();
        token("=");
        builder.open(plusFour);
        builder.breakOp(" ");
        scan(initializer, null);
        builder.close();
      }
      builder.close();
      if (first) {
        builder.close();
      }
      first = false;
    }
    builder.close();
    token(";");
    builder.close();
  }

  /** Add a list of declarations. */
  protected void addBodyDeclarations(
      List<? extends Tree> bodyDeclarations, BracesOrNot braces, FirstDeclarationsOrNot first0) {
    if (bodyDeclarations.isEmpty()) {
      if (braces.isYes()) {
        builder.space();
        tokenBreakTrailingComment("{", plusTwo);
        builder.blankLineWanted(BlankLineWanted.NO);
        builder.open(ZERO);
        token("}", plusTwo);
        builder.close();
      }
    } else {
      if (braces.isYes()) {
        builder.space();
        tokenBreakTrailingComment("{", plusTwo);
        builder.open(ZERO);
      }
      builder.open(plusTwo);
      boolean first = first0.isYes();
      boolean lastOneGotBlankLineBefore = false;
      PeekingIterator<Tree> it = Iterators.peekingIterator(bodyDeclarations.iterator());
      while (it.hasNext()) {
        Tree bodyDeclaration = it.next();
        dropEmptyDeclarations();
        builder.forcedBreak();
        boolean thisOneGetsBlankLineBefore =
            bodyDeclaration.getKind() != VARIABLE || hasJavaDoc(bodyDeclaration);
        if (first) {
          builder.blankLineWanted(PRESERVE);
        } else if (!first && (thisOneGetsBlankLineBefore || lastOneGotBlankLineBefore)) {
          builder.blankLineWanted(YES);
        }
        markForPartialFormat();

        if (bodyDeclaration.getKind() == VARIABLE) {
          visitVariables(
              variableFragments(it, bodyDeclaration),
              DeclarationKind.FIELD,
              fieldAnnotationDirection(((VariableTree) bodyDeclaration).getModifiers()));
        } else {
          scan(bodyDeclaration, null);
        }
        first = false;
        lastOneGotBlankLineBefore = thisOneGetsBlankLineBefore;
      }
      dropEmptyDeclarations();
      builder.forcedBreak();
      builder.close();
      builder.forcedBreak();
      markForPartialFormat();
      if (braces.isYes()) {
        builder.blankLineWanted(BlankLineWanted.NO);
        token("}", plusTwo);
        builder.close();
      }
    }
  }

  /** Gets the permits clause for the given node. This is only available in Java 15 and later. */
  protected List<? extends Tree> getPermitsClause(ClassTree node) {
    return ImmutableList.of();
  }

  private void classDeclarationTypeList(String token, List<? extends Tree> types) {
    if (types.isEmpty()) {
      return;
    }
    builder.breakToFill(" ");
    builder.open(types.size() > 1 ? plusFour : ZERO);
    token(token);
    builder.space();
    boolean first = true;
    for (Tree type : types) {
      if (!first) {
        token(",");
        builder.breakOp(" ");
      }
      scan(type, null);
      first = false;
    }
    builder.close();
  }

  /**
   * The parser expands multi-variable declarations into separate single-variable declarations. All
   * of the fragments in the original declaration have the same start position, so we use that as a
   * signal to collect them and preserve the multi-variable declaration in the output.
   *
   * <p>e.g. {@code int x, y;} is parsed as {@code int x; int y;}.
   */
  private static List<VariableTree> variableFragments(
      PeekingIterator<? extends Tree> it, Tree first) {
    List<VariableTree> fragments = new ArrayList<>();
    if (first.getKind() == VARIABLE) {
      int start = getStartPosition(first);
      fragments.add((VariableTree) first);
      while (it.hasNext()
          && it.peek().getKind() == VARIABLE
          && getStartPosition(it.peek()) == start) {
        fragments.add((VariableTree) it.next());
      }
    }
    return fragments;
  }

  /** Does this declaration have javadoc preceding it? */
  private boolean hasJavaDoc(Tree bodyDeclaration) {
    int position = ((JCTree) bodyDeclaration).getStartPosition();
    Input.Token token = builder.getInput().getPositionTokenMap().get(position);
    if (token != null) {
      for (Input.Tok tok : token.getToksBefore()) {
        if (tok.getText().startsWith("/**")) {
          return true;
        }
      }
    }
    return false;
  }

  private static Optional<? extends Input.Token> getNextToken(Input input, int position) {
    return Optional.ofNullable(input.getPositionTokenMap().get(position));
  }

  /** Does this list of trees end with the specified token? */
  private boolean hasTrailingToken(Input input, List<? extends Tree> nodes, String token) {
    if (nodes.isEmpty()) {
      return false;
    }
    Tree lastNode = getLast(nodes);
    Optional<? extends Input.Token> nextToken =
        getNextToken(input, getEndPosition(lastNode, getCurrentPath()));
    return nextToken.isPresent() && nextToken.get().getTok().getText().equals(token);
  }

  /**
   * Can a local with a set of modifiers be declared with horizontal annotations? This is currently
   * true if there is at most one parameterless annotation, and no others.
   *
   * @param modifiers the list of {@link ModifiersTree}s
   * @return whether the local can be declared with horizontal annotations
   */
  private static Direction canLocalHaveHorizontalAnnotations(ModifiersTree modifiers) {
    int parameterlessAnnotations = 0;
    for (AnnotationTree annotation : modifiers.getAnnotations()) {
      if (annotation.getArguments().isEmpty()) {
        parameterlessAnnotations++;
      }
    }
    return parameterlessAnnotations <= 1
            && parameterlessAnnotations == modifiers.getAnnotations().size()
        ? Direction.HORIZONTAL
        : Direction.VERTICAL;
  }

  /**
   * Should a field with a set of modifiers be declared with horizontal annotations? This is
   * currently true if all annotations are parameterless annotations.
   */
  private static Direction fieldAnnotationDirection(ModifiersTree modifiers) {
    for (AnnotationTree annotation : modifiers.getAnnotations()) {
      if (!annotation.getArguments().isEmpty()) {
        return Direction.VERTICAL;
      }
    }
    return Direction.HORIZONTAL;
  }

  /**
   * Emit a {@link Doc.Token}.
   *
   * @param token the {@link String} to wrap in a {@link Doc.Token}
   */
  protected final void token(String token) {
    builder.token(
        token,
        Doc.Token.RealOrImaginary.REAL,
        ZERO,
        /* breakAndIndentTrailingComment= */ Optional.empty());
  }

  /**
   * Emit a {@link Doc.Token}.
   *
   * @param token the {@link String} to wrap in a {@link Doc.Token}
   * @param plusIndentCommentsBefore extra indent for comments before this token
   */
  protected final void token(String token, Indent plusIndentCommentsBefore) {
    builder.token(
        token,
        Doc.Token.RealOrImaginary.REAL,
        plusIndentCommentsBefore,
        /* breakAndIndentTrailingComment= */ Optional.empty());
  }

  /** Emit a {@link Doc.Token}, and breaks and indents trailing javadoc or block comments. */
  final void tokenBreakTrailingComment(String token, Indent breakAndIndentTrailingComment) {
    builder.token(
        token, Doc.Token.RealOrImaginary.REAL, ZERO, Optional.of(breakAndIndentTrailingComment));
  }

  protected void markForPartialFormat() {
    if (!inExpression()) {
      builder.markForPartialFormat();
    }
  }

  /**
   * Sync to position in the input. If we've skipped outputting any tokens that were present in the
   * input tokens, output them here and complain.
   *
   * @param node the ASTNode holding the input position
   */
  protected final void sync(Tree node) {
    builder.sync(((JCTree) node).getStartPosition());
  }

  final BreakTag genSym() {
    return new BreakTag();
  }

  @Override
  public final String toString() {
    return MoreObjects.toStringHelper(this).add("builder", builder).toString();
  }
}
