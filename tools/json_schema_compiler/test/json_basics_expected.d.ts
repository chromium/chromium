// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.fakeJson API
 * Generated from: file.json
 * run `tools/json_schema_compiler/compiler.py file.json -g ts_definitions` to
 * regenerate.
 */



declare namespace chrome {
  export namespace fakeJson {

    export const lastError: string;

    export enum CrazyEnum {
      CAMEL_CASE_ENUM = 'camelCaseEnum',
      NON_CHARACTERS = 'Non-Characters',
      _5NUM_FIRST = '5NumFirst',
      _3JUST_PLAIN_OLD_MEAN = '3Just-plainOld_MEAN',
    }

    export interface CrazyObject {}

    export type ArraySimple = string[];

    export type ArrayOfInlineObject = Array<{
      name: string,
      value?: string,
      binaryValue?: number[],
    }>;

    export type TwoChoices = ArrayBuffer|string;

    export function funcWithInlineObj(
        inlineObj: {
          foo?: boolean,
             bar: number,
             baz: {
               depth: number,
             },
             quu: ArrayBuffer,
        },
        callback: (returnObj: {
          str: string,
        }) => void): {
      str: string,
      int: number,
    };

    export function funcWithReturnsAsync(someNumber: number): Promise<number>;

  }
}
