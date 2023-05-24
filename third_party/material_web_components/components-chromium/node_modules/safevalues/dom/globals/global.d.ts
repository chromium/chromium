/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { SafeScript } from '../../internals/script_impl';
/**
 * Evaluates a SafeScript value in the given scope using eval.
 *
 * Strongly consider avoiding this, as eval blocks CSP adoption and does not
 * benefit from compiler optimizations.
 */
export declare function globalEval(win: Window | typeof globalThis, script: SafeScript): unknown;
