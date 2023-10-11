/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import { SafeHtml } from '../../internals/html_impl';
/**
 * write safely calls {@link Document.write} on the given {@link Document} with
 * the given {@link SafeHtml}.
 */
export declare function write(doc: Document, text: SafeHtml): void;
declare type ValueType<Cmd extends string> = Lowercase<Cmd> extends 'inserthtml' ? SafeHtml : SafeHtml | string;
/**
 * Safely calls {@link Document.execCommand}. When command is insertHtml, a
 * SafeHtml must be passed in as value.
 */
export declare function execCommand<Cmd extends string>(doc: Document, command: Cmd, value?: ValueType<Cmd>): boolean;
/**
 * Safely calls {@link Document.execCommand}('insertHtml').
 * @deprecated Use safeDocument.execCommand.
 */
export declare function execCommandInsertHtml(doc: Document, html: SafeHtml): boolean;
export {};
