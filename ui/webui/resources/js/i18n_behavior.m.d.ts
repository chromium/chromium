// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SanitizeInnerHtmlOpts} from './parse_html_subset.m.js';

export interface I18nBehaviorInterface {
  locale: string|null|undefined;
  i18nUpdateLocale(): void;
  i18n(id: string, ...var_args: Array<string|number>): string;
  i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts): string;
  i18nDynamic(locale: string, id: string, ...var_args: string[]): string;
  i18nRecursive(locale: string, id: string, ...var_args: string[]): string;
  i18nExists(id: string): boolean;
}

export {I18nBehavior};

interface I18nBehavior extends I18nBehaviorInterface {}

declare const I18nBehavior: object;
