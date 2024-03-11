/**
 * Copyright 2024 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

const INTERESTING_LABELS = new Set(['chromium-bidi-2023']);

export const LABELS_TO_TESTS = new Map();

const response = await fetch(
  'https://wpt.fyi/api/metadata?includeTestLevel=true&product=chrome'
);
const data = await response.json();

for (const [path, elements] of Object.entries(data)) {
  for (const element of elements) {
    const label = element.label;
    if (INTERESTING_LABELS.has(label)) {
      if (LABELS_TO_TESTS.has(label)) {
        LABELS_TO_TESTS.get(label).add(path);
      } else {
        LABELS_TO_TESTS.set(label, new Set([path]));
      }
    }
  }
}
