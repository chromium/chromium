/**
@license
Copyright (c) 2019 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at
http://polymer.github.io/LICENSE.txt The complete set of authors may be found at
http://polymer.github.io/AUTHORS.txt The complete set of contributors may be
found at http://polymer.github.io/CONTRIBUTORS.txt Code distributed by Google as
part of the polymer project is also subject to an additional IP rights grant
found at http://polymer.github.io/PATENTS.txt
*/
/**
 * Whether the current browser supports `adoptedStyleSheets`.
 */
export declare const supportsAdoptingStyleSheets: boolean;
export declare class CSSResult {
    _styleSheet?: CSSStyleSheet | null;
    readonly cssText: string;
    constructor(cssText: string, safeToken: symbol);
    readonly styleSheet: CSSStyleSheet | null;
    toString(): string;
}
/**
 * Wrap a value for interpolation in a [[`css`]] tagged template literal.
 *
 * This is unsafe because untrusted CSS text can be used to phone home
 * or exfiltrate data to an attacker controlled site. Take care to only use
 * this with trusted input.
 */
export declare const unsafeCSS: (value: unknown) => CSSResult;
/**
 * Template tag which which can be used with LitElement's [[LitElement.styles |
 * `styles`]] property to set element styles. For security reasons, only literal
 * string values may be used. To incorporate non-literal values [[`unsafeCSS`]]
 * may be used inside a template string part.
 */
export declare const css: (strings: TemplateStringsArray, ...values: (number | CSSResult)[]) => CSSResult;
//# sourceMappingURL=css-tag.d.ts.map
