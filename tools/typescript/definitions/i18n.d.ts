// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions for chrome.i18n API
 * Generated from: extensions/common/api/i18n.json
 * run `tools/json_schema_compiler/compiler.py extensions/common/api/i18n.json
 * -g definitions` to regenerate.
 */



declare namespace chrome {
  export namespace i18n {

    export type LanguageCode = string;

    export function getAcceptLanguages(): Promise<LanguageCode[]>;

    export function getMessage(
        messageName: string, substitutions?: any, options?: {
          escapeLt?: boolean,
        }): string;

    export function getUILanguage(): string;

    export function detectLanguage(text: string): Promise<{
      isReliable: boolean,
      languages: Array<{
        language: LanguageCode,
        percentage: number,
      }>,
    }>;

  }
}
