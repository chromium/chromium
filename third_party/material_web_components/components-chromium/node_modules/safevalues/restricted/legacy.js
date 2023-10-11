"use strict";
/**
 * @license
 * SPDX-License-Identifier: Apache-2.0
 */
Object.defineProperty(exports, "__esModule", { value: true });
exports.legacyUnsafeStyleSheet = exports.legacyUnsafeStyle = exports.legacyUnsafeResourceUrl = exports.legacyUnsafeScript = exports.legacyUnsafeHtml = void 0;
require("../environment/dev");
var html_impl_1 = require("../internals/html_impl");
var resource_url_impl_1 = require("../internals/resource_url_impl");
var script_impl_1 = require("../internals/script_impl");
var style_impl_1 = require("../internals/style_impl");
var style_sheet_impl_1 = require("../internals/style_sheet_impl");
/*
 * Transitional utilities to unsafely trust random strings as
 * safe values. Intended for temporary use when upgrading a library that
 * used to accept plain strings to use safe values, but where it's not
 * practical to transitively update callers.
 *
 * IMPORTANT: No new code should use the conversion functions in this file,
 * they are intended for refactoring old code to use safe values. New code
 * should construct safe values via their APIs, template systems or
 * sanitizers. If that’s not possible it should use a reviewed conversion and
 * undergo security review.
 *
 * The semantics of the legacy conversions are very
 * different from the ones provided by reviewed conversions. The
 * latter are for use in code where it has been established through manual
 * security review that the value produced by a piece of code will always
 * satisfy the SafeHtml contract (e.g., the output of a secure HTML sanitizer).
 * In uses of legacy conversions, this guarantee is not given -- the
 * value in question originates in unreviewed legacy code and there is no
 * guarantee that it satisfies the SafeHtml contract.
 *
 * There are only three valid uses of legacy conversions:
 *
 * 1. Introducing a safe values version of a function which currently consumes
 * string and passes that string to a DOM API which can execute script - and
 * hence cause XSS - like innerHTML. For example, Dialog might expose a
 * setContent method which takes a string and sets the innerHTML property of
 * an element with it. In this case a setSafeHtmlContent function could be
 * added, consuming SafeHtml instead of string. setContent could then internally
 *  use legacyUnsafeHtml to create a SafeHtml
 * from string and pass the SafeHtml to a safe values consumer down the line. In
 * this scenario, remember to document the use of legacyUnsafeHtml in the
 * modified setContent and consider deprecating it as well.
 *
 * 2. Automated refactoring of application code which handles HTML as string
 * but needs to call a function which only takes safe values types. For example,
 * in the Dialog scenario from (1) an alternative option would be to refactor
 * setContent to accept SafeHtml instead of string and then refactor
 * all current callers to use legacy conversions to pass SafeHtml. This is
 * generally preferable to (1) because it keeps the library clean of
 * legacy conversions, and makes code sites in application code that are
 * potentially vulnerable to XSS more apparent.
 *
 * 3. Old code which needs to call APIs which consume safe values types and for
 * which it is prohibitively expensive to refactor to use these types.
 * Generally, this is code where safety from XSS is either hopeless or
 * unimportant.
 */
/**
 * Turns a string into SafeHtml for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
function legacyUnsafeHtml(s) {
    if (process.env.NODE_ENV !== 'production' && typeof s !== 'string') {
        throw new Error('Expected a string');
    }
    return (0, html_impl_1.createHtml)(s);
}
exports.legacyUnsafeHtml = legacyUnsafeHtml;
/**
 * Turns a string into SafeScript for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
function legacyUnsafeScript(s) {
    if (process.env.NODE_ENV !== 'production' && typeof s !== 'string') {
        throw new Error('Expected a string');
    }
    return (0, script_impl_1.createScript)(s);
}
exports.legacyUnsafeScript = legacyUnsafeScript;
/**
 * Turns a string into TrustedResourceUrl for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
function legacyUnsafeResourceUrl(s) {
    if (process.env.NODE_ENV !== 'production' && typeof s !== 'string') {
        throw new Error('Expected a string');
    }
    return (0, resource_url_impl_1.createResourceUrl)(s);
}
exports.legacyUnsafeResourceUrl = legacyUnsafeResourceUrl;
/**
 * Turns a string into SafeStyle for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
function legacyUnsafeStyle(s) {
    if (process.env.NODE_ENV !== 'production' && typeof s !== 'string') {
        throw new Error('Expected a string');
    }
    return (0, style_impl_1.createStyle)(s);
}
exports.legacyUnsafeStyle = legacyUnsafeStyle;
/**
 * Turns a string into SafeStyleSheet for legacy API purposes.
 *
 * Please read fileoverview documentation before using.
 */
function legacyUnsafeStyleSheet(s) {
    if (process.env.NODE_ENV !== 'production' && typeof s !== 'string') {
        throw new Error('Expected a string');
    }
    return (0, style_sheet_impl_1.createStyleSheet)(s);
}
exports.legacyUnsafeStyleSheet = legacyUnsafeStyleSheet;
