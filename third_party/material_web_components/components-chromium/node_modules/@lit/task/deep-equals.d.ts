/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: BSD-3-Clause
 */
export declare const deepArrayEquals: <T extends readonly unknown[]>(oldArgs: T, newArgs: T) => boolean;
/**
 * Recursively checks two objects for equality.
 *
 * This function handles the following cases:
 *  - Primitives: primitives compared with Object.is()
 *  - Objects: to be equal, two objects must:
 *    - have the same constructor
 *    - have same set of own property names
 *    - have each own property be deeply equal
 *  - Arrays, Maps, Sets, and RegExps
 *  - Objects with custom valueOf() (ex: Date)
 *  - Objects with custom toString() (ex: URL)
 *
 * Important: Objects must be free of cycles, otherwise this function will
 * run infinitely!
 */
export declare const deepEquals: (a: unknown, b: unknown) => boolean;
//# sourceMappingURL=deep-equals.d.ts.map