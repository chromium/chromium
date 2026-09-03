// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import assert from 'node:assert';

import {parseFragment} from '../../../../../third_party/node/node_modules/parse5/lib/index.js';

import {EXPR_PREFIX, FALSE_TEMPLATE_PREFIX, FORMAT_OFF_PREFIX, getChildDepthForNode, getDepthForNode, getIndentationPrefix, INDENT_SIZE, PROP_PREFIX, RESTRICTED_TAGS, TEMPLATE_PREFIX, TRAILING_NEWLINE_REGEX, VOID_ELEMENTS} from './html_utils.js';

/**
 * Adds the correct newline and indentation before a child node.
 * @param {Array<Object>} children The array of children nodes to push to or
 *     modify.
 * @param {number} depth The current nesting depth.
 */
function ensureNewlineAndIndent(children, depth) {
  const prevChild = children.at(-1) || null;
  const endsWithNewline = prevChild && prevChild.nodeName === '#text' &&
      TRAILING_NEWLINE_REGEX.test(prevChild.value);

  if (endsWithNewline) {
    prevChild.value = prevChild.value.replace(
        TRAILING_NEWLINE_REGEX, getIndentationPrefix(depth * INDENT_SIZE));
  } else {
    children.push({
      nodeName: '#text',
      value: getIndentationPrefix(depth * INDENT_SIZE),
    });
  }
}

/**
 * Checks if a new line should be inserted before a child node.
 * Returns true if following a non-text node or a text node that is already just
 * whitespace.
 * @param {Object} node The parent node.
 * @param {Object} child The current child node being processed.
 * @return {boolean}
 */
function shouldInsertNewline(node, child) {
  const idx = node.childNodes.indexOf(child);
  assert.ok(idx !== -1);
  const prevNode = idx > 0 ? node.childNodes[idx - 1] : null;
  return prevNode && prevNode.nodeName === '#text' &&
      (prevNode.value.trim() === '' || /\s$/.test(prevNode.value));
}

/**
 * Records indentation and attribute metadata for placeholders.
 * @param {Map<string, Object>} placeholderMap The map of placeholders to
 *     update.
 * @param {string} placeholder The placeholder ID.
 * @param {number} depth The current nesting depth.
 * @param {string} [attrName] The name of the attribute if this placeholder is
 *     an attribute value.
 */
function recordMetadata(
    placeholderMap, placeholder, depth, attrName = undefined) {
  assert.ok(
      placeholderMap.has(placeholder),
      `Placeholder ${placeholder} not found in map`);
  const metadata = placeholderMap.get(placeholder);
  const updates = {indent: depth * INDENT_SIZE};
  if (attrName) {
    updates.attrName = attrName;
  }
  placeholderMap.set(placeholder, {
    ...metadata,
    ...updates,
  });
}

/**
 * Annotates a comment node with flags indicating whether it suppresses
 * whitespace before or after it by being attached directly to surrounding
 * characters without intervening whitespace.
 * @param {Object} child The comment AST node.
 * @param {string} rawHtml The raw HTML string before AST parsing.
 */
function processComment(child, rawHtml) {
  assert.ok(
      child.sourceCodeLocation, 'Comment node missing sourceCodeLocation');
  const location = child.sourceCodeLocation;
  const charBefore =
      location.startOffset > 0 ? rawHtml[location.startOffset - 1] : '';
  const charAfter =
      location.endOffset < rawHtml.length ? rawHtml[location.endOffset] : '';
  child.suppressLeadingWhitespace = charBefore !== '' && !/\s/.test(charBefore);
  child.suppressTrailingWhitespace = charAfter !== '' && !/\s/.test(charAfter);
}

/**
 * Walks the AST tree from `node` and applies indentation and newlines
 * as needed. Updates `placeholderMap` with metadata.
 * @param {Object} node The AST node to format.
 * @param {number} depth The current nesting depth.
 * @param {Map<string, Object>} placeholderMap Map of placeholders to update
 *     in-place.
 * @param {string} [rawHtml] Raw HTML string before preprocessing.
 */
function format(node, depth, placeholderMap, rawHtml = '') {
  assert.ok(placeholderMap, 'placeholderMap must be provided to format');
  if (!node.childNodes) {
    return;
  }

  const nonEmptyChildren = node.childNodes.filter(
      child => child.nodeName !== '#text' || child.value.trim() !== '' ||
          (child.value.match(/\n/g) || []).length > 1);

  // Track if this is the first element child of the parent, to ensure it
  // always gets a newline and indentation.
  let isFirstElement = true;
  const newChildren = [];
  for (const child of nonEmptyChildren) {
    const prevChild = newChildren.at(-1) || null;

    // Handle text/comment nodes (non-tags).
    if (child.nodeName.startsWith('#')) {
      if (child.nodeName === '#comment') {
        processComment(child, rawHtml);

        // If the comment does not suppress leading whitespace, insert a newline
        // and indent it.
        if (!child.suppressLeadingWhitespace &&
            shouldInsertNewline(node, child)) {
          ensureNewlineAndIndent(newChildren, depth);
        }
        if (child.suppressLeadingWhitespace) {
          isFirstElement = false;
        }
      }

      if (child.nodeName === '#text') {
        // Normalize any blank lines to remove trailing whitespace.
        child.value = child.value.replace(/\n[ \t]+(?=\n)/g, '\n');

        if (child.value.trim() === '') {
          newChildren.push(child);
          continue;
        }
        // Check for placeholders to record metadata.
        const matches =
            child.value.match(new RegExp(`${EXPR_PREFIX}-\\d+`, 'g'));
        if (matches) {
          for (const match of matches) {
            recordMetadata(placeholderMap, match, depth);
          }
        }
        // If the child node has leading newlines and whitespace, replace them
        // with the appropriate blank lines and indentation whitespace.
        const leadingWhitespaceMatch = child.value.match(/^(\n[ \t]*)+/);
        if (leadingWhitespaceMatch) {
          const newlineCount =
              (leadingWhitespaceMatch[0].match(/\n/g) || []).length;
          const indentStr = ' '.repeat(depth * INDENT_SIZE);
          child.value = child.value.replace(
              /^(\n[ \t]*)+/, '\n'.repeat(newlineCount) + indentStr);
        }
        // If the child node has trailing whitespace, initially set it to the
        // parent's indentation (for the closing tag). If it has a subsequent
        // sibling, this will be overwritten later by ensureNewlineAndIndent.
        if (TRAILING_NEWLINE_REGEX.test(child.value)) {
          const trailingIndent =
              depth > 0 ? getDepthForNode(node, depth - 1) * INDENT_SIZE : 0;
          child.value = child.value.replace(
              TRAILING_NEWLINE_REGEX, getIndentationPrefix(trailingIndent));
        }
      }

      newChildren.push(child);
      continue;
    }

    // Must be a tag.
    const tagName = child.tagName;

    // Record indent for tag-style placeholders, which represent conditionals
    // or map statements. There should only ever be one match for a tag name.
    const tagMatch = tagName.match(new RegExp(
        `(?:${EXPR_PREFIX}|${FALSE_TEMPLATE_PREFIX}|${TEMPLATE_PREFIX})-\\d+`));
    if (tagMatch) {
      recordMetadata(placeholderMap, tagMatch[0], depth);
    }

    // Record metadata for attribute values.
    if (child.attrs) {
      for (const attr of child.attrs) {
        const valueMatches =
            attr.value.match(new RegExp(`${EXPR_PREFIX}-\\d+`, 'g'));
        if (!valueMatches) {
          continue;
        }

        // Get the original attribute name to record in the metadata for
        // the value's placeholder.
        for (const match of valueMatches) {
          const attrName = placeholderMap.has(attr.name) ?
              placeholderMap.get(attr.name).code :
              attr.name;
          recordMetadata(
              placeholderMap, match, getDepthForNode(child, depth), attrName);
        }
      }
    }

    // Link true and false branch placeholders for conditional expressions.
    if (prevChild?.tagName?.startsWith(TEMPLATE_PREFIX) &&
        tagName?.startsWith(FALSE_TEMPLATE_PREFIX)) {
      prevChild.falseBranch = child;
      child.trueBranch = prevChild;
    }

    // Skip newline if it's a false branch placeholder. Also skip if the tag is
    // directly preceded by a comment that suppresses trailing whitespace
    // (e.g. `--><span...`).
    if (!tagName || !tagName.startsWith(FALSE_TEMPLATE_PREFIX)) {
      if (!prevChild?.suppressTrailingWhitespace &&
          (isFirstElement || shouldInsertNewline(node, child))) {
        ensureNewlineAndIndent(newChildren, getDepthForNode(child, depth));
      }
      isFirstElement = false;
    }

    const nextDepth = getChildDepthForNode(child, depth);
    format(child, nextDepth, placeholderMap, rawHtml);
    newChildren.push(child);
  }

  // Add a newline and indentation before the closing tag if this element has
  // at least one child element (not just text) and is not the root document
  // fragment.
  if (node.nodeName !== '#document-fragment' &&
      newChildren.some(c => !c.nodeName.startsWith('#'))) {
    const closeIndent = getDepthForNode(node, depth - 1) * INDENT_SIZE;
    const prevChild = newChildren.at(-1) || null;
    if (prevChild && prevChild.nodeName === '#text' &&
        TRAILING_NEWLINE_REGEX.test(prevChild.value)) {
      prevChild.value = prevChild.value.replace(
          TRAILING_NEWLINE_REGEX, getIndentationPrefix(closeIndent));
    } else if (!prevChild?.suppressTrailingWhitespace) {
      newChildren.push({
        nodeName: '#text',
        value: getIndentationPrefix(closeIndent),
      });
    }
  }

  node.childNodes = newChildren;
}

/**
 * Preprocesses `html` to make it valid for parse5. Updates `placeholderMap`
 * with substitutions made.
 * @param {string} html The HTML content to preprocess.
 * @param {Map<string, Object>} placeholderMap Map of placeholders to original
 *     content.
 * @return {string} HTML string with substitutions, safe to parse with parse5.
 */
function preprocessHtml(html, placeholderMap) {
  let substitutedHtml = html;

  // Handle lit-template-format-off/on
  const offOnRegex =
      /<!--\s*lit-template-format-off\s*-->([\s\S]*?)<!--\s*lit-template-format-on\s*-->/g;
  substitutedHtml = substitutedHtml.replace(offOnRegex, (match) => {
    const placeholder = `${FORMAT_OFF_PREFIX}-${placeholderMap.size}`;
    placeholderMap.set(placeholder, {code: match});
    return `<${FORMAT_OFF_PREFIX} id="${placeholder}"></${FORMAT_OFF_PREFIX}>`;
  });

  // Replace restricted tags so that non-compliant children will not be
  // stripped by parse5.
  for (const [original, replacement] of Object.entries(RESTRICTED_TAGS)) {
    const openRegex = new RegExp(`<${original}(\\s|>)`, 'g');
    substitutedHtml = substitutedHtml.replace(openRegex, `<${replacement}$1`);

    const closeRegex = new RegExp(`</${original}>`, 'g');
    substitutedHtml = substitutedHtml.replace(closeRegex, `</${replacement}>`);
  }

  // Replace Lit property bindings, which are camelCase and would otherwise
  // be incorrectly converted to lowercase by parse5.
  return substitutedHtml.replace(/\s(\.[a-zA-Z0-9]+)=/g, (match, p1) => {
    const placeholder = `${PROP_PREFIX}-${placeholderMap.size}`;
    placeholderMap.set(placeholder, {code: p1});
    return ` ${placeholder}=`;
  });
}

/**
 * Prepares the HTML AST by substituting tags, parsing, and formatting
 * indentation.
 * @param {string} html The HTML content to process.
 * @param {Map<string, Object>} placeholderMap Map of placeholders to original
 *     content. Will be updated in-place with indentation and attribute
 *     metadata.
 * @param {number} [depth] Initial depth.
 * @return {Object} The formatted AST from parse5.
 */
export function prepareHtmlAst(html, placeholderMap = new Map(), depth = 0) {
  const substitutedHtml = preprocessHtml(html, placeholderMap);
  const ast = parseFragment(substitutedHtml, {sourceCodeLocationInfo: true});
  format(ast, depth, placeholderMap, substitutedHtml);
  return ast;
}
