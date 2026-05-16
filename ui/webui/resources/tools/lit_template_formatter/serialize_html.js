// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {EXPR_PREFIX, FORMAT_OFF_PREFIX, getIndentationPrefix, INDENT_SIZE, LINE_LENGTH_LIMIT, PROP_PREFIX, RESTRICTED_TAGS, VOID_ELEMENTS, WRAPPED_LINE_INDENT_SIZE} from './html_utils.js';

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

function analyzeChildNodes(node, placeholderMap) {
  let firstLineLength = 0;
  let hasChildElement = false;
  let newline = -1;
  for (const child of node.childNodes) {
    if (child.nodeName !== '#text' && child.nodeName !== '#comment') {
      hasChildElement = true;
      break;
    }
    if (newline !== -1) {
      continue;
    }

    const content = child.nodeName === '#text' ?
        resolvePlaceholders(child.value, placeholderMap) :
        `<!--${child.data}-->`;
    newline = content.indexOf('\n');
    firstLineLength += newline === -1 ? content.length : newline;
  }
  return {hasChildElement, firstLineLength};
}

/**
 * Wraps attributes if the full tag length exceeds 80 characters.
 * @param {string} tagName The name of the tag.
 * @param {Array<Object>} attrs The attributes of the element.
 * @param {number} depth The current nesting depth.
 * @param {Map<string, Object>} placeholderMap The placeholder map.
 * @param {number} childLen The length of the child content that needs to be
 *     placed onto the last line with the closing tag.
 * @param {Object} sourceCodeLocation The source code location info from parse5.
 * @param {boolean} [sortAttributes] Whether to sort attributes.
 * @return {string} The formatted attributes joined with the tag.
 */
function formatAttributes(
    tagName, attrs, depth, placeholderMap, childLen, sourceCodeLocation,
    sortAttributes = false) {
  const elementIndentStr = ' '.repeat((depth - 1) * INDENT_SIZE);

  const resolvedAttrs = attrs.map(attr => {
    let rawText = ` ${attr.name}="${attr.value}"`;
    // parse5 records a value of "" for boolean attributes. Don't inject
    // this into the formatted code.
    if (attr.value === '') {
      const location = sourceCodeLocation.attrs[attr.name];
      assert.ok(!!location);
      const isBoolean =
          (location.endOffset - location.startOffset) === attr.name.length;
      if (isBoolean) {
        rawText = ` ${attr.name}`;
      }
    }
    return resolvePlaceholders(rawText, placeholderMap);
  });

  if (sortAttributes) {
    sortResolvedAttributes(resolvedAttrs);
  }

  const fullTag = `<${tagName}${resolvedAttrs.join('')}>`;

  // Handle tags that fit on one line first
  if ((elementIndentStr.length + fullTag.length + childLen) <=
      LINE_LENGTH_LIMIT) {
    return fullTag;
  }

  const attrIndentStr = elementIndentStr + ' '.repeat(WRAPPED_LINE_INDENT_SIZE);
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
    const extraLen = (i === resolvedAttrs.length - 1) ? childLen + 1 : 0;

    if ((currentIndent + currentLine.length + firstLine.length + extraLen) >
        LINE_LENGTH_LIMIT) {
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
      currentLine += attrLines[0];
      if (attrLines.length > 1) {
        // For multi line attributes, commit the current line with the first
        // line of the attribute
        pushCurrentLine();
      }
    }
    // Push intermediate lines and set the current line to the last line of the
    // current attribute. Apply relative indentation to intermediate lines of
    // multiline attributes. Assume lines are indented as desired relative to
    // the last line.
    if (attrLines.length > 1) {
      const lastLine = attrLines[attrLines.length - 1];
      const leadingWhitespace = lastLine.match(/^\s*/)[0].length;
      for (let j = 1; j < attrLines.length - 1; j++) {
        const line = attrLines[j];
        const lineLeading = line.match(/^\s*/)[0].length;
        const stripAmount = Math.min(leadingWhitespace, lineLeading);
        const strippedLine = line.substring(stripAmount);
        lines.push(`${attrIndentStr}${strippedLine}`);
      }
      currentLine = lastLine.trim();
    }
  }
  pushCurrentLine();

  return lines.join('\n') + '>';
}

export function serializeNode(node, depth, placeholderMap, sortAttributes) {
  if (node.nodeName === '#document-fragment') {
    // Increment depth for children of document fragment.
    return node.childNodes
        .map(c => serializeNode(c, depth + 1, placeholderMap, sortAttributes))
        .join('');
  }

  if (node.nodeName === '#text') {
    return resolvePlaceholders(node.value, placeholderMap);
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

  // Determine if the contents of this element may be whitespace sensitive.
  // Avoid adding extra newlines between the opening and closing tag if
  // the element contains only text and/or comment node children. To do
  // the best possible job respecting 80 chars in such cases, also get the
  // length of the first line of child contents to account for when wrapping.
  const {hasChildElement, firstLineLength} =
      analyzeChildNodes(node, placeholderMap);

  // Resolve opening tag placeholders if they exist
  let startTag = '';
  if (placeholderMap.has(tagName)) {
    startTag = placeholderMap.get(tagName).code;
  } else if (!node.attrs || node.attrs.length === 0) {
    startTag = `<${tagName}>`;
  } else {
    startTag = formatAttributes(
        tagName, node.attrs, depth, placeholderMap, firstLineLength,
        node.sourceCodeLocation, sortAttributes);
  }

  if (VOID_ELEMENTS.includes(tagName)) {
    return startTag;
  }

  const childrenHtml = node.childNodes ?
      node.childNodes
          .map(c => serializeNode(c, depth + 1, placeholderMap, sortAttributes))
          .join('') :
      '';

  // Resolve closing tag placeholders if they exist
  const closingPlaceholder = '/' + tagName;
  const endTag = placeholderMap.has(closingPlaceholder) ?
      placeholderMap.get(closingPlaceholder).code :
      `</${tagName}>`;

  const elementIndent = depth > 0 ? (depth - 1) * INDENT_SIZE : 0;
  const fullLength =
      elementIndent + startTag.length + childrenHtml.length + endTag.length;

  // Closing tag on new line if opening tag didn't fit on one line, or if the
  // full element exceeds 80 characters.
  // Trim trailing whitespace and use the expected indent to avoid doubling
  // whitespace.
  if ((startTag.includes('\n') || fullLength > LINE_LENGTH_LIMIT) &&
      (hasChildElement || childrenHtml.trim() === '')) {
    const endTagIndent = getIndentationPrefix(elementIndent);

    // Since the full tag doesn't fit on one line, put children on new line
    // and indent if they're not already on one.
    if (!childrenHtml.startsWith('\n') && childrenHtml.trim() !== '') {
      const childIndent = getIndentationPrefix(elementIndent + INDENT_SIZE);
      return `${startTag}${childIndent}${childrenHtml.trim()}${endTagIndent}${
          endTag}`;
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
