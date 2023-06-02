/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { TrustedResourceUrl } from '../../internals/resource_url_impl';
import { SafeScript } from '../../internals/script_impl';
/** Sets textContent from the given SafeScript. */
export declare function setTextContent(script: HTMLScriptElement, v: SafeScript): void;
/** Sets the Src attribute using a TrustedResourceUrl */
export declare function setSrc(script: HTMLScriptElement, v: TrustedResourceUrl): void;
