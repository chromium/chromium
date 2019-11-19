// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Interface definition for a class which evaluates speech rules.
 *
 * A speech rule evaluator knows how to generate a description given a node and
 * a speech rule.
 */

goog.provide('cvox.SpeechRuleEvaluator');

goog.require('cvox.SpeechRule');


/**
 * @interface
 */
cvox.SpeechRuleEvaluator = goog.abstractMethod;


/**
 * Default evaluation of a node if no speech rule is applicable.
 * @param {!Node} node The target node (or root of subtree).
 * @return {!Array<cvox.NavDescription>} The resulting description.
 */
cvox.SpeechRuleEvaluator.prototype.evaluateDefault = goog.abstractMethod;
