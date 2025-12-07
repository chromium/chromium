// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Verify |value| is truthy.
 * @param value A value to check for truthiness. Note that this
 *     may be used to test whether |value| is defined or not, and we don't want
 *     to force a cast to boolean.
 */
export function assert<T>(value: T, message?: string): asserts value {
  if (value) {
    return;
  }

  throw new Error('Assertion failed' + (message ? `: ${message}` : ''));
}

export function assertInstanceof<T>(
    value: unknown, type: {new (...args: any): T},
    message?: string): asserts value is T {
  if (value instanceof type) {
    return;
  }

  throw new Error(
      message || `Value ${value} is not of type ${type.name || typeof type}`);
}

/**
 * Call this from places in the code that should never be reached.
 *
 * For example, handling all the values of enum with a switch() like this:
 *
 *   function getValueFromEnum(enum) {
 *     switch (enum) {
 *       case ENUM_FIRST_OF_TWO:
 *         return first
 *       case ENUM_LAST_OF_TWO:
 *         return last;
 *     }
 *     assertNotReached();
 *   }
 *
 * This code should only be hit in the case of serious programmer error or
 * unexpected input.
 */
export function assertNotReached(message: string = 'Unreachable code hit'):
    never {
  assert(false, message);
}

/**
 * Statically and dynamically assert that a code should not be reached.
 *
 * For example, handling all the values of enum with a switch() like this:
 *
 *   function getValueFromEnum(value: SomeEnum): number {
 *     switch (value) {
 *       case ENUM_FIRST_OF_TWO:
 *         return 1;
 *       case ENUM_LAST_OF_TWO:
 *         return 2;
 *       default:
 *         assertNotReachedCase(value);
 *     }
 *   }
 *
 * Helper function that should be preferred over assertNotReached in switch/case
 * statements referring to enums, because it results in a build time error if the
 * 'case' statements are not exhaustive. At runtime it behaves identically to
 * assertNotReached.
 */
export function assertNotReachedCase(_param: never, message?: string): never {
  assertNotReached(message);
}
