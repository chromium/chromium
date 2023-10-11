/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
import '../environment/dev';
import { createAttributePrefix } from '../internals/attribute_impl';
import { assertIsTemplateObject } from '../internals/string_literal';
import { SECURITY_SENSITIVE_ATTRIBUTES } from './sensitive_attributes';
/**
 * Creates a SafeAttributePrefix object from a template literal with no
 * interpolations for attributes that share a common prefix guaranteed to be not
 * security sensitive.
 *
 * The template literal is a prefix that makes it obvious this attribute is not
 * security sensitive. If it doesn't, this function will throw.
 */
export function safeAttrPrefix(templ) {
    if (process.env.NODE_ENV !== 'production') {
        assertIsTemplateObject(templ, true, 'safeAttr is a template literal tag function ' +
            'and should be called using the tagged template syntax. ' +
            'For example, safeAttr`foo`;');
    }
    const attrPrefix = templ[0].toLowerCase();
    if (process.env.NODE_ENV !== 'production') {
        if (attrPrefix.indexOf('on') === 0 || 'on'.indexOf(attrPrefix) === 0) {
            throw new Error(`Prefix '${templ[0]}' does not guarantee the attribute ` +
                `to be safe as it is also a prefix for event handler attributes` +
                `Please use 'addEventListener' to set event handlers.`);
        }
        SECURITY_SENSITIVE_ATTRIBUTES.forEach(sensitiveAttr => {
            if (sensitiveAttr.indexOf(attrPrefix) === 0) {
                throw new Error(`Prefix '${templ[0]}' does not guarantee the attribute ` +
                    `to be safe as it is also a prefix for ` +
                    `the security sensitive attribute '${sensitiveAttr}'. ` +
                    `Please use native or safe DOM APIs to set the attribute.`);
            }
        });
    }
    return createAttributePrefix(attrPrefix);
}
