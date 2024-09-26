// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {HistoryEmbeddingsResultImageElement} from './result_image.ts';

export function getHtml(this: HistoryEmbeddingsResultImageElement) {
  return html`
    <svg aria-hidden="true"
        xmlns="http://www.w3.org/2000/svg"
        width="42" height="42" viewBox="0 0 42 42">
      <path id="illustrationPath"
          d="M15.1906 0.931046C6.16922 -2.98707 -2.98707 6.16923 0.931046 15.1906L1.57886 16.6822C2.77508 19.4364 2.77508 22.5636 1.57885 25.3178L0.931044 26.8094C-2.98707 35.8308 6.16923 44.9871 15.1906 41.069L16.6822 40.4211C19.4364 39.2249 22.5636 39.2249 25.3178 40.4211L26.8094 41.069C35.8308 44.9871 44.9871 35.8308 41.0689 26.8094L40.4211 25.3178C39.2249 22.5636 39.2249 19.4364 40.4211 16.6822L41.069 15.1906C44.9871 6.16922 35.8308 -2.98706 26.8094 0.931049L25.3178 1.57886C22.5635 2.77508 19.4364 2.77508 16.6822 1.57886L15.1906 0.931046Z">
      </path>
    </svg>
  `;
}
