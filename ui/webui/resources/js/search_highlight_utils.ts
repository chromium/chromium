// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from './assert.js';

const WRAPPER_CSS_CLASS: string = 'search-highlight-wrapper';

const ORIGINAL_CONTENT_CSS_CLASS: string = 'search-highlight-original-content';

const HIT_CSS_CLASS: string = 'search-highlight-hit';

const SEARCH_BUBBLE_CSS_CLASS: string = 'search-bubble';

export interface Range {
  start: number;
  length: number;
}

/**
 * Replaces the the highlight wrappers given in |wrappers| with the original
 * search nodes.
 */
export function removeHighlights(wrappers: HTMLElement[]) {
  for (const wrapper of wrappers) {
    // If wrapper is already removed, do nothing.
    if (!wrapper.parentElement) {
      continue;
    }

    const originalContent =
        wrapper.querySelector(`.${ORIGINAL_CONTENT_CSS_CLASS}`);
    assert(originalContent);
    const textNode = originalContent.firstChild;
    assert(textNode);
    wrapper.parentElement.replaceChild(textNode, wrapper);
  }
}

/**
 * Finds all previous highlighted nodes under |node| and replaces the
 * highlights (yellow rectangles) with the original search node. Searches only
 * within the same shadowRoot and assumes that only one highlight wrapper
 * exists under |node|.
 */
export function findAndRemoveHighlights(node: Node) {
  const wrappers =
      Array.from((node as HTMLElement)
                     .querySelectorAll<HTMLElement>(`.${WRAPPER_CSS_CLASS}`));
  assert(wrappers.length === 1);
  removeHighlights(wrappers);
}

/**
 * Applies the highlight UI (yellow rectangle) around all matches in |node|.
 * @param node The text node to be highlighted. |node| ends up
 *     being hidden.
 * @return The new highlight wrapper.
 */
export function highlight(node: Node, ranges: Range[]): HTMLElement {
  assert(ranges.length > 0);

  const wrapper = document.createElement('span');
  wrapper.classList.add(WRAPPER_CSS_CLASS);
  // Use existing node as placeholder to determine where to insert the
  // replacement content.
  assert(node.parentNode);
  node.parentNode.replaceChild(wrapper, node);

  // Keep the existing node around for when the highlights are removed. The
  // existing text node might be involved in data-binding and therefore should
  // not be discarded.
  const span = document.createElement('span');
  span.classList.add(ORIGINAL_CONTENT_CSS_CLASS);
  span.style.display = 'none';
  span.appendChild(node);
  wrapper.appendChild(span);

  const text = node.textContent!;
  const tokens: string[] = [];
  for (let i = 0; i < ranges.length; ++i) {
    const range = ranges[i]!;
    const prev = ranges[i - 1]! || {start: 0, length: 0};
    const start = prev.start + prev.length;
    const length = range.start - start;
    tokens.push(text.substr(start, length));
    tokens.push(text.substr(range.start, range.length));
  }
  const last = ranges.slice(-1)[0]!;
  tokens.push(text.substr(last.start + last.length));

  for (let i = 0; i < tokens.length; ++i) {
    if (i % 2 === 0) {
      wrapper.appendChild(document.createTextNode(tokens[i]!));
    } else {
      const hitSpan = document.createElement('span');
      hitSpan.classList.add(HIT_CSS_CLASS);
      // Defaults to the color associated with --paper-yellow-500.
      hitSpan.style.backgroundColor =
          'var(--search-highlight-hit-background-color, #ffeb3b)';
      // Defaults to the color associated with --google-grey-900.
      hitSpan.style.color = 'var(--search-highlight-hit-color, #202124)';
      hitSpan.textContent = tokens[i]!;
      wrapper.appendChild(hitSpan);
    }
  }
  return wrapper;
}

/**
 * Creates an empty search bubble (styled HTML element without text).
 * |node| should already be visible or the bubble will render incorrectly.
 * @param node The node to be highlighted.
 * @param horizontallyCenter Whether or not to horizontally center
 *     the shown search bubble (if any) based on |node|'s left and width.
 * @return The search bubble that was added, or null if no new
 *     bubble was added.
 */
export function createEmptySearchBubble(
    node: Node, horizontallyCenter?: boolean): HTMLElement {
  let anchor = node;
  if (node.nodeName === 'SELECT') {
    anchor = node.parentNode!;
  }
  if (anchor instanceof ShadowRoot) {
    anchor = anchor.host.parentNode!;
  }

  let searchBubble =
      (anchor as HTMLElement)
          .querySelector<HTMLElement>(`.${SEARCH_BUBBLE_CSS_CLASS}`);
  // If the node has already been highlighted, there is no need to do
  // anything.
  if (searchBubble) {
    return searchBubble;
  }

  searchBubble = document.createElement('div');
  searchBubble.classList.add(SEARCH_BUBBLE_CSS_CLASS);
  const innards = document.createElement('div');
  innards.classList.add('search-bubble-innards');
  innards.textContent = '\u00a0';  // Non-breaking space for offsetHeight.
  searchBubble.appendChild(innards);
  anchor.appendChild(searchBubble);

  const updatePosition = function() {
    const nodeEl = node as HTMLElement;
    assert(searchBubble);
    assert(typeof nodeEl.offsetTop === 'number');
    searchBubble.style.top = nodeEl.offsetTop +
        (innards.classList.contains('above') ? -searchBubble.offsetHeight :
                                               nodeEl.offsetHeight) +
        'px';
    if (horizontallyCenter) {
      const width = nodeEl.offsetWidth - searchBubble.offsetWidth;
      searchBubble.style.left = nodeEl.offsetLeft + width / 2 + 'px';
    }
  };
  updatePosition();

  searchBubble.addEventListener('mouseover', function() {
    innards.classList.toggle('above');
    updatePosition();
  });
  // TODO(crbug.com/41096577): create a way to programmatically update these
  // bubbles (i.e. call updatePosition()) when outer scope knows they need to
  // be repositioned.
  return searchBubble;
}

export function stripDiacritics(text: string): string {
  return text.normalize('NFD').replace(/[\u0300-\u036f]/g, '');
}
