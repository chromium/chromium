/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview Safe iframe helpers and go/intents-for-iframes-for-closure
 */
import { SafeHtml } from '../../internals/html_impl';
import { TrustedResourceUrl } from '../../internals/resource_url_impl';
/** Sets the Src attribute using a TrustedResourceUrl */
export declare function setSrc(iframe: HTMLIFrameElement, v: TrustedResourceUrl): void;
/** Sets the Srcdoc attribute using a SafeHtml */
export declare function setSrcdoc(iframe: HTMLIFrameElement, v: SafeHtml): void;
