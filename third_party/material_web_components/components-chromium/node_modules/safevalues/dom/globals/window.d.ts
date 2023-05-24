/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { Url } from '../../builders/url_sanitizer';
/**
 * open calls {@link Window.open} on the given {@link Window}, given a
 * target {@link Url}.
 */
export declare function open(win: Window, url: Url, target?: string, features?: string): Window | null;
