// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {EXPR_PREFIX, FALSE_TEMPLATE_PREFIX, FORMAT_OFF_PREFIX, getChildDepthForNode, getDepthForNode, getDepthForTagName, getIndentationPrefix, INDENT_SIZE, LINE_LENGTH_LIMIT, PROP_PREFIX, RESTRICTED_TAGS, TEMPLATE_PREFIX, TRAILING_NEWLINE_REGEX, VOID_ELEMENTS, WRAPPED_LINE_INDENT_SIZE} from './html_utils.js';

const PREFIX_REGEX = /^[?.]/;

function getOrder(name) {
  if (name === 'id') {
    return 1;
  }
  if (name === 'class') {
    return 2;
  }
  if (name === 'part') {
    return 3;
  }
  if (name === 'slot') {
    return 4;
  }
  if (name.startsWith('.')) {
    return 6;
  }
  if (name.startsWith('@')) {
    return 7;
  }
  return 5;
}

function resolvePlaceholders(str, placeholderMap) {
  let result = str;
  // Sort keys by length descending to avoid prefix collisions (e.g.
  // placeholder-1 matching inside placeholder-10)
  const sortedEntries = Array.from(placeholderMap.entries())
                            .sort((a, b) => b[0].length - a[0].length);

  for (const [placeholder, jsCode] of sortedEntries) {
    const code = jsCode.code;
    if (placeholder.startsWith(EXPR_PREFIX) ||
        placeholder.startsWith(PROP_PREFIX)) {
      result = result.replaceAll(placeholder, code);
    } else if (placeholder.startsWith('/')) {
      result = result.replaceAll(`</${placeholder.substring(1)}>`, code);
    } else {
      result = result.replaceAll(`<${placeholder}>`, code);
    }
  }
  return result;
}

function sortResolvedAttributes(resolvedAttrs) {
  resolvedAttrs.sort((a, b) => {
    const nameA = a.trim().split('=')[0];
    const nameB = b.trim().split('=')[0];

    const orderA = getOrder(nameA);
    const orderB = getOrder(nameB);
    if (orderA !== orderB) {
      return orderA - orderB;
    }

    // Example: .someProp -> someProp, ?disabled -> disabled
    const cleanA = nameA.replace(PREFIX_REGEX, '');
    const cleanB = nameB.replace(PREFIX_REGEX, '');

    if (cleanA === cleanB) {
      return 0;
    }
    return cleanA < cleanB ? -1 : 1;
  });
}

/**
 * Analyzes the child nodes of an element to determine:
 * 1. Whether it contains any non-text/comment child elements (hasChildElement).
 * 2. The length of any inline content (text plus closing tag) that will appear
 *    on the same line as the opening tag (inlineChildLength).
 *
 * For single-line elements with text children (e.g. `<span
 * class="...">*</span>`), the text and closing tag remain inline on the opening
 * tag's line. In this case, `inlineChildLength` accounts for both the text
 * content and closing tag length so attribute wrapping can respect the
 * 80-character limit.
 *
 * For elements whose text content spans multiple lines, or elements with child
 * elements, the children and closing tag will be placed on separate lines, so
 * no trailing child content is attached to the opening tag's line (length is
 * 0).
 *
 * @param {Object} node The AST node whose children to analyze.
 * @param {string} tagName The tag name of the element.
 * @param {Map<string, Object>} placeholderMap Map of placeholders to code.
 * @return {{hasChildElement: boolean, inlineChildLength: number}}
 */
function analyzeChildNodes(node, tagName, placeholderMap) {
  let firstLineLength = 0;
  let hasChildElement = false;
  let hasNonWhitespaceText = false;
  let hasNewline = false;
  for (const child of node.childNodes) {
    if (child.nodeName !== '#text' && child.nodeName !== '#comment') {
      hasChildElement = true;
      break;
    }

    if (hasNewline) {
      continue;
    }

    const content = child.nodeName === '#text' ?
        resolvePlaceholders(child.value, placeholderMap) :
        `<!--${child.data}-->`;
    if (content.trim() !== '') {
      hasNonWhitespaceText = true;
    }
    const newlineIdx = content.indexOf('\n');
    hasNewline = newlineIdx !== -1;
    firstLineLength += hasNewline ? newlineIdx : content.length;
  }

  // If the element has non-whitespace text on a single line with no child
  // elements, the text and closing tag stay on the same line as the opening
  // tag. Account for both the text length and the closing tag length.
  let inlineChildLength = 0;
  if (!hasChildElement && hasNonWhitespaceText && !hasNewline &&
      !VOID_ELEMENTS.includes(tagName)) {
    const closingPlaceholder = '/' + tagName;
    const endTagLen = placeholderMap.get(closingPlaceholder)?.code.length ??
        (tagName.length + 3);
    inlineChildLength = firstLineLength + endTagLen;
  }

  return {hasChildElement, inlineChildLength};
}

/**
 * Resolves placeholders in an attribute and wraps lines to respect the
 * 80-character line limit.
 *
 * Because modifying whitespace within static attribute text could alter the
 * attribute's runtime value, we cannot insert line breaks inside static text
 * chunks. However, whitespace inside JavaScript template expressions (`${...}`)
 * has no effect at runtime. Therefore, the only safe points where line breaks
 * can be introduced (beyond any breaks already produced by clang-format within
 * the expression itself) are immediately after the opening '${' and
 * immediately before the closing '}'.
 *
 * @param {Object} attr The attribute AST object ({name, value}).
 * @param {string} attrIndentStr The indentation string for a wrapped attribute
 *     line (element indent + 4 spaces).
 * @param {Map<string, Object>} placeholderMap Map of placeholders to code.
 * @return {string} The resolved attribute string (with leading space).
 */
function formatAttributeWithPlaceholders(attr, attrIndentStr, placeholderMap) {
  const attrName = placeholderMap.has(attr.name) ?
      placeholderMap.get(attr.name).code :
      attr.name;
  const parts = attr.value.split(new RegExp(`(${EXPR_PREFIX}-\\d+)`, 'g'));
  // If there are no expression placeholders, or if the attribute is a single
  // pure expression or nested template (already handled by
  // formatTsExpressions), resolve directly.
  if (parts.length === 1 ||
      (parts.length === 3 && parts[0] === '' && parts[2] === '') ||
      parts.some((p, idx) => idx % 2 === 1 && placeholderMap.get(p)?.nested)) {
    return resolvePlaceholders(` ${attr.name}="${attr.value}"`, placeholderMap);
  }

  const exprIndentStr = attrIndentStr + ' '.repeat(WRAPPED_LINE_INDENT_SIZE);
  let result = ` ${attrName}="${parts[0]}`;
  let currentLineLen = parts[0].includes('\n') ?
      parts[0].split('\n').at(-1).length :
      attrIndentStr.length + attrName.length + 2 + parts[0].length;

  for (let i = 1; i < parts.length; i += 2) {
    const placeholder = parts[i];
    const nextStatic = parts[i + 1];
    const exprCode = placeholderMap.get(placeholder).code;
    assert.ok(exprCode.startsWith('${') && exprCode.endsWith('}'));
    const innerExpr = exprCode.substring(2, exprCode.length - 1);

    result += '${';
    currentLineLen += 2;

    // Account for closing '}' (1 char) and any static text that remains on the
    // same line after '}'. If `nextStatic` does not contain a newline, also
    // account for either the next '${' (2 chars) or closing '">' (2 chars).
    const staticLines = nextStatic.split('\n');
    const staticOnSameLine = staticLines[0];
    const staticHasNewline = staticLines.length > 1;
    const suffixAndClosingLen =
        1 + staticOnSameLine.length + (staticHasNewline ? 0 : 2);

    const exprLines = innerExpr.split('\n');
    const isMultilineExpr = exprLines.length > 1;
    const firstLineCheckLen = isMultilineExpr ?
        exprLines[0].length :
        innerExpr.length + suffixAndClosingLen;

    if (innerExpr.startsWith('\n')) {
      // Case 1: The expression was already wrapped across multiple lines by
      // formatTsExpressions (e.g. a long ternary or multiline call) and
      // already starts with a newline and indentation.
      result += innerExpr;
      currentLineLen = exprLines.at(-1).length;
    } else if (currentLineLen + firstLineCheckLen > LINE_LENGTH_LIMIT) {
      // Case 2: The expression fits on its own wrapped line, but exceeds 80
      // characters when combined with the preceding attribute name, static
      // prefix, or trailing static suffix. Break the line immediately after
      // '${' and indent the first line of the expression. For multiline
      // expressions, indent the first line by the attribute prefix length
      // (`attr="${`) so it starts at the exact column clang-format aligned
      // subsequent lines against.
      const firstLineIndent = isMultilineExpr ?
          attrIndentStr + ' '.repeat(attrName.length + 4) :
          exprIndentStr;
      result += `\n${firstLineIndent}${innerExpr}`;
      currentLineLen = isMultilineExpr ?
          exprLines.at(-1).length :
          firstLineIndent.length + innerExpr.length;
    } else {
      // Case 3: The expression (or its first line) fits on the current line
      // without needing a line break after '${'.
      result += innerExpr;
      currentLineLen = isMultilineExpr ? exprLines.at(-1).length :
                                         currentLineLen + innerExpr.length;
    }

    if (staticOnSameLine !== '' &&
        currentLineLen + suffixAndClosingLen > LINE_LENGTH_LIMIT) {
      // Even with the expression on its own line, the static suffix following
      // '}' would push the expression's last line past 80 characters. Move the
      // closing '}' and the static suffix onto a new line aligned with the
      // attribute indentation before continuing to the next expression.
      result += `\n${attrIndentStr}}${nextStatic}`;
      // Reset `currentLineLen` to the indentation of the new line so the
      // shared update below adds `1 + staticOnSameLine.length` onto the new
      // line's indent rather than the previous expression line's length.
      currentLineLen = attrIndentStr.length;
    } else {
      // The closing '}' and any following static text fit on the same line as
      // the end of the expression.
      result += `}${nextStatic}`;
    }
    // Advance `currentLineLen` past '}' and `nextStatic`. If `nextStatic`
    // itself contains newlines, the active line is now the last line of
    // `nextStatic`; otherwise add '}' (1 char) plus `staticOnSameLine`.
    currentLineLen = staticHasNewline ?
        staticLines.at(-1).length :
        currentLineLen + 1 + staticOnSameLine.length;
  }

  return result + '"';
}

/**
 * Wraps attributes if the full tag length exceeds 80 characters.
 * @param {string} tagName The name of the tag.
 * @param {Array<Object>} attrs The attributes of the element.
 * @param {number} depth The current nesting depth.
 * @param {Map<string, Object>} placeholderMap The placeholder map.
 * @param {number} inlineChildLength The length of any inline child text and
 *     closing tag that will be placed onto the same line as the opening tag.
 * @param {Object} sourceCodeLocation The source code location info from parse5.
 * @param {boolean} [sortAttributes] Whether to sort attributes.
 * @return {string} The formatted attributes joined with the tag.
 */
function formatAttributes(
    tagName, attrs, depth, placeholderMap, inlineChildLength,
    sourceCodeLocation, sortAttributes = false) {
  const elementIndentStr =
      ' '.repeat(getDepthForTagName(tagName, depth - 1) * INDENT_SIZE);
  const attrIndentStr = elementIndentStr + ' '.repeat(WRAPPED_LINE_INDENT_SIZE);
  const resolvedAttrs = attrs.map(attr => {
    // parse5 records a value of "" for boolean attributes. Don't inject
    // this into the formatted code.
    if (attr.value === '') {
      const location = sourceCodeLocation.attrs[attr.name];
      assert.ok(!!location);
      const isBoolean =
          (location.endOffset - location.startOffset) === attr.name.length;
      if (isBoolean) {
        return resolvePlaceholders(` ${attr.name}`, placeholderMap);
      }
    }
    return formatAttributeWithPlaceholders(attr, attrIndentStr, placeholderMap);
  });

  if (sortAttributes) {
    sortResolvedAttributes(resolvedAttrs);
  }

  const fullTag = `<${tagName}${resolvedAttrs.join('')}>`;

  // Handle tags that fit on one line first
  if (!fullTag.includes('\n') &&
      (elementIndentStr.length + fullTag.length + inlineChildLength) <=
          LINE_LENGTH_LIMIT) {
    return fullTag;
  }
  let currentLine = `<${tagName}`;
  const lines = [];

  const pushCurrentLine = () => {
    lines.push(
        lines.length === 0 ? currentLine : `${attrIndentStr}${currentLine}`);
  };

  for (let i = 0; i < resolvedAttrs.length; i++) {
    const attrText = resolvedAttrs[i];
    const attrLines = attrText.split('\n');
    const firstLine = attrLines[0];

    const currentIndent =
        lines.length === 0 ? elementIndentStr.length : attrIndentStr.length;
    const extraLen =
        (i === resolvedAttrs.length - 1) ? inlineChildLength + 1 : 0;
    const exceedsLimit = (currentIndent + currentLine.length +
                          firstLine.length + extraLen) > LINE_LENGTH_LIMIT;
    // For expressions that wrapped onto a newline after '${', if they
    // appear on lines 2..N, the attribute is already at the wrapped indent
    // level so we don't need a newline before the attribute name. On line 1,
    // force it to a new line to avoid an awkward +8 indent jump from the tag.
    const isWrappedExpr = attrLines[0].endsWith('${');
    const isMultiline = lines.length === 0 ?
        attrLines.length > 1 :
        attrLines.length > 1 && !isWrappedExpr;

    if (currentLine !== '' && (isMultiline || exceedsLimit)) {
      // The first line of the attribute doesn't fit, so push the current line
      pushCurrentLine();

      if (attrLines.length === 1) {
        // Single line attribute: Reset the in-progress line to the attribute
        // text
        currentLine = attrText.trim();
      } else {
        // Push the first line of the multi-line attribute as a new line.
        lines.push(`${attrIndentStr}${attrLines[0].trim()}`);
      }
    } else {
      // The first line of the attribute fits on the current line, so append it
      currentLine =
          currentLine === '' ? attrLines[0].trim() : currentLine + attrLines[0];
      if (attrLines.length > 1) {
        // For multi line attributes, commit the current line with the first
        // line of the attribute
        pushCurrentLine();
      }
    }
    // Push remaining lines of multiline attributes directly and reset current
    // line to empty so subsequent attributes start on a clean dedicated line.
    if (attrLines.length > 1) {
      for (let j = 1; j < attrLines.length; j++) {
        lines.push(attrLines[j].trimEnd());
      }
      currentLine = '';
    }
  }

  if (currentLine !== '') {
    pushCurrentLine();
  }

  return lines.join('\n') + '>';
}

export function serializeNode(
    node, depth, placeholderMap, sortAttributes, skipSibling = false) {
  if (node.nodeName === '#document-fragment') {
    // Increment depth for children of document fragment.
    return node.childNodes
        .map(c => serializeNode(c, depth + 1, placeholderMap, sortAttributes))
        .join('');
  }

  if (node.nodeName === '#text') {
    // Re-escape '<' and '>' which parse5 unescapes into raw characters when
    // parsing HTML text nodes.
    const text = node.value.replaceAll('<', '&lt;').replaceAll('>', '&gt;');
    return resolvePlaceholders(text, placeholderMap);
  }

  if (node.nodeName === '#comment') {
    return `<!--${node.data}-->`;
  }

  // Resolve substituted tags
  const tagName = Object.keys(RESTRICTED_TAGS)
                      .find(k => RESTRICTED_TAGS[k] === node.tagName) ||
      node.tagName;

  if (tagName === FORMAT_OFF_PREFIX) {
    const attr = node.attrs.find(a => a.name === 'id');
    assert.ok(
        attr && placeholderMap.has(attr.value),
        `${FORMAT_OFF_PREFIX} missing id or placeholder mapping`);
    return placeholderMap.get(attr.value).code;
  }

  const isTemplatePlaceholder = tagName.startsWith(TEMPLATE_PREFIX) ||
      tagName.startsWith(FALSE_TEMPLATE_PREFIX);
  if (isTemplatePlaceholder) {
    assert.ok(placeholderMap.has(tagName));
  }
  const isTemplateNode =
      isTemplatePlaceholder && placeholderMap.get(tagName).isTemplate;

  // Determine if the contents of this element may be whitespace sensitive.
  // Avoid adding extra newlines between the opening and closing tag if
  // the element contains only text and/or comment node children. To do
  // the best possible job respecting 80 chars in such cases, also get the
  // length of any inline child contents to account for when wrapping.
  const {hasChildElement, inlineChildLength} =
      analyzeChildNodes(node, tagName, placeholderMap);

  // Resolve opening tag placeholders if they exist
  let startTag = '';
  if (placeholderMap.has(tagName)) {
    startTag = placeholderMap.get(tagName).code;
  } else if (!node.attrs || node.attrs.length === 0) {
    startTag = `<${tagName}>`;
  } else {
    startTag = formatAttributes(
        tagName, node.attrs, depth, placeholderMap, inlineChildLength,
        node.sourceCodeLocation, sortAttributes);
  }

  if (VOID_ELEMENTS.includes(tagName)) {
    return startTag;
  }

  const nextDepth = getChildDepthForNode(node, depth);
  const childrenHtml = node.childNodes ?
      node.childNodes
          .map(c => serializeNode(c, nextDepth, placeholderMap, sortAttributes))
          .join('') :
      '';

  // Resolve closing tag placeholders if they exist
  const closingPlaceholder = '/' + tagName;
  const endTag = placeholderMap.has(closingPlaceholder) ?
      placeholderMap.get(closingPlaceholder).code :
      `</${tagName}>`;

  const elementIndent =
      depth > 0 ? getDepthForNode(node, depth - 1) * INDENT_SIZE : 0;
  let fullLength =
      elementIndent + startTag.length + childrenHtml.length + endTag.length;

  // For conditional expressions (true and false branches), account for the
  // attached sibling branch when computing full single-line length.
  if (!skipSibling) {
    if (node.falseBranch) {
      fullLength +=
          serializeNode(
              node.falseBranch, nextDepth, placeholderMap, sortAttributes, true)
              .length;
    } else if (node.trueBranch) {
      fullLength +=
          serializeNode(
              node.trueBranch, nextDepth, placeholderMap, sortAttributes, true)
              .length;
    }
  }

  // Avoid inserting a newline and child indentation after the opening tag if
  // the first child is a comment that suppresses leading whitespace (e.g.
  // `<div><!--`).
  const firstChildSuppressesWhitespace = !!node.childNodes &&
      node.childNodes.length > 0 &&
      node.childNodes[0].suppressLeadingWhitespace;

  const lastChildSuppressesWhitespace = !!node.childNodes &&
      node.childNodes.length > 0 &&
      node.childNodes.at(-1).suppressTrailingWhitespace;

  // Determine if children and closing tag should be placed on new lines (i.e.
  // if opening tag didn't fit on one line, or if the full element exceeds 80
  // characters).
  // For templates (e.g. html`...`), wrap non-empty template contents across
  // lines if the template exceeds 80 characters, but keep empty templates
  // (html``) on a single line. For regular HTML elements, only wrap if there
  // are child elements or if the element is empty.
  const tagIsMultiline = startTag.includes('\n');
  const exceedsLineLimit = fullLength > LINE_LENGTH_LIMIT;
  const shouldWrap = isTemplateNode ?
      childrenHtml.trim() !== '' && (tagIsMultiline || exceedsLineLimit) :
      (tagIsMultiline || exceedsLineLimit) &&
          (hasChildElement || childrenHtml.trim() === '');

  if (shouldWrap) {
    if (lastChildSuppressesWhitespace) {
      return `${startTag}${childrenHtml}${endTag}`;
    }

    const endTagIndent = getIndentationPrefix(elementIndent);

    // Since the full tag doesn't fit on one line, put children on new line
    // and indent if they're not already on one.
    if (!childrenHtml.startsWith('\n') && childrenHtml.trim() !== '') {
      if (firstChildSuppressesWhitespace) {
        return `${startTag}${childrenHtml.trimEnd()}${endTagIndent}${endTag}`;
      }
      const childIndentSize = nextDepth > 0 ? (nextDepth - 1) * INDENT_SIZE : 0;
      const childIndent = getIndentationPrefix(childIndentSize);
      return `${startTag}${childIndent}${childrenHtml.trim()}${endTagIndent}${
          endTag}`;
    }

    if (TRAILING_NEWLINE_REGEX.test(childrenHtml)) {
      return `${startTag}${
          childrenHtml.replace(TRAILING_NEWLINE_REGEX, endTagIndent)}${endTag}`;
    }
    return `${startTag}${childrenHtml.trimEnd()}${endTagIndent}${endTag}`;
  }

  return `${startTag}${childrenHtml}${endTag}`;
}

/**
 * Serializes the HTML AST back to a string, handling attribute wrapping and
 * sorting.
 * @param {Object} ast The HTML AST to serialize.
 * @param {Map<string, string>} placeholderMap Map of placeholders to original
 *     content.
 * @param {boolean} sortAttributes Whether to sort attributes.
 * @return {string} The formatted HTML string.
 */
export function serializeHtmlAst(
    ast, placeholderMap = new Map(), sortAttributes = false) {
  return serializeNode(ast, 0, placeholderMap, sortAttributes);
}
