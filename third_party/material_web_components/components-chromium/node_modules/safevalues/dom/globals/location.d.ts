/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { Url } from '../../builders/url_sanitizer';
/**
 * setHref safely sets {@link Location.href} on the given {@link Location} with
 * given {@link Url}.
 */
export declare function setHref(loc: Location, url: Url): void;
/**
 * replace safely calls {@link Location.replace} on the given {@link Location}
 * with given {@link Url}.
 */
export declare function replace(loc: Location, url: Url): void;
/**
 * assign safely calls {@link Location.assign} on the given {@link Location}
 * with given {@link Url}.
 */
export declare function assign(loc: Location, url: Url): void;
