/*
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

/**
 * Returns an array of distinct values. Order is not guaranteed.
 * @param values - The values to filter. Should be JSON-serializable.
 * @return - An array of distinct values.
 */
export function distinctValues<T>(values: T[]): T[] {
  const map = new Map<string, T>();
  for (const value of values) {
    map.set(deterministicJSONStringify(value), value);
  }
  return Array.from(map.values());
}

/**
 * Returns a stringified version of the object with keys sorted. This is required to
 * ensure that the stringified version of an object is deterministic independent of the
 * order of keys.
 * @param obj
 * @return {string}
 */
export function deterministicJSONStringify(obj: unknown) {
  return JSON.stringify(normalizeObject(obj));
}

function normalizeObject(obj: any) {
  if (
    obj === undefined ||
    obj === null ||
    Array.isArray(obj) ||
    typeof obj !== 'object'
  ) {
    return obj;
  }

  // Copy the original object key and values to a new object in sorted order.
  const newObj: any = {};
  for (const key of Object.keys(obj).sort()) {
    const value = obj[key];
    newObj[key] = normalizeObject(value); // Recursively sort nested objects
  }

  return newObj;
}
