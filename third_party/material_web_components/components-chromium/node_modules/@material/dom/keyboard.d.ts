/**
 * @license
 * Copyright 2020 Google Inc.
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
/**
 * KEY provides normalized string values for keys.
 */
export declare const KEY: {
    UNKNOWN: string;
    BACKSPACE: string;
    ENTER: string;
    SPACEBAR: string;
    PAGE_UP: string;
    PAGE_DOWN: string;
    END: string;
    HOME: string;
    ARROW_LEFT: string;
    ARROW_UP: string;
    ARROW_RIGHT: string;
    ARROW_DOWN: string;
    DELETE: string;
    ESCAPE: string;
    TAB: string;
};
/**
 * normalizeKey returns the normalized string for a navigational action.
 */
export declare function normalizeKey(evt: KeyboardEvent): string;
/**
 * isNavigationEvent returns whether the event is a navigation event
 */
export declare function isNavigationEvent(evt: KeyboardEvent): boolean;
