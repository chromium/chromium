// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SanitizeInnerHtmlOpts} from 'chrome://resources/js/parse_html_subset.m.js';

export {I18nBehavior};

interface I18nBehavior {
  locale: string|null|undefined;
  i18nUpdateLocale(): void;
  i18n(id: string, ...var_args: Array<string|number>): string;
  i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts|null): string;
  i18nDynamic(locale: string, id: string, ...var_args: string[]): string;
  i18nRecursive(locale: string, id: string, ...var_args: string[]): string;
  i18nExists(id: string): boolean;
}

declare const I18nBehavior: object;
