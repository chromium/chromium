// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'I18nMixinLit' is a Mixin offering loading of internationalization
 * strings. Typically it is used as [[i18n('someString')]] computed bindings or
 * for this.i18n('foo'). It is not needed for HTML $i18n{otherString}, which is
 * handled by a C++ templatizer.
 */

import {loadTimeData} from '//resources/js/load_time_data.js';
import type {SanitizeInnerHtmlOpts} from '//resources/js/parse_html_subset.js';
import {parseHtmlSubset, sanitizeInnerHtml} from '//resources/js/parse_html_subset.js';
import type {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

type Constructor<T> = new (...args: any[]) => T;

export const I18nMixinLit = <T extends Constructor<CrLitElement>>(
    superClass: T): T&Constructor<I18nMixinLitInterface> => {
  class I18nMixinLit extends superClass implements I18nMixinLitInterface {
    /**
     * Returns a translated string where $1 to $9 are replaced by the given
     * values.
     * @param id The ID of the string to translate.
     * @param varArgs Values to replace the placeholders $1 to $9 in the
     *     string.
     * @return A translated, substituted string.
     */
    private i18nRaw_(id: string, ...varArgs: Array<string|number>) {
      return varArgs.length === 0 ? loadTimeData.getString(id) :
                                    loadTimeData.getStringF(id, ...varArgs);
    }

    /**
     * Returns a translated string where $1 to $9 are replaced by the given
     * values. Also sanitizes the output to filter out dangerous HTML/JS.
     * Use with Lit bindings that are *not* innerHTML.
     * NOTE: This is not related to $i18n{foo} in HTML, see file overview.
     * @param id The ID of the string to translate.
     * @param varArgs Values to replace the placeholders $1 to $9 in the
     *     string.
     * @return A translated, sanitized, substituted string.
     */
    i18n(id: string, ...varArgs: Array<string|number>) {
      const rawString = this.i18nRaw_(id, ...varArgs);
      return parseHtmlSubset(`<b>${rawString}</b>`).firstChild!.textContent!;
    }

    /**
     * Similar to 'i18n', returns a translated, sanitized, substituted
     * string. It receives the string ID and a dictionary containing the
     * substitutions as well as optional additional allowed tags and
     * attributes. Use with Lit bindings that are innerHTML.
     * @param id The ID of the string to translate.
     */
    i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts) {
      opts = opts || {};
      const rawString = this.i18nRaw_(id, ...(opts.substitutions || []));
      return sanitizeInnerHtml(rawString, opts);
    }

    /**
     * Similar to 'i18n', with an unused |locale| parameter used to trigger
     * updates when the locale changes.
     * @param locale The UI language used.
     * @param id The ID of the string to translate.
     * @param varArgs Values to replace the placeholders $1 to $9 in the
     *     string.
     * @return A translated, sanitized, substituted string.
     */
    i18nDynamic(_locale: string, id: string, ...varArgs: string[]) {
      return this.i18n(id, ...varArgs);
    }

    /**
     * Similar to 'i18nDynamic', but varArgs values are interpreted as keys
     * in loadTimeData. This allows generation of strings that take other
     * localized strings as parameters.
     * @param locale The UI language used.
     * @param id The ID of the string to translate.
     * @param varArgs Values to replace the placeholders $1 to $9
     *     in the string. Values are interpreted as strings IDs if found in
     * the list of localized strings.
     * @return A translated, sanitized, substituted string.
     */
    i18nRecursive(locale: string, id: string, ...varArgs: string[]) {
      let args = varArgs;
      if (args.length > 0) {
        // Try to replace IDs with localized values.
        args = args.map(str => {
          return this.i18nExists(str) ? loadTimeData.getString(str) : str;
        });
      }
      return this.i18nDynamic(locale, id, ...args);
    }

    /**
     * Returns true if a translation exists for |id|.
     */
    i18nExists(id: string) {
      return loadTimeData.valueExists(id);
    }
  }

  return I18nMixinLit;
};

export interface I18nMixinLitInterface {
  i18n(id: string, ...varArgs: Array<string|number>): string;
  i18nAdvanced(id: string, opts?: SanitizeInnerHtmlOpts): TrustedHTML;
  i18nDynamic(locale: string, id: string, ...varArgs: string[]): string;
  i18nRecursive(locale: string, id: string, ...varArgs: string[]): string;
  i18nExists(id: string): boolean;
}
