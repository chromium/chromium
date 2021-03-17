// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {CSSRule}
 * @see http://dev.w3.org/csswg/css-animations/#interface-csskeyframerule
 */
function CSSKeyframeRule() {}

/** @type {string} */
CSSKeyframeRule.prototype.keyText;

/** @type {CSSStyleDeclaration} */
CSSKeyframeRule.prototype.style;


/**
 * @constructor
 * @extends {CSSRule}
 * @see http://dev.w3.org/csswg/css-animations/#interface-csskeyframesrule
 */
function CSSKeyframesRule() {}

/** @type {string} */
CSSKeyframesRule.prototype.name;

/** @type {!CSSRuleList} */
CSSKeyframesRule.prototype.cssRules;


/**
 * @type {number}
 * @see http://dev.w3.org/csswg/css-animations/#interface-cssrule
 */
CSSRule.KEYFRAMES_RULE = 7;


/**
 * @type {number}
 * @see http://dev.w3.org/csswg/css-animations/#interface-cssrule
 */
CSSRule.KEYFRAME_RULE = 8;
