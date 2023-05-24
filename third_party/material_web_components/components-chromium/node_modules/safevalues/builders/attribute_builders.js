"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.safeAttrPrefix = void 0;
require("../environment/dev");
var attribute_impl_1 = require("../internals/attribute_impl");
var string_literal_1 = require("../internals/string_literal");
var sensitive_attributes_1 = require("./sensitive_attributes");
/**
 * Creates a SafeAttributePrefix object from a template literal with no
 * interpolations for attributes that share a common prefix guaranteed to be not
 * security sensitive.
 *
 * The template literal is a prefix that makes it obvious this attribute is not
 * security sensitive. If it doesn't, this function will throw.
 */
function safeAttrPrefix(templ) {
    if (process.env.NODE_ENV !== 'production') {
        (0, string_literal_1.assertIsTemplateObject)(templ, true, 'safeAttr is a template literal tag function ' +
            'and should be called using the tagged template syntax. ' +
            'For example, safeAttr`foo`;');
    }
    var attrPrefix = templ[0].toLowerCase();
    if (process.env.NODE_ENV !== 'production') {
        if (attrPrefix.indexOf('on') === 0 || 'on'.indexOf(attrPrefix) === 0) {
            throw new Error("Prefix '".concat(templ[0], "' does not guarantee the attribute ") +
                "to be safe as it is also a prefix for event handler attributes" +
                "Please use 'addEventListener' to set event handlers.");
        }
        sensitive_attributes_1.SECURITY_SENSITIVE_ATTRIBUTES.forEach(function (sensitiveAttr) {
            if (sensitiveAttr.indexOf(attrPrefix) === 0) {
                throw new Error("Prefix '".concat(templ[0], "' does not guarantee the attribute ") +
                    "to be safe as it is also a prefix for " +
                    "the security sensitive attribute '".concat(sensitiveAttr, "'. ") +
                    "Please use native or safe DOM APIs to set the attribute.");
            }
        });
    }
    return (0, attribute_impl_1.createAttributePrefix)(attrPrefix);
}
exports.safeAttrPrefix = safeAttrPrefix;
