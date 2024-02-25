/*
 * Copyright 2016 Google Inc.
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
import static com.google.common.primitives.Booleans.trueFirst;

import com.google.common.base.CharMatcher;
import com.google.common.base.Preconditions;
import com.google.common.base.Splitter;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.ImmutableSortedSet;
import com.google.googlejavaformat.Newlines;
import com.google.googlejavaformat.java.JavaFormatterOptions.Style;
import com.google.googlejavaformat.java.JavaInput.Tok;
import com.sun.tools.javac.parser.Tokens.TokenKind;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Optional;
import java.util.function.BiFunction;
import java.util.stream.Stream;

/** Orders imports in Java source code. */
public class ImportOrderer {

  private static final Splitter DOT_SPLITTER = Splitter.on('.');

  /**
   * Reorder the inputs in {@code text}, a complete Java program. On success, another complete Java
   * program is returned, which is the same as the original except the imports are in order.
   *
   * @throws FormatterException if the input could not be parsed.
   */
  public static String reorderImports(String text, Style style) throws FormatterException {
    ImmutableList<Tok> toks = JavaInput.buildToks(text, CLASS_START);
    return new ImportOrderer(text, toks, style).reorderImports();
  }

  /**
   * Reorder the inputs in {@code text}, a complete Java program, in Google style. On success,
   * another complete Java program is returned, which is the same as the original except the imports
   * are in order.
   *
   * @deprecated Use {@link #reorderImports(String, Style)} instead
   * @throws FormatterException if the input could not be parsed.
   */
  @Deprecated
  public static String reorderImports(String text) throws FormatterException {
    return reorderImports(text, Style.GOOGLE);
  }

  private String reorderImports() throws FormatterException {
    int firstImportStart;
    Optional<Integer> maybeFirstImport = findIdentifier(0, IMPORT_OR_CLASS_START);
    if (!maybeFirstImport.isPresent() || !tokenAt(maybeFirstImport.get()).equals("import")) {
      // No imports, so nothing to do.
      return text;
    }
    firstImportStart = maybeFirstImport.get();
    int unindentedFirstImportStart = unindent(firstImportStart);

    ImportsAndIndex imports = scanImports(firstImportStart);
    int afterLastImport = imports.index;

    // Make sure there are no more imports before the next class (etc) definition.
    Optional<Integer> maybeLaterImport = findIdentifier(afterLastImport, IMPORT_OR_CLASS_START);
    if (maybeLaterImport.isPresent() && tokenAt(maybeLaterImport.get()).equals("import")) {
      throw new FormatterException("Imports not contiguous (perhaps a comment separates them?)");
    }

    StringBuilder result = new StringBuilder();
    String prefix = tokString(0, unindentedFirstImportStart);
    result.append(prefix);
    if (!prefix.isEmpty() && Newlines.getLineEnding(prefix) == null) {
      result.append(lineSeparator).append(lineSeparator);
    }
    result.append(reorderedImportsString(imports.imports));

    List<String> tail = new ArrayList<>();
    tail.add(CharMatcher.whitespace().trimLeadingFrom(tokString(afterLastImport, toks.size())));
    if (!toks.isEmpty()) {
      Tok lastTok = getLast(toks);
      int tailStart = lastTok.getPosition() + lastTok.length();
      tail.add(text.substring(tailStart));
    }
    if (tail.stream().anyMatch(s -> !s.isEmpty())) {
      result.append(lineSeparator);
      tail.forEach(result::append);
    }

    return result.toString();
  }

  /**
   * {@link TokenKind}s that indicate the start of a type definition. We use this to avoid scanning
   * the whole file, since we know that imports must precede any type definition.
   */
  private static final ImmutableSet<TokenKind> CLASS_START =
      ImmutableSet.of(TokenKind.CLASS, TokenKind.INTERFACE, TokenKind.ENUM);

  /**
   * We use this set to find the first import, and again to check that there are no imports after
   * the place we stopped gathering them. An annotation definition ({@code @interface}) is two
   * tokens, the second which is {@code interface}, so we don't need a separate entry for that.
   */
  private static final ImmutableSet<String> IMPORT_OR_CLASS_START =
      ImmutableSet.of("import", "class", "interface", "enum");

  /**
   * A {@link Comparator} that orders {@link Import}s by Google Style, defined at
   * https://google.github.io/styleguide/javaguide.html#s3.3.3-import-ordering-and-spacing.
   */
  private static final Comparator<Import> GOOGLE_IMPORT_COMPARATOR =
      Comparator.comparing(Import::isStatic, trueFirst()).thenComparing(Import::imported);

  /**
   * A {@link Comparator} that orders {@link Import}s by AOSP Style, defined at
   * https://source.android.com/setup/contribute/code-style#order-import-statements and implemented
   * in IntelliJ at
   * https://android.googlesource.com/platform/development/+/master/ide/intellij/codestyles/AndroidStyle.xml.
   */
  private static final Comparator<Import> AOSP_IMPORT_COMPARATOR =
      Comparator.comparing(Import::isStatic, trueFirst())
          .thenComparing(Import::isAndroid, trueFirst())
          .thenComparing(Import::isThirdParty, trueFirst())
          .thenComparing(Import::isJava, trueFirst())
          .thenComparing(Import::imported);

  /**
   * Determines whether to insert a blank line between the {@code prev} and {@code curr} {@link
   * Import}s based on Google style.
   */
  private static boolean shouldInsertBlankLineGoogle(Import prev, Import curr) {
    return prev.isStatic() && !curr.isStatic();
  }

  /**
   * Determines whether to insert a blank line between the {@code prev} and {@code curr} {@link
   * Import}s based on AOSP style.
   */
  private static boolean shouldInsertBlankLineAosp(Import prev, Import curr) {
    if (prev.isStatic() && !curr.isStatic()) {
      return true;
    }
    // insert blank line between "com.android" from "com.anythingelse"
    if (prev.isAndroid() && !curr.isAndroid()) {
      return true;
    }
    return !prev.topLevel().equals(curr.topLevel());
  }

  private final String text;
  private final ImmutableList<Tok> toks;
  private final String lineSeparator;
  private final Comparator<Import> importComparator;
  private final BiFunction<Import, Import, Boolean> shouldInsertBlankLineFn;

  private ImportOrderer(String text, ImmutableList<Tok> toks, Style style) {
    this.text = text;
    this.toks = toks;
    this.lineSeparator = Newlines.guessLineSeparator(text);
    if (style.equals(Style.GOOGLE)) {
      this.importComparator = GOOGLE_IMPORT_COMPARATOR;
      this.shouldInsertBlankLineFn = ImportOrderer::shouldInsertBlankLineGoogle;
    } else if (style.equals(Style.AOSP)) {
      this.importComparator = AOSP_IMPORT_COMPARATOR;
      this.shouldInsertBlankLineFn = ImportOrderer::shouldInsertBlankLineAosp;
    } else {
      throw new IllegalArgumentException("Unsupported code style: " + style);
    }
  }

  /** An import statement. */
  class Import {
    private final String imported;
    private final boolean isStatic;
    private final String trailing;

    Import(String imported, String trailing, boolean isStatic) {
      this.imported = imported;
      this.trailing = trailing;
      this.isStatic = isStatic;
    }

    /** The name being imported, for example {@code java.util.List}. */
    String imported() {
      return imported;
    }

    /** True if this is {@code import static}. */
    boolean isStatic() {
      return isStatic;
    }

    /** The top-level package of the import. */
    String topLevel() {
      return DOT_SPLITTER.split(imported()).iterator().next();
    }

    /** True if this is an Android import per AOSP style. */
    boolean isAndroid() {
      return Stream.of("android.", "androidx.", "dalvik.", "libcore.", "com.android.")
          .anyMatch(imported::startsWith);
    }

    /** True if this is a Java import per AOSP style. */
    boolean isJava() {
      switch (topLevel()) {
        case "java":
        case "javax":
          return true;
        default:
          return false;
      }
    }

    /**
     * The {@code //} comment lines after the final {@code ;}, up to and including the line
     * terminator of the last one. Note: In case two imports were separated by a space (which is
     * disallowed by the style guide), the trailing whitespace of the first import does not include
     * a line terminator.
     */
    String trailing() {
      return trailing;
    }

    /** True if this is a third-party import per AOSP style. */
    public boolean isThirdParty() {
      return !(isAndroid() || isJava());
    }

    // One or multiple lines, the import itself and following comments, including the line
    // terminator.
    @Override
    public String toString() {
      StringBuilder sb = new StringBuilder();
      sb.append("import ");
      if (isStatic()) {
        sb.append("static ");
      }
      sb.append(imported()).append(';');
      if (trailing().trim().isEmpty()) {
        sb.append(lineSeparator);
      } else {
        sb.append(trailing());
      }
      return sb.toString();
    }
  }

  private String tokString(int start, int end) {
    StringBuilder sb = new StringBuilder();
    for (int i = start; i < end; i++) {
      sb.append(toks.get(i).getOriginalText());
    }
    return sb.toString();
  }

  private static class ImportsAndIndex {
    final ImmutableSortedSet<Import> imports;
    final int index;

    ImportsAndIndex(ImmutableSortedSet<Import> imports, int index) {
      this.imports = imports;
      this.index = index;
    }
  }

  /**
   * Scans a sequence of import lines. The parsing uses this approximate grammar:
   *
   * <pre>{@code
   * <imports> -> (<end-of-line> | <import>)*
   * <import> -> "import" <whitespace> ("static" <whitespace>)?
   *    <identifier> ("." <identifier>)* ("." "*")? <whitespace>? ";"
   *    <whitespace>? <end-of-line>? (<line-comment> <end-of-line>)*
   * }</pre>
   *
   * @param i the index to start parsing at.
   * @return the result of parsing the imports.
   * @throws FormatterException if imports could not parsed according to the grammar.
   */
  private ImportsAndIndex scanImports(int i) throws FormatterException {
    int afterLastImport = i;
    ImmutableSortedSet.Builder<Import> imports = ImmutableSortedSet.orderedBy(importComparator);
    // JavaInput.buildToks appends a zero-width EOF token after all tokens. It won't match any
    // of our tests here and protects us from running off the end of the toks list. Since it is
    // zero-width it doesn't matter if we include it in our string concatenation at the end.
    while (i < toks.size() && tokenAt(i).equals("import")) {
      i++;
      if (isSpaceToken(i)) {
        i++;
      }
      boolean isStatic = tokenAt(i).equals("static");
      if (isStatic) {
        i++;
        if (isSpaceToken(i)) {
          i++;
        }
      }
      if (!isIdentifierToken(i)) {
        throw new FormatterException("Unexpected token after import: " + tokenAt(i));
      }
      StringAndIndex imported = scanImported(i);
      String importedName = imported.string;
      i = imported.index;
      if (isSpaceToken(i)) {
        i++;
      }
      if (!tokenAt(i).equals(";")) {
        throw new FormatterException("Expected ; after import");
      }
      while (tokenAt(i).equals(";")) {
        // Extra semicolons are not allowed by the JLS but are accepted by javac.
        i++;
      }
      StringBuilder trailing = new StringBuilder();
      if (isSpaceToken(i)) {
        trailing.append(tokenAt(i));
        i++;
      }
      if (isNewlineToken(i)) {
        trailing.append(tokenAt(i));
        i++;
      }
      // Gather (if any) all single line comments and accompanied line terminators following this
      // import
      while (isSlashSlashCommentToken(i)) {
        trailing.append(tokenAt(i));
        i++;
        if (isNewlineToken(i)) {
          trailing.append(tokenAt(i));
          i++;
        }
      }
      while (tokenAt(i).equals(";")) {
        // Extra semicolons are not allowed by the JLS but are accepted by javac.
        i++;
      }
      imports.add(new Import(importedName, trailing.toString(), isStatic));
      // Remember the position just after the import we just saw, before skipping blank lines.
      // If the next thing after the blank lines is not another import then we don't want to
      // include those blank lines in the text to be replaced.
      afterLastImport = i;
      while (isNewlineToken(i) || isSpaceToken(i)) {
        i++;
      }
    }
    return new ImportsAndIndex(imports.build(), afterLastImport);
  }

  // Produces the sorted output based on the imports we have scanned.
  private String reorderedImportsString(ImmutableSortedSet<Import> imports) {
    Preconditions.checkArgument(!imports.isEmpty(), "imports");

    // Pretend that the first import was preceded by another import of the same kind, so we don't
    // insert a newline there.
    Import prevImport = imports.iterator().next();

    StringBuilder sb = new StringBuilder();
    for (Import currImport : imports) {
      if (shouldInsertBlankLineFn.apply(prevImport, currImport)) {
        // Blank line between static and non-static imports.
        sb.append(lineSeparator);
      }
      sb.append(currImport);
      prevImport = currImport;
    }
    return sb.toString();
  }

  private static class StringAndIndex {
    private final String string;
    private final int index;

    StringAndIndex(String string, int index) {
      this.string = string;
      this.index = index;
    }
  }

  /**
   * Scans the imported thing, the dot-separated name that comes after import [static] and before
   * the semicolon. We don't allow spaces inside the dot-separated name. Wildcard imports are
   * supported: if the input is {@code import java.util.*;} then the returned string will be {@code
   * java.util.*}.
   *
   * @param start the index of the start of the identifier. If the import is {@code import
   *     java.util.List;} then this index points to the token {@code java}.
   * @return the parsed import ({@code java.util.List} in the example) and the index of the first
   *     token after the imported thing ({@code ;} in the example).
   * @throws FormatterException if the imported name could not be parsed.
   */
  private StringAndIndex scanImported(int start) throws FormatterException {
    int i = start;
    StringBuilder imported = new StringBuilder();
    // At the start of each iteration of this loop, i points to an identifier.
    // On exit from the loop, i points to a token after an identifier or after *.
    while (true) {
      Preconditions.checkState(isIdentifierToken(i));
      imported.append(tokenAt(i));
      i++;
      if (!tokenAt(i).equals(".")) {
        return new StringAndIndex(imported.toString(), i);
      }
      imported.append('.');
      i++;
      if (tokenAt(i).equals("*")) {
        imported.append('*');
        return new StringAndIndex(imported.toString(), i + 1);
      } else if (!isIdentifierToken(i)) {
        throw new FormatterException("Could not parse imported name, at: " + tokenAt(i));
      }
    }
  }

  /**
   * Returns the index of the first place where one of the given identifiers occurs, or {@code
   * Optional.empty()} if there is none.
   *
   * @param start the index to start looking at
   * @param identifiers the identifiers to look for
   */
  private Optional<Integer> findIdentifier(int start, ImmutableSet<String> identifiers) {
    for (int i = start; i < toks.size(); i++) {
      if (isIdentifierToken(i)) {
        String id = tokenAt(i);
        if (identifiers.contains(id)) {
          return Optional.of(i);
        }
      }
    }
    return Optional.empty();
  }

  /** Returns the given token, or the preceding token if it is a whitespace token. */
  private int unindent(int i) {
    if (i > 0 && isSpaceToken(i - 1)) {
      return i - 1;
    } else {
      return i;
    }
  }

  private String tokenAt(int i) {
    return toks.get(i).getOriginalText();
  }

  private boolean isIdentifierToken(int i) {
    String s = tokenAt(i);
    return !s.isEmpty() && Character.isJavaIdentifierStart(s.codePointAt(0));
  }

  private boolean isSpaceToken(int i) {
    String s = tokenAt(i);
    if (s.isEmpty()) {
      return false;
    } else {
      return " \t\f".indexOf(s.codePointAt(0)) >= 0;
    }
  }

  private boolean isSlashSlashCommentToken(int i) {
    return toks.get(i).isSlashSlashComment();
  }

  private boolean isNewlineToken(int i) {
    return toks.get(i).isNewline();
  }
}
