/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { TrustedResourceUrl } from '../../internals/resource_url_impl';
/** Safely registers a service worker by URL */
export declare function register(container: ServiceWorkerContainer, scriptURL: TrustedResourceUrl, options?: RegistrationOptions): Promise<ServiceWorkerRegistration>;
