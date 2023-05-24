/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { TrustedResourceUrl } from '../../internals/resource_url_impl';
/**
 * ScopeWithImportScripts is an {@link WindowOrWorkerGlobalScope} that also
 * has {@link WorkerGlobalScope.importScripts} as {@link WorkerGlobalScope} in
 * some cases cannot be depended on directly.
 */
export interface ScopeWithImportScripts extends WindowOrWorkerGlobalScope {
    importScripts: (...url: string[]) => void;
}
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
export declare function create(url: TrustedResourceUrl, options?: {}): Worker;
/** Safely creates a shared Web Worker. */
export declare function createShared(url: TrustedResourceUrl, options?: string | WorkerOptions): SharedWorker;
/** Safely calls importScripts */
export declare function importScripts(scope: ScopeWithImportScripts, ...urls: TrustedResourceUrl[]): void;
