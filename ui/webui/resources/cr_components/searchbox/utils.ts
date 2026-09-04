// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import {RenderType, SideType} from '//resources/mojo/components/omnibox/browser/searchbox.mojom-webui.js';
import type {TimeTicks} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

/**
 * Converts a time ticks in milliseconds to TimeTicks.
 * @param timeTicks time ticks in milliseconds
 */
export function mojoTimeTicks(timeTicks: number): TimeTicks {
  return {internalValue: BigInt(Math.floor(timeTicks * 1000))};
}

/** Returns promise that resolves when lazy rendering should be started. */
export function waitForLazyRender(): Promise<void> {
  return new Promise<void>(resolve => {
    requestIdleCallback(() => resolve(), {timeout: 500});
  });
}

/** Converts a side type to a string to be used in CSS. */
export function sideTypeToClass(sideType: SideType): string {
  switch (sideType) {
    case SideType.kDefaultPrimary:
      return 'primary-side';
    case SideType.kSecondary:
      return 'secondary-side';
    default:
      assertNotReached('Unexpected side type');
  }
}

/** Converts a render type to a string to be used in CSS. */
export function renderTypeToClass(renderType: RenderType): string {
  switch (renderType) {
    case RenderType.kDefaultVertical:
      return 'vertical';
    case RenderType.kHorizontal:
      return 'horizontal';
    case RenderType.kGrid:
      return 'grid';
    default:
      assertNotReached('Unexpected render type');
  }
}

/**
 * Records a performance mark using the Web Performance API if it has not
 * already been recorded for the current document.
 */
export function markOnce(name: string) {
  if (!performance.getEntriesByName(name).length) {
    performance.mark(name);
  }
}

// LINT.IfChange(StripJavascriptSchemas)
export function stripJavascriptSchemas(text: string): string {
  const kJsPrefix = 'javascript:';
  let foundJavaScript = false;
  let i = 0;
  // Find the index of the first character that isn't whitespace, a control
  // character, or a part of a JavaScript: scheme.
  while (i < text.length) {
    if (/\s/.test(text.charAt(i)) || text.charCodeAt(i) < 0x20) {
      i++;
    } else {
      if (text.substring(i, i + kJsPrefix.length).toLowerCase() !== kJsPrefix) {
        break;
      }

      // We've found a JavaScript scheme. Continue searching to ensure that
      // strings like "javascript:javascript:alert()" are fully stripped.
      foundJavaScript = true;
      i += kJsPrefix.length;
    }
  }

  // If we found any "JavaScript:" schemes in the text, return the text starting
  // at the first non-whitespace/control character after the last instance of
  // the scheme.
  if (foundJavaScript) {
    return text.substring(i);
  }

  return text;
}
// LINT.ThenChange(//components/omnibox/browser/omnibox_text_util.cc:StripJavascriptSchemas)

// LINT.IfChange(SanitizeTextForPaste)
export function sanitizeTextForPaste(text: string): string {
  if (!text) {
    // Nothing to do.
    return '';
  }

  if (/^\s+$/.test(text)) {
    // Convert all-whitespace to single space.
    return ' ';
  }

  let output = '';
  let end = 0;
  while (end < text.length && /\s/.test(text.charAt(end))) {
    end++;
  }
  // Because `end` points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Copy all non-whitespace sequences.
  // Do not copy trailing whitespace.
  // Copy all other whitespace sequences that do not contain CR/LF.
  // Convert all other whitespace sequences that do contain CR/LF to either ' '
  // or nothing, depending on whether there are any other sequences that do not
  // contain CR/LF.
  let outputNeedsLfConversion = false;
  let seenNonLfWhitespace = false;
  while (true) {
    // Copy this non-whitespace sequence.
    let begin = end;
    while (end < text.length && !/\s/.test(text.charAt(end))) {
      end++;
    }
    output += text.substring(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end < text.length) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      while (end < text.length && /\s/.test(text.charAt(end)) &&
             text.charAt(end) !== '\n' && text.charAt(end) !== '\r') {
        end++;
      }
      if (end < text.length && text.charAt(end) !== '\n' &&
          text.charAt(end) !== '\r') {
        // Found a non-trailing whitespace sequence without CR/LF. Copy it.
        seenNonLfWhitespace = true;
        output += text.substring(begin, end);
        continue;
      }
    }

    // `end` either points at the end of the string or a CR/LF.
    while (end < text.length && /\s/.test(text.charAt(end))) {
      end++;
    }
    if (end === text.length) {
      // Ignore any trailing whitespace.
      break;
    }

    // The preceding whitespace sequence contained CR/LF. Convert to a single
    // LF that we'll fix up below the loop.
    outputNeedsLfConversion = true;
    output += '\n';
  }

  // Convert LFs to ' ' or '' depending on whether there were non-LF whitespace
  // sequences.
  if (outputNeedsLfConversion) {
    const replacement = seenNonLfWhitespace ? ' ' : '';
    output = output.replace(/\n/g, replacement);
  }

  return stripJavascriptSchemas(output);
}
// LINT.ThenChange(//components/omnibox/browser/omnibox_text_util.cc:SanitizeTextForPaste)
