// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import type {FormattedString, FormattedStringFragment} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';
import {ColorType} from '//resources/mojo/components/omnibox/browser/suggest_template_info.mojom-webui.js';

/** A run of FormattedString text and the CSS classes that style it. */
interface FormattedStringSegment {
  text: string;
  classes: string[];
}

/**
 * Maps the color and weight of a FormattedString fragment to CSS classes.
 * Each suggest color gets a class named after its color type.
 * Other colors get no class and keep the default color.
 */
function getFragmentCssClasses(fragment: FormattedStringFragment): string[] {
  const classes = [];
  switch (fragment.color) {
    case ColorType.kSuggestQueryText:
      classes.push('suggest-query-text');
      break;
    case ColorType.kSuggestHintText:
      classes.push('suggest-hint-text');
      break;
    case ColorType.kSuggestActionHigh:
      classes.push('suggest-action-high');
      break;
    default:
      break;
  }
  if (fragment.isBolded) {
    classes.push('bolded');
  }
  return classes;
}

/**
 * Splits the text of `formattedString` into segments. Each fragment styles the
 * text from its `startIndex` up to the next fragment's `startIndex`, or to the
 * end of the text for the last one. Text before the first fragment, or all of
 * the text if there are no fragments, is unstyled.
 */
function getFormattedStringSegments(formattedString: FormattedString):
    FormattedStringSegment[] {
  const {text, fragments} = formattedString;
  if (fragments.length === 0) {
    return [{text, classes: []}];
  }

  const segments: FormattedStringSegment[] = [];
  const firstFragment = fragments[0]!;
  if (firstFragment.startIndex > 0) {
    segments.push(
        {text: text.substring(0, firstFragment.startIndex), classes: []});
  }
  fragments.forEach((fragment, index) => {
    const nextStartIndex = index + 1 < fragments.length ?
        fragments[index + 1]!.startIndex :
        text.length;
    segments.push({
      text: text.substring(fragment.startIndex, nextStartIndex),
      classes: getFragmentCssClasses(fragment),
    });
  });
  return segments;
}

/**
 * Returns the text of `formattedString` as sanitized HTML, with a <span> for
 * each segment that has the CSS classes of the segment.
 */
export function formattedStringToHtml(formattedString: FormattedString):
    TrustedHTML {
  const container = document.createElement('span');
  for (const {text, classes} of getFormattedStringSegments(formattedString)) {
    const span = document.createElement('span');
    if (classes.length) {
      span.classList.add(...classes);
    }
    span.textContent = text;
    container.appendChild(span);
  }
  try {
    return sanitizeInnerHtml(container.innerHTML, {attrs: ['class']});
  } catch (e) {
    // If sanitization fails return empty HTML.
    return window.trustedTypes!.emptyHTML;
  }
}
