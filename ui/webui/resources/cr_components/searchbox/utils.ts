// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from '//resources/js/assert.js';
import type {String16} from '//resources/mojo/mojo/public/mojom/base/string16.mojom-webui.js';
import type {TimeTicks} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';

import {RenderType, SideType} from './searchbox.mojom-webui.js';

/** Converts a String16 to a JavaScript String. */
export function decodeString16(str: String16|null): string {
  return str ? str.data.map(ch => String.fromCodePoint(ch)).join('') : '';
}

/** Converts a JavaScript String to a String16. */
export function mojoString16(str: string): String16 {
  const array = new Array(str.length);
  for (let i = 0; i < str.length; ++i) {
    array[i] = str.charCodeAt(i);
  }
  return {data: array};
}

/**
 * Converts a time ticks in milliseconds to TimeTicks.
 * @param timeTicks time ticks in milliseconds
 */
export function mojoTimeTicks(timeTicks: number): TimeTicks {
  return {internalValue: BigInt(Math.floor(timeTicks * 1000))};
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
