// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from './assert.js';

export interface SanitizeInnerHtmlOpts {
  substitutions?: string[];
  attrs?: string[];
  tags?: string[];
}

/**
 * Make a string safe for Polymer bindings that are inner-h-t-m-l or other
 * innerHTML use.
 * @param rawString The unsanitized string
 * @param opts Optional additional allowed tags and attributes.
 */
function sanitizeInnerHtmlInternal(
    rawString: string, opts?: SanitizeInnerHtmlOpts): string {
  opts = opts || {};
  const html = parseHtmlSubset(`<b>${rawString}</b>`, opts.tags, opts.attrs)
                   .firstElementChild!;
  return html.innerHTML;
}

// <if expr="not is_ios">
let sanitizedPolicy: TrustedTypePolicy|null = null;

/**
 * Same as |sanitizeInnerHtmlInternal|, but it passes through sanitizedPolicy
 * to create a TrustedHTML.
 */
export function sanitizeInnerHtml(
    rawString: string, opts?: SanitizeInnerHtmlOpts): TrustedHTML {
  assert(window.trustedTypes);
  if (sanitizedPolicy === null) {
    // Initialize |sanitizedPolicy| lazily.
    sanitizedPolicy = window.trustedTypes.createPolicy('sanitize-inner-html', {
      createHTML: sanitizeInnerHtmlInternal,
      createScript: () => assertNotReached(),
      createScriptURL: () => assertNotReached(),
    });
  }
  return sanitizedPolicy.createHTML(rawString, opts);
}
// </if>

// <if expr="is_ios">
/**
 * Delegates to sanitizeInnerHtmlInternal() since on iOS there is no
 * window.trustedTypes support yet.
 */
export function sanitizeInnerHtml(
    rawString: string, opts?: SanitizeInnerHtmlOpts): string {
  assert(!window.trustedTypes);
  return sanitizeInnerHtmlInternal(rawString, opts);
}
// </if>

type AllowFunction = (node: Node, value: string) => boolean;

const allowAttribute: AllowFunction = (_node, _value) => true;

/** Allow-list of attributes in parseHtmlSubset. */
const allowedAttributes: Map<string, AllowFunction> = new Map([
  [
    'href',
    (node, value) => {
      // Only allow a[href] starting with chrome:// or https:// or equaling
      // to #.
      return (node as HTMLElement).tagName === 'A' &&
          (value.startsWith('chrome://') || value.startsWith('https://') ||
           value === '#');
    },
  ],
  [
    'target',
    (node, value) => {
      // Only allow a[target='_blank'].
      // TODO(dbeam): are there valid use cases for target !== '_blank'?
      return (node as HTMLElement).tagName === 'A' && value === '_blank';
    },
  ],
]);

/** Allow-list of optional attributes in parseHtmlSubset. */
const allowedOptionalAttributes: Map<string, AllowFunction> = new Map([
  ['class', allowAttribute],
  ['id', allowAttribute],
  ['is', (_node, value) => value === 'action-link' || value === ''],
  ['role', (_node, value) => value === 'link'],
  [
    'src',
    (node, value) => {
      // Only allow img[src] starting with chrome://
      return (node as HTMLElement).tagName === 'IMG' &&
          value.startsWith('chrome://');
    },
  ],
  ['tabindex', allowAttribute],
  ['aria-description', allowAttribute],
  ['aria-hidden', allowAttribute],
  ['aria-label', allowAttribute],
  ['aria-labelledby', allowAttribute],
]);

/** Allow-list of tag names in parseHtmlSubset. */
const allowedTags: Set<string> = new Set(
    ['A', 'B', 'I', 'BR', 'DIV', 'EM', 'KBD', 'P', 'PRE', 'SPAN', 'STRONG']);

/** Allow-list of optional tag names in parseHtmlSubset. */
const allowedOptionalTags: Set<string> = new Set(['IMG', 'LI', 'UL']);

/**
 * This policy maps a given string to a `TrustedHTML` object
 * without performing any validation. Callsites must ensure
 * that the resulting object will only be used in inert
 * documents. Initialized lazily.
 */
let unsanitizedPolicy: TrustedTypePolicy;

/**
 * @param optTags an Array to merge.
 * @return Set of allowed tags.
 */
function mergeTags(optTags: string[]): Set<string> {
  const clone = new Set(allowedTags);
  optTags.forEach(str => {
    const tag = str.toUpperCase();
    if (allowedOptionalTags.has(tag)) {
      clone.add(tag);
    }
  });
  return clone;
}

/**
 * @param optAttrs an Array to merge.
 * @return Map of allowed attributes.
 */
function mergeAttrs(optAttrs: string[]): Map<string, AllowFunction> {
  const clone = new Map(allowedAttributes);
  optAttrs.forEach(key => {
    if (allowedOptionalAttributes.has(key)) {
      clone.set(key, allowedOptionalAttributes.get(key)!);
    }
  });
  return clone;
}

function walk(n: Node, f: (p: Node) => void) {
  f(n);
  for (let i = 0; i < n.childNodes.length; i++) {
    walk(n.childNodes[i]!, f);
  }
}

function assertElement(tags: Set<string>, node: Node) {
  if (!tags.has((node as HTMLElement).tagName)) {
    throw Error((node as HTMLElement).tagName + ' is not supported');
  }
}

function assertAttribute(
    attrs: Map<string, AllowFunction>, attrNode: Attr, node: Node) {
  const n = attrNode.nodeName;
  const v = attrNode.nodeValue || '';
  if (!attrs.has(n) || !attrs.get(n)!(node, v)) {
    throw Error(
        (node as HTMLElement).tagName + '[' + n + '="' + v +
        '"] is not supported');
  }
}

/**
 * Parses a very small subset of HTML. This ensures that insecure HTML /
 * javascript cannot be injected into WebUI.
 * @param s The string to parse.
 * @param extraTags Optional extra allowed tags.
 * @param extraAttrs
 *     Optional extra allowed attributes (all tags are run through these).
 * @throws an Error in case of non supported markup.
 * @return A document fragment containing the DOM tree.
 */
export function parseHtmlSubset(
    s: string, extraTags?: string[], extraAttrs?: string[]): DocumentFragment {
  const tags = extraTags ? mergeTags(extraTags) : allowedTags;
  const attrs = extraAttrs ? mergeAttrs(extraAttrs) : allowedAttributes;

  const doc = document.implementation.createHTMLDocument('');
  const r = doc.createRange();
  r.selectNode(doc.body);

  if (window.trustedTypes) {
    if (!unsanitizedPolicy) {
      unsanitizedPolicy =
          window.trustedTypes.createPolicy('parse-html-subset', {
            createHTML: (untrustedHTML: string) => untrustedHTML,
            createScript: () => assertNotReached(),
            createScriptURL: () => assertNotReached(),
          });
    }
    s = unsanitizedPolicy.createHTML(s) as unknown as string;
  }

  // This does not execute any scripts because the document has no view.
  const df = r.createContextualFragment(s);
  walk(df, function(node) {
    switch (node.nodeType) {
      case Node.ELEMENT_NODE:
        assertElement(tags, node);
        const nodeAttrs = (node as HTMLElement).attributes;
        for (let i = 0; i < nodeAttrs.length; ++i) {
          assertAttribute(attrs, nodeAttrs[i]!, node);
        }
        break;

      case Node.COMMENT_NODE:
      case Node.DOCUMENT_FRAGMENT_NODE:
      case Node.TEXT_NODE:
        break;

      default:
        throw Error('Node type ' + node.nodeType + ' is not supported');
    }
  });
  return df;
}
