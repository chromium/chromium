/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { Url } from '../../builders/url_sanitizer';
import { TrustedResourceUrl } from '../../internals/resource_url_impl';
declare const SAFE_URL_REL_VALUES: readonly ["alternate", "author", "bookmark", "canonical", "cite", "help", "icon", "license", "next", "prefetch", "dns-prefetch", "prerender", "preconnect", "preload", "prev", "search", "subresource"];
/**
 * Values of the "rel" attribute when "href" should accept `SafeUrl` instead of
 * `TrustedResourceUrl`.
 */
export declare type SafeUrlRelTypes = typeof SAFE_URL_REL_VALUES[number];
/**
 * Values of the "rel" attribute when "href" should accept a
 * `TrustedResourceUrl`. Note that this list is not exhaustive and is here just
 * for better documentation, any unknown "rel" values will also require passing
 * a `TrustedResourceUrl` "href".
 */
export declare type TrustedResourecUrlRelTypes = 'stylesheet' | 'manifest';
/**
 * Safely sets a link element's "href" property using a sensitive "rel" value.
 */
export declare function setHrefAndRel(link: HTMLLinkElement, url: TrustedResourceUrl, rel: TrustedResourecUrlRelTypes): void;
/**
 * Safely sets a link element's "href" property using a non-sensitive "rel"
 * value.
 */
export declare function setHrefAndRel(link: HTMLLinkElement, url: Url, rel: SafeUrlRelTypes): void;
/**
 * Safely sets a link element's "href" property using an arbitrary "rel"
 * value.
 */
export declare function setHrefAndRel(link: HTMLLinkElement, url: TrustedResourceUrl, rel: string): void;
export {};
