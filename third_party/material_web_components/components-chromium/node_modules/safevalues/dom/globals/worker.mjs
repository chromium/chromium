/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { unwrapResourceUrl } from '../../internals/resource_url_impl';
/**
 * Safely creates a Web Worker.
 *
 * Example usage:
 *   const trustedResourceUrl = trustedResourceUrl`/safe_script.js`;
 *   safedom.safeWorker.create(trustedResourceUrl);
 * which is a safe alternative to
 *   new Worker(url);
 * The latter can result in loading untrusted code.
 */
export function create(url, options) {
    return new Worker(unwrapResourceUrl(url), options);
}
/** Safely creates a shared Web Worker. */
export function createShared(url, options) {
    return new SharedWorker(unwrapResourceUrl(url), options);
}
/** Safely calls importScripts */
export function importScripts(scope, ...urls) {
    scope.importScripts(...urls.map(url => unwrapResourceUrl(url)));
}
