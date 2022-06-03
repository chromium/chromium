/**
 * @license
 * Copyright 2019 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
export declare type StandardCssPropertyName = 'animation' | 'transform' | 'transition';
export declare type PrefixedCssPropertyName = '-webkit-animation' | '-webkit-transform' | '-webkit-transition';
export declare type StandardJsEventType = 'animationend' | 'animationiteration' | 'animationstart' | 'transitionend';
export declare type PrefixedJsEventType = 'webkitAnimationEnd' | 'webkitAnimationIteration' | 'webkitAnimationStart' | 'webkitTransitionEnd';
export interface CssVendorProperty {
    prefixed: PrefixedCssPropertyName;
    standard: StandardCssPropertyName;
}
export interface JsVendorProperty {
    cssProperty: StandardCssPropertyName;
    prefixed: PrefixedJsEventType;
    standard: StandardJsEventType;
}
export declare type CssVendorPropertyMap = {
    [K in StandardCssPropertyName]: CssVendorProperty;
};
export declare type JsVendorPropertyMap = {
    [K in StandardJsEventType]: JsVendorProperty;
};
