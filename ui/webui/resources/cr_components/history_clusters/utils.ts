// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {highlight} from '//resources/js/search_highlight_utils.js';

import type {MatchPosition} from './history_cluster_types.mojom-webui.js';

/**
 * Populates `container` with the highlighted `text` based on the mojom provided
 * `match_positions`. This function takes care of converting from the mojom
 * format to the format expected by search_highlight_utils.
 */
export function insertHighlightedTextWithMatchesIntoElement(
    container: HTMLElement, text: string, matches: MatchPosition[]) {
  container.textContent = '';
  const node = document.createTextNode(text);
  container.appendChild(node);

  const ranges = [];
  for (const match of matches) {
    ranges.push({
      start: match.begin,
      length: match.end - match.begin,
    });
  }

  if (ranges.length > 0) {
    highlight(node, ranges);
  }
}
