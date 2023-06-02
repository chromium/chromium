/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
import { SafeAttributePrefix } from '../internals/attribute_impl';
/**
 * Creates a SafeAttributePrefix object from a template literal with no
 * interpolations for attributes that share a common prefix guaranteed to be not
 * security sensitive.
 *
 * The template literal is a prefix that makes it obvious this attribute is not
 * security sensitive. If it doesn't, this function will throw.
 */
export declare function safeAttrPrefix(templ: TemplateStringsArray): SafeAttributePrefix;
