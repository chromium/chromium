// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Speech rules for mathml and mathjax nodes.
 */

goog.provide('cvox.MathmlStoreRules');

goog.require('cvox.MathStore');
goog.require('cvox.MathmlStore');
goog.require('cvox.MathmlStoreUtil');
goog.require('cvox.StoreUtil');


/**
 * Rule initialization.
 * @constructor
 */
cvox.MathmlStoreRules = function() {
  // Custom functions used in the rules.
  cvox.MathmlStoreRules.initCustomFunctions_();
  cvox.MathmlStoreRules.initDefaultRules_(); // MathML rules.
  cvox.MathmlStoreRules.initMathjaxRules_(); // MathJax Rules
  cvox.MathmlStoreRules.initAliases_(); // MathJax Aliases for MathML rules.
  cvox.MathmlStoreRules.initSpecializationRules_(); // Square, cube, etc.
  cvox.MathmlStoreRules.initSemanticRules_();
};
goog.addSingletonGetter(cvox.MathmlStoreRules);


/**
 * @type {cvox.MathStore}
 */
cvox.MathmlStoreRules.mathStore = cvox.MathmlStore.getInstance();
/**
 * @override
 */
cvox.MathmlStoreRules.mathStore.initialize = cvox.MathmlStoreRules.getInstance;

// These are used to work around Closure's rules for aliasing.
/** @private */
cvox.MathmlStoreRules.defineDefaultMathmlRule_ = goog.bind(
    cvox.MathmlStoreRules.mathStore.defineDefaultMathmlRule,
    cvox.MathmlStoreRules.mathStore);
/** @private */
cvox.MathmlStoreRules.defineRule_ = goog.bind(
    cvox.MathmlStoreRules.mathStore.defineRule,
    cvox.MathmlStoreRules.mathStore);
/** @private */
cvox.MathmlStoreRules.defineRuleAlias_ = goog.bind(
    cvox.MathmlStoreRules.mathStore.defineRuleAlias,
    cvox.MathmlStoreRules.mathStore);
/** @private */
cvox.MathmlStoreRules.addContextFunction_ = goog.bind(
    cvox.MathmlStoreRules.mathStore.contextFunctions.add,
    cvox.MathmlStoreRules.mathStore.contextFunctions);
/** @private */
cvox.MathmlStoreRules.addCustomQuery_ = goog.bind(
    cvox.MathmlStoreRules.mathStore.customQueries.add,
    cvox.MathmlStoreRules.mathStore.customQueries);

goog.scope(function() {
var defineDefaultMathmlRule = cvox.MathmlStoreRules.defineDefaultMathmlRule_;
var defineRule = cvox.MathmlStoreRules.defineRule_;
var defineRuleAlias = cvox.MathmlStoreRules.defineRuleAlias_;

var addCTXF = cvox.MathmlStoreRules.addContextFunction_;
var addCQF = cvox.MathmlStoreRules.addCustomQuery_;

/**
 * Initialize the custom functions.
 * @private
 */
cvox.MathmlStoreRules.initCustomFunctions_ = function() {
  addCTXF('CTXFnodeCounter', cvox.StoreUtil.nodeCounter);
  addCTXF('CTXFmfSeparators', cvox.MathmlStoreUtil.mfencedSeparators);
  addCTXF('CTXFcontentIterator', cvox.MathmlStoreUtil.contentIterator);

  addCQF('CQFextender', cvox.MathmlStoreUtil.retrieveMathjaxExtender);
  addCQF('CQFmathmlmunder', cvox.MathmlStoreUtil.checkMathjaxMunder);
  addCQF('CQFmathmlmover', cvox.MathmlStoreUtil.checkMathjaxMover);
  addCQF('CQFmathmlmsub', cvox.MathmlStoreUtil.checkMathjaxMsub);
  addCQF('CQFmathmlmsup', cvox.MathmlStoreUtil.checkMathjaxMsup);
  addCQF('CQFlookupleaf', cvox.MathmlStoreUtil.retrieveMathjaxLeaf);

};


/**
 * Initialize the default mathrules.
 * @private
 */
cvox.MathmlStoreRules.initDefaultRules_ = function() {
  // Initial rule
  defineDefaultMathmlRule('math', '[m] ./*');
  defineDefaultMathmlRule('semantics', '[n] ./*[1]');

  // Space elements
  defineDefaultMathmlRule('mspace', '[p] (pause:250)');
  defineDefaultMathmlRule('mstyle', '[m] ./*');
  defineDefaultMathmlRule('mpadded', '[m] ./*');
  defineDefaultMathmlRule('merror', '[t] ""');
  defineDefaultMathmlRule('mphantom', '[t] ""');

  // Token elements.
  defineDefaultMathmlRule('mtext', '[t] text(); [p] (pause:200)');
  defineDefaultMathmlRule('mi', '[n] text()');
  defineDefaultMathmlRule('mo', '[n] text() (rate:-0.1)');
  defineDefaultMathmlRule('mn', '[n] text()');

  // Dealing with fonts.
  defineRule('mtext-variant', 'default.default',
      '[t] "begin"; [t] @mathvariant (pause:150);' +
          '[t] text() (pause:150); [t] "end"; ' +
          '[t] @mathvariant (pause:200)',
      'self::mathml:mtext', '@mathvariant', '@mathvariant!="normal"');

  defineRule('mi-variant', 'default.default',
      '[t] @mathvariant; [n] text()',
      'self::mathml:mi', '@mathvariant', '@mathvariant!="normal"');

  defineRuleAlias('mi-variant', 'self::mathml:mn',  // mn
      '@mathvariant', '@mathvariant!="normal"');

  defineRule('mo-variant', 'default.default',
      '[t] @mathvariant; [n] text() (rate:-0.1)',
      'self::mathml:mo', '@mathvariant', '@mathvariant!="normal"');

  defineDefaultMathmlRule(
      'ms',
      '[t] "string" (pitch:0.5, rate:0.5); [t] text()');

  // Script elements.
  defineDefaultMathmlRule(
      'msup', '[n] ./*[1]; [t] "super";' +
          '[n] ./*[2] (pitch:0.35); [p] (pause:300)');
  defineDefaultMathmlRule(
      'msubsup',
      '[n] ./*[1]; [t] "sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:200);' +
          '[t] "super"; [n] ./*[3] (pitch:0.35); [p] (pause:300)'
      );
  defineDefaultMathmlRule(
      'msub',
      '[n] ./*[1]; [t] "sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:300)');
  defineDefaultMathmlRule(
      'mover', '[n] ./*[2] (pitch:0.35); [p] (pause:200);' +
          ' [t] "over"; [n] ./*[1]; [p] (pause:400)');
  defineDefaultMathmlRule(
      'munder',
      '[n] ./*[2] (pitch:-0.35); [t] "under"; [n] ./*[1]; [p] (pause:400)');
  defineDefaultMathmlRule(
      'munderover',
      '[n] ./*[2] (pitch:-0.35); [t] "under and"; [n] ./*[3] (pitch:0.35);' +
          ' [t] "over"; [n] ./*[1]; [p] (pause:400)');

  // Layout elements.
  defineDefaultMathmlRule('mrow', '[m] ./*');
  defineDefaultMathmlRule(
      'msqrt', '[t] "Square root of"; [m] ./* (rate:0.2); [p] (pause:400)');
  defineDefaultMathmlRule(
      'mroot', '[t] "root of order"; [n] ./*[2]; [t] "of";' +
          '[n] ./*[1] (rate:0.2); [p] (pause:400)');
  defineDefaultMathmlRule(
      'mfrac', ' [p] (pause:400); [n] ./*[1] (pitch:0.3);' +
          ' [t] "divided by"; [n] ./*[2] (pitch:-0.3); [p] (pause:400)');
  defineRule(
      'mfrac', 'default.short', '[p] (pause:200); [t] "start frac";' +
          '[n] ./*[1] (pitch:0.3); [t] "over"; ' +
          '[n] ./*[2] (pitch:-0.3); [p] (pause:400); [t] "end frac"',
      'self::mathml:mfrac');


  defineRule(
      'mfenced-single', 'default.default',
      '[t] concat(substring(@open, 0 div boolean(@open)), ' +
          'substring("(", 0 div not(boolean(@open)))) (context:"opening"); ' +
          '[m] ./* (separator:@separators); ' +
          '[t] concat(substring(@close, 0 div boolean(@close)), ' +
          'substring(")", 0 div not(boolean(@close)))) (context:"closing")',
      'self::mathml:mfenced', 'string-length(string(@separators))=1');

  defineRule(
      'mfenced-omit', 'default.default',
      '[t] concat(substring(@open, 0 div boolean(@open)), ' +
          'substring("(", 0 div not(boolean(@open)))) (context:"opening"); ' +
          '[m] ./*; ' +
          '[t] concat(substring(@close, 0 div boolean(@close)), ' +
          'substring(")", 0 div not(boolean(@close)))) (context:"closing")',
      'self::mathml:mfenced', '@separators',
      'string-length(string(@separators))=0', 'string(@separators)=""');

  defineRule(
      'mfenced-empty', 'default.default',
      '[t] concat(substring(@open, 0 div boolean(@open)), ' +
          'substring("(", 0 div not(boolean(@open)))) (context:"opening"); ' +
          '[m] ./*;' +
          '[t] concat(substring(@close, 0 div boolean(@close)), ' +
          'substring(")", 0 div not(boolean(@close)))) (context:"closing")',
      'self::mathml:mfenced', 'string-length(string(@separators))=1',
      'string(@separators)=" "');

  defineRule(
      'mfenced-comma', 'default.default',
      '[t] concat(substring(@open, 0 div boolean(@open)), ' +
          'substring("(", 0 div not(boolean(@open)))) (context:"opening"); ' +
          '[m] ./* (separator:"comma");' +
          '[t] concat(substring(@close, 0 div boolean(@close)), ' +
          'substring(")", 0 div not(boolean(@close)))) (context:"closing")',
      'self::mathml:mfenced');

  defineRule(
      'mfenced-multi', 'default.default',
      '[t] concat(substring(@open, 0 div boolean(@open)), ' +
          'substring("(", 0 div not(boolean(@open)))) (context:"opening"); ' +
          '[m] ./* (sepFunc:CTXFmfSeparators, separator:@separators); ' +
          '[t] concat(substring(@close, 0 div boolean(@close)), ' +
          'substring(")", 0 div not(boolean(@close)))) (context:"closing")',
      'self::mathml:mfenced', 'string-length(string(@separators))>1');

  // Mtable rules.
  defineRule(
      'mtable', 'default.default',
      '[t] "matrix"; [m] ./* (ctxtFunc:CTXFnodeCounter,' +
          'context:"row",pause:100)',
      'self::mathml:mtable');

  defineRule(
      'mtr', 'default.default',
      '[m] ./* (ctxtFunc:CTXFnodeCounter,context:"column",pause:100)',
      'self::mathml:mtr');

  defineRule(
      'mtd', 'default.default',
      '[m] ./*', 'self::mathml:mtd');

  // Mtable superbrief rules.
  defineRule(
      'mtable', 'default.superbrief',
      '[t] count(child::mathml:mtr);  [t] "by";' +
          '[t] count(child::mathml:mtr[1]/mathml:mtd); [t] "matrix";',
      'self::mathml:mtable');

  // Mtable short rules.
  defineRule(
      'mtable', 'default.short',
      '[t] "matrix"; [m] ./*',
      'self::mathml:mtable');

  defineRule(
      'mtr', 'default.short',
      '[m] ./*', 'self::mathml:mtr');

  defineRule(
      'mtd', 'default.short',
      '[t] "Element"; [t] count(./preceding-sibling::mathml:mtd)+1;' +
          '[t] count(./parent::mathml:mtr/preceding-sibling::mathml:mtr)+1;' +
              '[p] (pause:500); [m] ./*',
      'self::mathml:mtd');

  // Mmultiscripts rules.
  defineRule(
      'mmultiscripts-4', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:200);' +
      '[t] "right sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:200);' +
      '[t] "right super"; [n] ./*[3] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts');
  defineRule(
      'mmultiscripts-3-1', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:200);' +
      '[t] "right super"; [n] ./*[3] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts', './mathml:none=./*[2]',
      './mathml:mprescripts=./*[4]');
  defineRule(
      'mmultiscripts-3-2', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:200);' +
      '[t] "right sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:200);',
      'self::mathml:mmultiscripts', './mathml:none=./*[3]',
      './mathml:mprescripts=./*[4]');
  defineRule(
      'mmultiscripts-3-3', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:200);' +
      '[t] "right sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:200);' +
      '[t] "right super"; [n] ./*[3] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts', './mathml:none=./*[5]',
      './mathml:mprescripts=./*[4]');
  defineRule(
      'mmultiscripts-3-4', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);' +
      '[t] "right sub"; [n] ./*[2] (pitch:-0.35); [p] (pause:200);' +
      '[t] "right super"; [n] ./*[3] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts', './mathml:none=./*[6]',
      './mathml:mprescripts=./*[4]');
  defineRule(
      'mmultiscripts-2-1', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts', './mathml:none=./*[2]',
      './mathml:none=./*[3]', './mathml:mprescripts=./*[4]');
  defineRule(
      'mmultiscripts-1-1', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left super"; [n] ./*[6] (pitch:0.35); [p] (pause:300);',
      'self::mathml:mmultiscripts', './mathml:none=./*[2]',
      './mathml:none=./*[3]', './mathml:mprescripts=./*[4]',
      './mathml:none=./*[5]');
  defineRule(
      'mmultiscripts-1-2', 'default.default',
      '[n] ./*[1]; [p] (pause:200);' +
      '[t] "left sub"; [n] ./*[5] (pitch:-0.35); [p] (pause:200);',
      'self::mathml:mmultiscripts', './mathml:none=./*[2]',
      './mathml:none=./*[3]', './mathml:mprescripts=./*[4]',
      './mathml:none=./*[6]');
};


/**
 * Initialize mathJax Rules
 * @private
 */
cvox.MathmlStoreRules.initMathjaxRules_ = function() {
  // Initial rule
  defineRule('mj-math', 'default.default',
             '[n] ./*[1]/*[1]/*[1]', 'self::span[@class="math"]');

  // Token Elements
  defineRule(
      'mj-leaf', 'default.default',
      '[n] CQFlookupleaf', 'self::span[@class="mi"]');
  defineRuleAlias('mj-leaf', 'self::span[@class="mo"]');
  defineRuleAlias('mj-leaf', 'self::span[@class="mn"]');
  defineRuleAlias('mj-leaf', 'self::span[@class="mtext"]');
  defineRule(
      'mj-mo-ext', 'default.default',
      '[n] CQFextender', 'self::span[@class="mo"]',
      './*[1]/*[1]/text()', './*[1]/*[2]/text()');
  defineRule(
      'mj-texatom', 'default.default',
      '[n] ./*[1]', 'self::span[@class="texatom"]');

  // Script elements.
  defineRule(
      'mj-msubsup', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "sub"; [n] ./*[1]/*[3]/*[1] (pitch:-0.35);' +
      '[p] (pause:200); [t] "super"; [n] ./*[1]/*[2]/*[1] (pitch:0.35);' +
      '[p] (pause:300)',
      'self::span[@class="msubsup"]');
  defineRule(
      'mj-msub', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "sub";' +
          '[n] ./*[1]/*[2]/*[1] (pitch:-0.35); [p] (pause:300)',
      'self::span[@class="msub"]');
  defineRule(
      'mj-msup', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "super";' +
          '[n] ./*[1]/*[2]/*[1] (pitch:0.35); [p] (pause:300)',
      'self::span[@class="msup"]');
  defineRule(
      'mj-munderover', 'default.default',
      '[n] ./*[1]/*[2]/*[1] (pitch:0.35); [t] "under and";' +
          '[n] ./*[1]/*[3]/*[1] (pitch:-0.35); [t] "over";' +
              '[n] ./*[1]/*[1]/*[1]; [p] (pause:400)',
      'self::span[@class="munderover"]');
  defineRule(
      'mj-munder', 'default.default',
      '[n] ./*[1]/*[2]/*[1] (pitch:0.35); [t] "under";' +
          '[n] ./*[1]/*[1]/*[1]; [p] (pause:400)',
      'self::span[@class="munder"]');
  defineRule(
      'mj-mover', 'default.default',
      '[n] ./*[1]/*[2]/*[1] (pitch:0.35); [t] "over";' +
          '[n] ./*[1]/*[1]/*[1]; [p] (pause:400)',
      'self::span[@class="mover"]');


  // Layout elements.
  defineRule(
      'mj-mfrac', 'default.default',
      '[p] (pause:250); [n] ./*[1]/*[1]/*[1] (pitch:0.3); [p] (pause:250);' +
          ' [t] "divided by"; [n] ./*[1]/*[2]/*[1] (pitch:-0.3);' +
              '[p] (pause:400)',
      'self::span[@class="mfrac"]');
  defineRule(
      'mj-msqrt', 'default.default',
      '[t] "Square root of";' +
          '[n] ./*[1]/*[1]/*[1] (rate:0.2); [p] (pause:400)',
      'self::span[@class="msqrt"]');
  defineRule(
      'mj-mroot', 'default.default',
      '[t] "root of order"; [n] ./*[1]/*[4]/*[1]; [t] "of";' +
          '[n] ./*[1]/*[1]/*[1] (rate:0.2); [p] (pause:400)',
      'self::span[@class="mroot"]');

  defineRule(
      'mj-mfenced', 'default.default',
      '[t] "opening"; [n] ./*[1]; ' +
          '[m] ./*[position()>1 and position()<last()];' +
              ' [t] "closing"; [n] ./*[last()]',
      'self::span[@class="mfenced"]');

  // Mtable short rules.
  defineRuleAlias('mj-leaf', 'self::span[@class="mtable"]');
  // Mmultiscripts rules.
  defineRuleAlias('mj-leaf', 'self::span[@class="mmultiscripts"]');
};


/**
 * Initialize mathJax Aliases
 * @private
 */
cvox.MathmlStoreRules.initAliases_ = function() {
  // Space elements
  defineRuleAlias('mspace', 'self::span[@class="mspace"]');
  defineRuleAlias('mstyle', 'self::span[@class="mstyle"]');
  defineRuleAlias('mpadded', 'self::span[@class="mpadded"]');
  defineRuleAlias('merror', 'self::span[@class="merror"]');
  defineRuleAlias('mphantom', 'self::span[@class="mphantom"]');

  // Token elements.
  defineRuleAlias('ms', 'self::span[@class="ms"]');

  // Layout elements.
  defineRuleAlias('mrow', 'self::span[@class="mrow"]');

  // The following rules fix bugs in MathJax's LaTeX translation.
  defineRuleAlias(
      'mj-msub', 'self::span[@class="msubsup"]', 'CQFmathmlmsub');

  defineRuleAlias(
      'mj-msup', 'self::span[@class="msubsup"]', 'CQFmathmlmsup');

  defineRuleAlias(
      'mj-munder', 'self::span[@class="munderover"]', 'CQFmathmlmunder');

  defineRuleAlias(
      'mj-mover', 'self::span[@class="munderover"]', 'CQFmathmlmover');
};


/**
 * Initialize specializations wrt. content of nodes.
 * @private
 */
cvox.MathmlStoreRules.initSpecializationRules_ = function() {
  // Some special nodes for square and cube.
  // MathML
  defineRule(
      'square', 'default.default',
      '[n] ./*[1]; [t] "square" (pitch:0.35); [p] (pause:300)',
      'self::mathml:msup', './*[2][text()=2]');
  defineRuleAlias(
      'square', 'self::mathml:msup',
      './mathml:mrow=./*[2]', 'count(./*[2]/*)=1', './*[2]/*[1][text()=2]');

  defineRule(
      'cube', 'default.default',
      '[n] ./*[1]; [t] "cube" (pitch:0.35); [p] (pause:300)',
      'self::mathml:msup', './*[2][text()=3]');
  defineRuleAlias(
      'cube', 'self::mathml:msup',
      './mathml:mrow=./*[2]', 'count(./*[2]/*)=1', './*[2]/*[1][text()=3]');

  defineRule(
      'square-sub', 'default.default',
      '[n] ./*[1]; [t] "sub"; [n] ./*[2] (pitch:-0.35);' +
          '[p] (pause:300); [t] "square" (pitch:0.35); [p] (pause:400)',
      'self::mathml:msubsup', './*[3][text()=2]');
  defineRuleAlias(
      'square-sub', 'self::mathml:msubsup',
      './mathml:mrow=./*[3]', 'count(./*[3]/*)=1', './*[3]/*[1][text()=2]');

  defineRule(
      'cube-sub', 'default.default',
      '[n] ./*[1]; [t] "sub"; [n] ./*[2] (pitch:-0.35);' +
          '[p] (pause:300); [t] "cube" (pitch:0.35); [p] (pause:400)',
      'self::mathml:msubsup', './*[3][text()=3]');
  defineRuleAlias(
      'cube-sub', 'self::mathml:msubsup',
      './mathml:mrow=./*[3]', 'count(./*[3]/*)=1', './*[3]/*[1][text()=3]');

  // MathJax
  defineRule(
      'mj-square', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "square" (pitch:0.35); [p] (pause:300)',
      'self::span[@class="msup"]', './*[1]/*[2]/*[1][text()=2]');
  defineRuleAlias(
      'mj-square', 'self::span[@class="msup"]',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=2]');
  defineRuleAlias(
      'mj-square', 'self::span[@class="msubsup"]', 'CQFmathmlmsup',
      './*[1]/*[2]/*[1][text()=2]');
  defineRuleAlias(
      'mj-square', 'self::span[@class="msubsup"]', 'CQFmathmlmsup',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=2]');

  defineRule(
      'mj-cube', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "cube" (pitch:0.35); [p] (pause:300)',
      'self::span[@class="msup"]', './*[1]/*[2]/*[1][text()=3]');
  defineRuleAlias(
      'mj-cube', 'self::span[@class="msup"]',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=3]');
  defineRuleAlias(
      'mj-cube', 'self::span[@class="msubsup"]', 'CQFmathmlmsup',
      './*[1]/*[2]/*[1][text()=3]');
  defineRuleAlias(
      'mj-cube', 'self::span[@class="msubsup"]', 'CQFmathmlmsup',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=3]');

  defineRule(
      'mj-square-sub', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "sub"; [n] ./*[1]/*[3]/*[1] (pitch:-0.35); ' +
          '[p] (pause:300); [t] "square" (pitch:0.35); [p] (pause:400)',
      'self::span[@class="msubsup"]', './*[1]/*[2]/*[1][text()=2]');
  defineRuleAlias(
      'mj-square-sub', 'self::span[@class="msubsup"]',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=2]');

  defineRule(
      'mj-cube-sub', 'default.default',
      '[n] ./*[1]/*[1]/*[1]; [t] "sub"; [n] ./*[1]/*[3]/*[1] (pitch:-0.35); ' +
          '[p] (pause:300); [t] "cube" (pitch:0.35); [p] (pause:400)',
      'self::span[@class="msubsup"]', './*[1]/*[2]/*[1][text()=3]');
  defineRuleAlias(
      'mj-cube-sub', 'self::span[@class="msubsup"]',
      './*[1]/*[2]/*[1]=./*[1]/*[2]/span[@class="mrow"]',
      'count(./*[1]/*[2]/*[1]/*)=1', './*[1]/*[2]/*[1]/*[1][text()=3]');
};


/**
 * Initialize mathJax Aliases
 * @private
 */
cvox.MathmlStoreRules.initSemanticRules_ = function() {
  // Initial rule
  defineRule(
      'stree', 'default.default',
      '[n] ./*[1]', 'self::stree');

  defineRule(
      'multrel', 'default.default',
      '[t] "multirelation"; [m] children/* (sepFunc:CTXFcontentIterator)',
      'self::multirel');

  defineRule(
      'variable-equality', 'default.default',
      '[t] "equation sequence"; [m] ./children/* ' +
          '(context:"part",ctxtFunc:CTXFnodeCounter,separator:./text())',
      'self::relseq[@role="equality"]', 'count(./children/*)>2',
      './children/punct[@role="ellipsis"]');// Make that better!

  defineRule(
      'multi-equality', 'default.default',
      '[t] "equation sequence"; [m] ./children/* ' +
          '(context:"part",ctxtFunc:CTXFnodeCounter,separator:./text())',
      'self::relseq[@role="equality"]', 'count(./children/*)>2');

  defineRule(
      'multi-equality', 'default.short',
      '[t] "equation sequence"; [m] ./children/* ' +
          '(separator:./text())',
      'self::relseq[@role="equality"]', 'count(./children/*)>2');

  defineRule(
      'equality', 'default.default',
      '[t] "equation"; [t] "left hand side"; [n] children/*[1];' +
          '[p] (pause:200); [n] text() (pause:200);' +
          '[t] "right hand side"; [n] children/*[2]',
      'self::relseq[@role="equality"]', 'count(./children/*)=2');

  defineRule(
      'simple-equality', 'default.default',
      '[n] children/*[1]; [p] (pause:200); [n] text() (pause:200);' +
          '[n] children/*[2]',
      'self::relseq[@role="equality"]', 'count(./children/*)=2',
      './children/identifier or ./children/number');

  defineRule(
      'simple-equality2', 'default.default',
      '[n] children/*[1]; [p] (pause:200); [n] text() (pause:200);' +
          '[n] children/*[2]',
      'self::relseq[@role="equality"]', 'count(./children/*)=2',
      './children/function or ./children/appl');

  defineRule(
      'multrel', 'default.default',
      '[m] children/* (separator:./text())',
      'self::relseq');

  defineRule(
      'binary-operation', 'default.default',
      '[m] children/* (separator:text());',
      'self::infixop');

  defineRule(
      'variable-addition', 'default.default',
      '[t] "sum with variable number of summands";' +
          '[p] (pause:400); [m] children/* (separator:./text())',
      'self::infixop[@role="addition"]', 'count(children/*)>2',
      'children/punct[@role="ellipsis"]');// Make that better!

  defineRule(
      'multi-addition', 'default.default',
      '[t] "sum with,"; [t] count(./children/*); [t] ", summands";' +
          '[p] (pause:400); [m] ./children/* (separator:./text())',
      'self::infixop[@role="addition"]', 'count(./children/*)>2');

  // Prefix Operator
  defineRule(
      'prefix', 'default.default',
      '[t] "prefix"; [n] text(); [t] "of" (pause 150);' +
      '[n] children/*[1]',
      'self::prefixop');

  defineRule(
      'negative', 'default.default',
      '[t] "negative"; [n] children/*[1]',
      'self::prefixop', 'self::prefixop[@role="negative"]');

  // Postfix Operator
  defineRule(
      'postfix', 'default.default',
      '[n] children/*[1]; [t] "postfix"; [n] text() (pause 300)',
      'self::postfixop');

  defineRule(
      'identifier', 'default.default',
      '[n] text()', 'self::identifier');

  defineRule(
      'number', 'default.default',
      '[n] text()', 'self::number');

  defineRule(
      'fraction', 'default.default',
      '[p] (pause:250); [n] children/*[1] (pitch:0.3); [p] (pause:250);' +
          ' [t] "divided by"; [n] children/*[2] (pitch:-0.3); [p] (pause:400)',
      'self::fraction');

  defineRule(
      'superscript', 'default.default',
      '[n] children/*[1]; [t] "super"; [n] children/*[2] (pitch:0.35);' +
      '[p] (pause:300)',
      'self::superscript');
  defineRule(
      'subscript', 'default.default',
      '[n] children/*[1]; [t] "sub"; [n] children/*[2] (pitch:-0.35);' +
      '[p] (pause:300)',
      'self::subscript');

  defineRule(
      'ellipsis', 'default.default',
      '[p] (pause:200); [t] "dot dot dot"; [p] (pause:300)',
      'self::punct', 'self::punct[@role="ellipsis"]');

  defineRule(
      'fence-single', 'default.default',
      '[n] text()',
      'self::punct', 'self::punct[@role="openfence"]');
  defineRuleAlias('fence-single', 'self::punct',
                  'self::punct[@role="closefence"]');
  defineRuleAlias('fence-single', 'self::punct',
                  'self::punct[@role="vbar"]');
  defineRuleAlias('fence-single', 'self::punct',
                  'self::punct[@role="application"]');

  // TODO (sorge) Refine punctuations further.
  defineRule(
      'omit-punct', 'default.default',
      '[p] (pause:200);',
      'self::punct');

  defineRule(
      'omit-empty', 'default.default',
      '',
      'self::empty');

  // Fences rules.
  defineRule(
      'fences-open-close', 'default.default',
      '[p] (pause:100); [t] "open"; [n] children/*[1]; [p] (pause:200);' +
      '[t] "close"',
      'self::fenced[@role="leftright"]');

  defineRule(
      'fences-open-close-in-appl', 'default.default',
      '[p] (pause:100); [n] children/*[1]; [p] (pause:200);',
      'self::fenced[@role="leftright"]', './parent::children/parent::appl');

  defineRule(
      'fences-neutral', 'default.default',
      '[p] (pause:100); [t] "absolute value of"; [n] children/*[1];' +
      '[p] (pause:350);',
      'self::fenced', 'self::fenced[@role="neutral"]');

  defineRule(
      'omit-fences', 'default.default',
      '[p] (pause:500); [n] children/*[1]; [p] (pause:200);',
      'self::fenced');

  // Matrix rules.
  defineRule(
      'matrix', 'default.default',
      '[t] "matrix"; [m] children/* ' +
      '(ctxtFunc:CTXFnodeCounter,context:"row",pause:100)',
      'self::matrix');

  defineRule(
      'matrix-row', 'default.default',
      '[m] children/* (ctxtFunc:CTXFnodeCounter,context:"column",pause:100)',
      'self::row[@role="matrix"]');

  defineRule(
      'matrix-cell', 'default.default',
      '[n] children/*[1]', 'self::cell[@role="matrix"]');

  // Vector rules.
  defineRule(
      'vector', 'default.default',
      '[t] "vector"; [m] children/* ' +
      '(ctxtFunc:CTXFnodeCounter,context:"element",pause:100)',
      'self::vector');

  // Cases rules.
  defineRule(
      'cases', 'default.default',
      '[t] "case statement"; [m] children/* ' +
      '(ctxtFunc:CTXFnodeCounter,context:"case",pause:100)',
      'self::cases');

  defineRule(
      'cases-row', 'default.default',
      '[m] children/*', 'self::row[@role="cases"]');

  defineRule(
      'cases-cell', 'default.default',
      '[n] children/*[1]', 'self::cell[@role="cases"]');

  defineRule(
      'row', 'default.default',
      '[m] ./* (ctxtFunc:CTXFnodeCounter,context:"column",pause:100)',
      'self::row"');

  defineRule(
      'cases-end', 'default.default',
      '[t] "case statement"; ' +
      '[m] children/* (ctxtFunc:CTXFnodeCounter,context:"case",pause:100);' +
      '[t] "end cases"',
      'self::cases', 'following-sibling::*');

  // Multiline rules.
  defineRule(
      'multiline', 'default.default',
      '[t] "multiline equation";' +
      '[m] children/* (ctxtFunc:CTXFnodeCounter,context:"line",pause:100)',
      'self::multiline');

  defineRule(
      'line', 'default.default',
      '[m] children/*', 'self::line');

  // Table rules.
  defineRule(
      'table', 'default.default',
      '[t] "multiline equation";' +
      '[m] children/* (ctxtFunc:CTXFnodeCounter,context:"row",pause:200)',
      'self::table');

  defineRule(
      'table-row', 'default.default',
      '[m] children/* (pause:100)', 'self::row[@role="table"]');

  defineRuleAlias(
      'cases-cell', 'self::cell[@role="table"]');


  // Rules for punctuated expressions.
  defineRule(
      'end-punct', 'default.default',
      '[m] children/*; [p] (pause:300)',
      'self::punctuated', '@role="endpunct"');

  defineRule(
      'start-punct', 'default.default',
      '[n] content/*[1]; [p] (pause:200); [m] children/*',
      'self::punctuated', '@role="startpunct"');

  defineRule(
      'integral-punct', 'default.default',
      '[n] children/*[1] (rate:0.2); [n] children/*[3] (rate:0.2)',
      'self::punctuated', '@role="integral"');

  defineRule(
      'punctuated', 'default.default',
      '[m] children/* (pause:100)',
      'self::punctuated');

  // Function rules
  defineRule(
    'function', 'default.default',
    '[n] text()', 'self::function');

  defineRule(
    'appl', 'default.default',
    '[n] children/*[1]; [n] content/*[1]; [n] children/*[2]', 'self::appl');

  // Limit operator rules
  defineRule(
    'limboth', 'default.default',
    '[n] children/*[1]; [t] "from"; [n] children/*[2]; [t] "to";' +
    '[n] children/*[3]', 'self::limboth');

  defineRule(
    'sum-only', 'default.default',
    '[n] children/*[1]; [p] (pause 100); [t] "over"; [n] children/*[2];' +
    '[p] (pause 250);',
    'self::limboth', 'self::limboth[@role="sum"]');

  defineRule(
    'limlower', 'default.default',
    '[n] children/*[1]; [t] "over"; [n] children/*[2];', 'self::limlower');

  defineRule(
    'limupper', 'default.default',
    '[n] children/*[1]; [t] "under"; [n] children/*[2];', 'self::limupper');

  // Bigoperator rules
  defineRule(
    'largeop', 'default.default',
    '[n] text()', 'self::largeop');

  defineRule(
    'bigop', 'default.default',
    '[n] children/*[1]; [p] (pause 100); [t] "over"; [n] children/*[2];' +
    '[p] (pause 250);',
    'self::bigop');


  // Integral rules
  defineRule(
    'integral', 'default.default',
    '[n] children/*[1]; [p] (pause 100); [n] children/*[2]; [p] (pause 200);' +
    '[n] children/*[3] (rate:0.35);', 'self::integral');


  defineRule(
      'sqrt', 'default.default',
      '[t] "Square root of"; [n] children/*[1] (rate:0.2); [p] (pause:400)',
      'self::sqrt');

  defineRule(
      'square', 'default.default',
      '[n] children/*[1]; [t] "square" (pitch:0.35); [p] (pause:300)',
      'self::superscript', 'children/*[2][text()=2]');

  defineRule(
      'text-no-mult', 'default.default',
      '[n] children/*[1]; [p] (pause:200); [n] children/*[2]',
      'self::infixop', 'children/text');
};

}); // goog.scope
