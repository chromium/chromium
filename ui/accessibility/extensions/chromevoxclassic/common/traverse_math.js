// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.TraverseMath');

goog.require('cvox.ChromeVox');
goog.require('cvox.DomUtil');
goog.require('cvox.SemanticTree');


/**
 * Initializes the traversal with the provided math node.
 *
 * @constructor
 */
cvox.TraverseMath = function() {
  /**
   * The active math <MATH> node. In this context, "active" means that this is
   * the math expression the TraverseMath object is navigating.
   * @type {Node}
   */
  this.activeMath = null;

  /**
   * The node currently under inspection.
   * @type {Node}
   */
  this.activeNode = null;

  /**
   * Dictionary of all LaTeX elements in the page if there are any.
   * @type {!Object<!Node>}
   * @private
   */
  this.allTexs_ = {};

  /**
   * Dictionary of all MathJaxs elements in the page if there are any.
   * @type {!Object<!Node>}
   * @private
   */
  this.allMathjaxs_ = {};

  /**
   * Dictionary of all MathJaxs elements that have not yet been translated at
   * page load or during MathJax rendering.
   * @type {!Object<!Node>}
   * @private
   */
  this.todoMathjaxs_ = {};

  /**
   * When traversing a Mathjax node this will contain the internal
   * MathML representation of the node.
   * @type {Node}
   */
  this.activeMathmlHost = null;

  /**
   * Semantic representation of the current node.
   * @type {Node}
   */
  this.activeSemanticHost = null;

  /**
   * List of domain names.
   * @type {Array<string>}
   */
  this.allDomains = [];

  /**
   * List of style names.
   * @type {Array<string>}
   */
  this.allStyles = [];

  /**
   * Current domain.
   * @type {string}
   */
  this.domain = 'default';

  /**
   * Current style.
   * @type {string}
   */
  this.style = 'short';

  /**
   * Initialize special objects if necessary.
   */
  if (cvox.ChromeVox.mathJax) {
    this.initializeMathjaxs();
    this.initializeAltMaths();
  }
};
goog.addSingletonGetter(cvox.TraverseMath);


/**
 * @type {boolean}
 * @private
 */
cvox.TraverseMath.setSemantic_ = false;


/**
 * Toggles the semantic setting.
 * @return {boolean} True if semantic interpretation is switched on. False
 *     otherwise.
 */
cvox.TraverseMath.toggleSemantic = function() {
  return cvox.TraverseMath.setSemantic_ = !cvox.TraverseMath.setSemantic_;
};


/**
 * Initializes a traversal of a math expression.
 * @param {Node} node A MathML node.
 */
cvox.TraverseMath.prototype.initialize = function(node) {
  if (cvox.DomUtil.isMathImg(node)) {
    // If a node has a cvoxid attribute we know that it contains a LaTeX
    // expression that we have rewritten into its corresponding MathML
    // representation, which we can speak and walk.
    if (!node.hasAttribute('cvoxid')) {
      return;
    }
    var cvoxid = node.getAttribute('cvoxid');
    node = this.allTexs_[cvoxid];
  }
  if (cvox.DomUtil.isMathJax(node)) {
      this.activeMathmlHost = this.allMathjaxs_[node.getAttribute('id')];
  }
  this.activeMath = this.activeMathmlHost || node;
  this.activeNode = this.activeMathmlHost || node;
  if (this.activeNode && cvox.TraverseMath.setSemantic_ &&
      this.activeNode.nodeType == Node.ELEMENT_NODE) {
    this.activeNode =
        (new cvox.SemanticTree(/** @type {!Element} */ (this.activeNode))).xml();
  }
};


/**
 * Adds a mapping of a MathJax node to its MathML representation to the
 * dictionary of MathJax elements.
 * @param {!Node} mml The MathML node.
 * @param {string} id The MathJax node id.
 */
cvox.TraverseMath.prototype.addMathjax = function(mml, id) {
  var spanId = cvox.DomUtil.getMathSpanId(id);
  if (spanId) {
    this.allMathjaxs_[spanId] = mml;
  } else {
    this.redoMathjaxs(mml, id);
  }
};


/**
 * Retries to compute MathML representations of MathJax elements, if
 * they have not been filled in during rendering.
 * @param {!Node} mml The MathML node.
 * @param {string} id The MathJax node id.
 */
cvox.TraverseMath.prototype.redoMathjaxs = function(mml, id) {
  var fetch = goog.bind(function() {this.addMathjax(mml, id);}, this);
  setTimeout(fetch, 500);
};


/**
 * Initializes the MathJax to MathML mapping.
 * We first try to get all MathJax elements that are already being rendered.
 * Secondly, we register a signal to get updated on all elements that are
 * rendered or re-rendered later.
 */
cvox.TraverseMath.prototype.initializeMathjaxs = function() {
  var callback =
      goog.bind(function(mml, id) {
                  this.addMathjax(mml, id);
                }, this);
  cvox.ChromeVox.mathJax.isMathjaxActive(
      function(bool) {
        if (bool) {
          cvox.ChromeVox.mathJax.getAllJax(callback);
          cvox.ChromeVox.mathJax.registerSignal(callback, 'New Math');
        }
      });
};


/**
 * Initializes the elements in the page that we identify as potentially
 * containing tex or asciimath alt text.
 */
cvox.TraverseMath.prototype.initializeAltMaths = function() {
  if (!document.querySelector(
      cvox.DomUtil.altMathQuerySelector('tex') + ', ' +
          cvox.DomUtil.altMathQuerySelector('asciimath'))) {
    return;
  }
  var callback = goog.bind(
      function(mml, id) {
        this.allTexs_[id] = mml;
      }, this);
  // Inject a minimalistic version of MathJax into the page.
  cvox.ChromeVox.mathJax.injectScripts();
  // Once MathJax is injected we harvest all Latex and AsciiMath in alt
  // attributes and translate them to MathML expression.
  cvox.ChromeVox.mathJax.isMathjaxActive(
      function(active) {
        if (active) {
          cvox.ChromeVox.mathJax.configMediaWiki();
          cvox.ChromeVox.mathJax.getAllTexs(callback);
          cvox.ChromeVox.mathJax.getAllAsciiMaths(callback);
        }
      });
};


/**
 * Moves to the next leaf node in the current Math expression if it exists.
 * @param {boolean} reverse True if reversed. False by default.
 * @param {function(!Node):boolean} pred Predicate deciding what a leaf is.
 * @return {Node} The next node.
 */
cvox.TraverseMath.prototype.nextLeaf = function(reverse, pred) {
  if (this.activeNode && this.activeMath) {
    var next = pred(this.activeNode) ?
      cvox.DomUtil.directedFindNextNode(
          this.activeNode, this.activeMath, reverse, pred) :
      cvox.DomUtil.directedFindFirstNode(this.activeNode, reverse, pred);
    if (next) {
      this.activeNode = next;
    }
  }
  return this.activeNode;
};


// TODO (sorge) Refactor this logic into single walkers.
/**
 * Returns a string with the content of the active node.
 * @return {string} The active content.
 */
cvox.TraverseMath.prototype.activeContent = function() {
  return this.activeNode.textContent;
};


/**
 * Moves to the next subtree from a given node in a depth first fashion.
 * @param {boolean} reverse True if reversed. False by default.
 * @param {function(!Node):boolean} pred Predicate deciding what a subtree is.
 * @return {Node} The next subtree.
 */
cvox.TraverseMath.prototype.nextSubtree = function(reverse, pred) {
  if (!this.activeNode || !this.activeMath) {
    return null;
  }
  if (!reverse) {
    var child = cvox.DomUtil.directedFindFirstNode(
        this.activeNode, reverse, pred);
    if (child) {
      this.activeNode = child;
    } else {
      var next = cvox.DomUtil.directedFindNextNode(
          this.activeNode, this.activeMath, reverse, pred);
      if (next) {
          this.activeNode = next;
      }
    }
  } else {
    if (this.activeNode == this.activeMath) {
      var child = cvox.DomUtil.directedFindDeepestNode(
          this.activeNode, reverse, pred);
      if (child != this.activeNode) {
        this.activeNode = child;
        return this.activeNode;
      }
    }
    var prev = cvox.DomUtil.directedFindNextNode(
      this.activeNode, this.activeMath, reverse, pred, true, true);
    if (prev) {
      this.activeNode = prev;
    }
  }
  return this.activeNode;
};


/**
 * left or right in the math expression.
 * Navigation is bounded by the presence of a sibling.
 * @param {boolean} r True to move left; false to move right.
 * @return {Node} The result.
 */
cvox.TraverseMath.prototype.nextSibling = function(r) {
  if (!this.activeNode || !this.activeMath) {
    return null;
  }
  var node = this.activeNode;
      node = r ? node.previousSibling : node.nextSibling;
  if (!node) {
    return null;
  }
  this.activeNode = node;
  return this.activeNode;
};


/**
 * Moves up or down the math expression.
 * Navigation is bounded by the root math expression.
 * @param {boolean} r True to move up; false to move down.
 * @return {Node} The result.
 */
cvox.TraverseMath.prototype.nextParentChild = function(r) {
  if (!this.activeNode || !this.activeMath) {
    return null;
  }
  if (this.activeNode == this.activeMath && r) {
    return null;
  }
  var node = this.activeNode;
  node = r ? node.parentNode : node.firstChild;
  if (!node) {
    return null;
  }
  this.activeNode = node;
  return this.activeNode;
};


/**
 * Adds a list of domains and styles to the existing one.
 * @param {Array<string>} domains List of domain names.
 * @param {Array<string>} styles List of style names.
 */
cvox.TraverseMath.prototype.addDomainsAndStyles = function(domains, styles) {
  this.allDomains.push.apply(
      this.allDomains,
      domains.filter(
          goog.bind(function(x) {return this.allDomains.indexOf(x) < 0;},
                    this)));
  this.allStyles.push.apply(
      this.allStyles,
      styles.filter(
          goog.bind(function(x) {return this.allStyles.indexOf(x) < 0;},
                    this)));
};


/**
 * Gets a list of domains and styles from the symbol and function mappings.
 * Depending on the platform they either live in the background page or
 * in the android math map.
 */
cvox.TraverseMath.prototype.initDomainsAndStyles = function() {
  if (cvox.ChromeVox.host['mathMap']) {
    this.addDomainsAndStyles(
        cvox.ChromeVox.host['mathMap'].allDomains,
        cvox.ChromeVox.host['mathMap'].allStyles);
    } else {
      cvox.ChromeVox.host.sendToBackgroundPage(
          {'target': 'Math',
           'action': 'getDomains'});
    }
};


/**
 * Sets the domain for the TraverseMath object to the next one in the list
 * restarting from the first, if necessary.
 * @return {string} The name of the newly set domain.
 */
cvox.TraverseMath.prototype.cycleDomain = function() {
  this.initDomainsAndStyles();
  var index = this.allDomains.indexOf(this.domain);
  if (index == -1) {
    return this.domain;
  }
  this.domain = this.allDomains[(++index) % this.allDomains.length];
  return this.domain;
};


/**
 * Sets the style for the TraverseMath object to the next one in the list
 * restarting from the first, if necessary.
 * @return {string} The name of the newly set style.
 */
cvox.TraverseMath.prototype.cycleStyle = function() {
  this.initDomainsAndStyles();
  var index = this.allStyles.indexOf(this.style);
  if (index == -1) {
    return this.domain;
  }
  this.style = this.allStyles[(++index) % this.allStyles.length];
  return this.style;
};


/**
 *  Sets the domain for the TraverseMath object.
 * @param {string} domain Name of the domain.
 * @private
 */
cvox.TraverseMath.prototype.setDomain_ = function(domain) {
  if (this.allDomains.indexOf(domain) != -1) {
    this.domain = domain;
  } else {
    this.domain = 'default';
  }
};


/**
 *  Sets the style for the TraverseMath object.
 * @param {string} style Name of the style.
 * @private
 */
cvox.TraverseMath.prototype.setStyle_ = function(style) {
  if (this.allStyles.indexOf(style) != -1) {
    this.style = style;
  } else {
    this.style = 'default';
  }
};


/**
 * Gets the active node attached to the current document.
 * @return {Node} The active node, if it exists.
 */
cvox.TraverseMath.prototype.getAttachedActiveNode = function() {
  var node = this.activeNode;
  if (!node || node.nodeType != Node.ELEMENT_NODE) {
    return null;
  }
  var id = node.getAttribute('spanID');
  return document.getElementById(id);
};
