// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A semantic tree for MathML expressions.
 *
 * This file contains functionality to compute a semantic interpretation from a
 * given MathML expression. This is a very heuristic approach that assumes a
 * fairly simple default semantic which is suitable for K-12 and simple UG
 * mathematics.
 *
 */

goog.provide('cvox.SemanticTree');
goog.provide('cvox.SemanticTree.Node');

goog.require('cvox.DomUtil');
goog.require('cvox.SemanticAttr');
goog.require('cvox.SemanticUtil');


/**
 * Create an initial semantic tree.
 * @param {!Element} mml The original MathML node.
 * @constructor
 */
cvox.SemanticTree = function(mml) {
  /** ID counter.
   * @type {number}
   * @private
   */
  this.idCounter_ = 0;

  /** Original MathML tree.
   * @type {Node}
   */
  this.mathml = mml;

  /** @type {cvox.SemanticTree.Node} */
  this.root = this.parseMathml_(mml);
};


/**
 * @param {number} id Node id.
 * @constructor
 */
cvox.SemanticTree.Node = function(id) {
  /** @type {number} */
  this.id = id;

  /** @type {Array<Node>} */
  this.mathml = [];

  /** @type {cvox.SemanticTree.Node} */
  this.parent = null;

  /** @type {cvox.SemanticAttr.Type} */
  this.type = cvox.SemanticAttr.Type.UNKNOWN;

  /** @type {cvox.SemanticAttr.Role} */
  this.role = cvox.SemanticAttr.Role.UNKNOWN;

  /** @type {cvox.SemanticAttr.Font} */
  this.font = cvox.SemanticAttr.Font.UNKNOWN;

  /** @type {!Array<cvox.SemanticTree.Node>} */
  this.childNodes = [];

  /** @type {string} */
  this.textContent = '';

  /** Branch nodes can store additional nodes that can be useful.
   * E.g. a node of type FENCED can have the opening and closing fences here.
   * @type {!Array<cvox.SemanticTree.Node>}
   */
  this.contentNodes = [];
};


/**
 * Retrieve all subnodes (including the node itself) that satisfy a given
 * predicate.
 * @param {function(cvox.SemanticTree.Node): boolean} pred The predicate.
 * @return {!Array<cvox.SemanticTree.Node>} The nodes in the tree for which the
 *     predicate holds.
 */
cvox.SemanticTree.Node.prototype.querySelectorAll = function(pred) {
  var result = [];
  for (var i = 0, child; child = this.childNodes[i]; i++) {
    result = result.concat(child.querySelectorAll(pred));
  }
  if (pred(this)) {
    result.unshift(this);
  }
  return result;
};


 /**
  * Returns an XML representation of the tree.
  * @param {boolean=} brief If set attributes are omitted.
  * @return {Node} The XML representation of the tree.
  */
 cvox.SemanticTree.prototype.xml = function(brief) {
   var dp = new DOMParser();
   var xml = dp.parseFromString('<stree></stree>', 'text/xml');

   var xmlRoot = this.root.xml(xml, brief);
   xml.childNodes[0].appendChild(xmlRoot);

   return xml.childNodes[0];
 };


 /**
  * An XML tree representation of the current node.
  * @param {Document} xml The XML document.
  * @param {boolean=} brief If set attributes are omitted.
  * @return {Node} The XML representation of the node.
  */
 cvox.SemanticTree.Node.prototype.xml = function(xml, brief) {
   /**
    * Translates a list of nodes into XML representation.
    * @param {string} tag Name of the enclosing tag.
    * @param {!Array<!cvox.SemanticTree.Node>} nodes A list of nodes.
    * @return {Node} An XML representation of the node list.
    */
   var xmlNodeList = function(tag, nodes) {
     var xmlNodes = nodes.map(function(x) {return x.xml(xml, brief);});
     var tagNode = xml.createElement(tag);
     for (var i = 0, child; child = xmlNodes[i]; i++) {
       tagNode.appendChild(child);
     }
     return tagNode;
   };
   var node = xml.createElement(this.type);
   if (!brief) {
     this.xmlAttributes_(node);
   }
   node.textContent = this.textContent;
   if (this.contentNodes.length > 0) {
     node.appendChild(xmlNodeList('content', this.contentNodes));
   }
   if (this.childNodes.length > 0) {
     node.appendChild(xmlNodeList('children', this.childNodes));
   }
   return node;
 };


/**
  * Serializes the XML representation of the tree.
  * @param {boolean=} brief If set attributes are omitted.
 * @return {string} Serialized string.
 */
cvox.SemanticTree.prototype.toString = function(brief) {
  var xmls = new XMLSerializer();
  return xmls.serializeToString(this.xml(brief));
};


/**
 * Pretty print the XML representation of the tree.
 * @param {boolean=} brief If set attributes are omitted.
 * @return {string} The formatted string.
 */
cvox.SemanticTree.prototype.formatXml = function(brief) {
  var xml = this.toString(brief);
  return cvox.SemanticTree.formatXml(xml);
};


/**
 * Pretty prints an XML representation.
 * @param {string} xml The serialised XML string.
 * @return {string} The formatted string.
 */
cvox.SemanticTree.formatXml = function(xml) {
  var reg = /(>)(<)(\/*)/g;
  xml = xml.replace(reg, '$1\r\n$2$3');
  reg = /(>)(.+)(<c)/g;
  xml = xml.replace(reg, '$1\r\n$2\r\n$3');
  var formatted = '';
  var padding = '';
  xml.split('\r\n')
      .forEach(function(node) {
                 if (node.match(/.+<\/\w[^>]*>$/)) {
                   // Node with content.
                   formatted += padding + node + '\r\n';
                 } else if (node.match(/^<\/\w/)) {
                   if (padding) {
                     // Closing tag
                     padding = padding.slice(2);
                     formatted += padding + node + '\r\n';
                   }
                 } else if (node.match(/^<\w[^>]*[^\/]>.*$/)) {
                   // Opening tag
                   formatted += padding + node + '\r\n';
                   padding += '  ';
                 } else {
                   // Empty tag
                   formatted += padding + node + '\r\n';
                 }
               });
  return formatted;
};


/**
 * Serializes the XML representation of a node.
 * @param {boolean=} brief If attributes are to be omitted.
 * @return {string} Serialized string.
 */
cvox.SemanticTree.Node.prototype.toString = function(brief) {
  var xmls = new XMLSerializer();
  var dp = new DOMParser();
  var xml = dp.parseFromString('', 'text/xml');
  return xmls.serializeToString(this.xml(xml, brief));
};


/**
 * Adds attributes to the XML representation of the current node.
 * @param {Node} node The XML node.
 * @private
 */
cvox.SemanticTree.Node.prototype.xmlAttributes_ = function(node) {
  node.setAttribute('role', this.role);
  if (this.font != cvox.SemanticAttr.Font.UNKNOWN) {
    node.setAttribute('font', this.font);
  }
  node.setAttribute('id', this.id);
};


/** Creates a new node object.
 * @return {cvox.SemanticTree.Node} The newly created node.
 * @private
 */
cvox.SemanticTree.prototype.createNode_ = function() {
  return new cvox.SemanticTree.Node(this.idCounter_++);
};


/**
 * Replaces a node in the tree. Updates the root node if necessary.
 * @param {!cvox.SemanticTree.Node} oldNode The node to be replaced.
 * @param {!cvox.SemanticTree.Node} newNode The new node.
 * @private
 */
cvox.SemanticTree.prototype.replaceNode_ = function(oldNode, newNode) {
  var parent = oldNode.parent;
  if (!parent) {
    this.root = newNode;
    return;
  }
  parent.replaceChild_(oldNode, newNode);
};


/**
 * Updates the content of the node thereby possibly changing type and role.
 * @param {string} content The new content string.
 * @private
 */
cvox.SemanticTree.Node.prototype.updateContent_ = function(content) {
  // Remove superfluous whitespace!
  content = content.trim();
  if (this.textContent == content) {
    return;
  }
  var meaning = cvox.SemanticAttr.lookupMeaning(content);
  this.textContent = content;
  this.role = meaning.role;
  this.type = meaning.type;
  this.font = meaning.font;
};


/**
 * Adds MathML nodes to the node's store of MathML nodes if necessary only, as
 * we can not necessarily assume that the MathML of the content nodes and
 * children are all disjoint.
 * @param {Array<Node>} mmlNodes List of MathML nodes.
 * @private
 */
cvox.SemanticTree.Node.prototype.addMathmlNodes_ = function(mmlNodes) {
  for (var i = 0, mml; mml = mmlNodes[i]; i++) {
    if (this.mathml.indexOf(mml) == -1) {
      this.mathml.push(mml);
    }
  }
};


/**
 * Removes MathML nodes from the node's store of MathML nodes.
 * @param {Array<Node>} mmlNodes List of MathML nodes.
 * @private
 */
cvox.SemanticTree.Node.prototype.removeMathmlNodes_ = function(mmlNodes) {
  var mmlList = this.mathml;
  for (var i = 0, mml; mml = mmlNodes[i]; i++) {
    var index = mmlList.indexOf(mml);
    if (index != -1) {
      mmlList.splice(index, 1);
    }
  }
  this.mathml = mmlList;
};


/**
 * Appends a child to the node.
 * @param {cvox.SemanticTree.Node} child The new child.
 * @private
 */
cvox.SemanticTree.Node.prototype.appendChild_ = function(child) {
  this.childNodes.push(child);
  this.addMathmlNodes_(child.mathml);
  child.parent = this;
};


/**
 * Replaces a child node of the node.
 * @param {!cvox.SemanticTree.Node} oldNode The node to be replaced.
 * @param {!cvox.SemanticTree.Node} newNode The new node.
 * @private
 */
cvox.SemanticTree.Node.prototype.replaceChild_ = function(oldNode, newNode) {
  var index = this.childNodes.indexOf(oldNode);
  if (index == -1) {
    return;
  }
  newNode.parent = this;
  oldNode.parent = null;
  this.childNodes[index] = newNode;
  // To not mess up the order of MathML elements more than necessary, we only
  // remove and add difference lists. The hope is that we might end up with
  // little change.
  var removeMathml = oldNode.mathml.filter(
      function(x) {return newNode.mathml.indexOf(x) == -1;});
  var addMathml = newNode.mathml.filter(
      function(x) {return oldNode.mathml.indexOf(x) == -1;});
  this.removeMathmlNodes_(removeMathml);
  this.addMathmlNodes_(addMathml);
};


/**
 * Appends a content node to the node.
 * @param {cvox.SemanticTree.Node} node The new content node.
 * @private
 */
cvox.SemanticTree.Node.prototype.appendContentNode_ = function(node) {
  if (node) {
    this.contentNodes.push(node);
    this.addMathmlNodes_(node.mathml);
    node.parent = this;
  }
};


/**
 * Removes a content node from the node.
 * @param {cvox.SemanticTree.Node} node The content node to be removed.
 * @private
 */
cvox.SemanticTree.Node.prototype.removeContentNode_ = function(node) {
  if (node) {
    var index = this.contentNodes.indexOf(node);
    if (index != -1) {
      this.contentNodes.splice(index, 1);
    }
  }
};


/**
 * This is the main function that creates the semantic tree by recursively
 * parsing the initial MathML tree and bottom up assembling the tree.
 * @param {!Element} mml The MathML tree.
 * @return {!cvox.SemanticTree.Node} The root of the new tree.
 * @private
 */
cvox.SemanticTree.prototype.parseMathml_ = function(mml) {
  var children = cvox.DomUtil.toArray(mml.children);
  switch (cvox.SemanticUtil.tagName(mml)) {
    case 'MATH':
    case 'MROW':
    case 'MPADDED':
    case 'MSTYLE':
      children = cvox.SemanticUtil.purgeNodes(children);
      // Single child node, i.e. the row is meaningless.
      if (children.length == 1) {
        return this.parseMathml_(/** @type {!Element} */(children[0]));
      }
      // Case of a 'meaningful' row, even if they are empty.
      return this.processRow_(this.parseMathmlChildren_(children));
    break;
    case 'MFRAC':
      var newNode = this.makeBranchNode_(
          cvox.SemanticAttr.Type.FRACTION,
          [this.parseMathml_(children[0]), this.parseMathml_(children[1])],
          []);
      newNode.role = cvox.SemanticAttr.Role.DIVISION;
      return newNode;
      break;
    case 'MSUB':
    case 'MSUP':
    case 'MSUBSUP':
    case 'MOVER':
    case 'MUNDER':
    case 'MUNDEROVER':
      return this.makeLimitNode_(cvox.SemanticUtil.tagName(mml),
                                 this.parseMathmlChildren_(children));
      break;
    case 'MROOT':
      return this.makeBranchNode_(
          cvox.SemanticAttr.Type.ROOT,
          [this.parseMathml_(children[0]), this.parseMathml_(children[1])],
          []);
      break;
    case 'MSQRT':
      children = this.parseMathmlChildren_(
          cvox.SemanticUtil.purgeNodes(children));
      return this.makeBranchNode_(
          cvox.SemanticAttr.Type.SQRT, [this.processRow_(children)], []);
      break;
    case 'MTABLE':
      newNode = this.makeBranchNode_(
          cvox.SemanticAttr.Type.TABLE,
          this.parseMathmlChildren_(children), []);
      if (cvox.SemanticTree.tableIsMultiline_(newNode)) {
        this.tableToMultiline_(newNode);
        }
      return newNode;
      break;
    case 'MTR':
      newNode = this.makeBranchNode_(
          cvox.SemanticAttr.Type.ROW,
          this.parseMathmlChildren_(children), []);
      newNode.role = cvox.SemanticAttr.Role.TABLE;
      return newNode;
      break;
    case 'MTD':
      children = this.parseMathmlChildren_(
          cvox.SemanticUtil.purgeNodes(children));
      newNode = this.makeBranchNode_(
          cvox.SemanticAttr.Type.CELL, [this.processRow_(children)], []);
      newNode.role = cvox.SemanticAttr.Role.TABLE;
      return newNode;
      break;
    case 'MTEXT':
      var leaf = this.makeLeafNode_(mml);
      leaf.type = cvox.SemanticAttr.Type.TEXT;
      return leaf;
      break;
    // TODO (sorge) Role and font of multi-character and digits unicode strings.
    // TODO (sorge) Reclassify wrongly tagged numbers or identifiers.
    // TODO (sorge) Put this all in a single clean reclassification method.
    case 'MI':
      leaf = this.makeLeafNode_(mml);
      if (leaf.type == cvox.SemanticAttr.Type.UNKNOWN) {
        leaf.type = cvox.SemanticAttr.Type.IDENTIFIER;
      }
      return leaf;
      break;
    case 'MN':
      leaf = this.makeLeafNode_(mml);
      if (leaf.type == cvox.SemanticAttr.Type.UNKNOWN) {
        leaf.type = cvox.SemanticAttr.Type.NUMBER;
      }
      return leaf;
      break;
    case 'MO':
      leaf = this.makeLeafNode_(mml);
      if (leaf.type == cvox.SemanticAttr.Type.UNKNOWN) {
        leaf.type = cvox.SemanticAttr.Type.OPERATOR;
      }
      return leaf;
      break;
    // TODO (sorge) Do something useful with error and phantom symbols.
    default:
    // Ordinarilly at this point we should not get any other tag.
      return this.makeUnprocessed_(mml);
  }
};


/**
 * Parse a list of MathML nodes into the semantic tree.
 * @param {Array<Element>} mmls A list of MathML nodes.
 * @return {!Array<cvox.SemanticTree.Node>} The list of resulting semantic
 *     node.
 * @private
 */
cvox.SemanticTree.prototype.parseMathmlChildren_ = function(mmls) {
  var result = [];
  for (var i = 0, mml; mml = mmls[i]; i++) {
    result.push(this.parseMathml_(mml));
  }
  return result;
};

/**
 * Create a node that is to be processed at a later point in time.
 * @param {Node} mml The MathML tree.
 * @return {!cvox.SemanticTree.Node} The new node.
 * @private
 */
cvox.SemanticTree.prototype.makeUnprocessed_ = function(mml) {
  var node = this.createNode_();
  node.mathml = [mml];
  return node;
};


/**
 * Create an empty leaf node.
 * @return {!cvox.SemanticTree.Node} The new node.
 * @private
 */
cvox.SemanticTree.prototype.makeEmptyNode_ = function() {
  var node = this.createNode_();
  node.type = cvox.SemanticAttr.Type.EMPTY;
  return node;
};


/**
 * Create a leaf node.
 * @param {Node} mml The MathML tree.
 * @return {!cvox.SemanticTree.Node} The new node.
 * @private
 */
cvox.SemanticTree.prototype.makeLeafNode_ = function(mml) {
  var node = this.createNode_();
  node.mathml = [mml];
  node.updateContent_(mml.textContent);
  node.font = mml.getAttribute('mathvariant') || node.font;
  return node;
};


/**
 * Create a branching node.
 * @param {!cvox.SemanticAttr.Type} type The type of the node.
 * @param {!Array<cvox.SemanticTree.Node>} children The child nodes.
 * @param {!Array<cvox.SemanticTree.Node>} contentNodes The content Nodes.
 * @param {string=} content Content string if there is any.
 * @return {!cvox.SemanticTree.Node} The new node.
 * @private
 */
cvox.SemanticTree.prototype.makeBranchNode_ = function(
    type, children, contentNodes, content) {
  var node = this.createNode_();
  if (content) {
    node.updateContent_(content);
  }
  node.type = type;
  node.childNodes = children;
  node.contentNodes = contentNodes;
  children.concat(contentNodes)
      .forEach(
          function(x) {
            x.parent = node;
            node.addMathmlNodes_(x.mathml);
          });
  return node;
};


/**
 * Create a branching node for an implicit operation, currently assumed to
 * be of multiplicative type.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes The operands.
 * @return {!cvox.SemanticTree.Node} The new branch node.
 * @private
 */
cvox.SemanticTree.prototype.makeImplicitNode_ = function(nodes) {
  if (nodes.length == 1) {
    return nodes[0];
    }
  var operator = this.createNode_();
  // For now we assume this is a multiplication using invisible times.
  operator.updateContent_(cvox.SemanticAttr.invisibleTimes());
  var newNode = this.makeInfixNode_(nodes, operator);
  newNode.role = cvox.SemanticAttr.Role.IMPLICIT;
  return newNode;
};


/**
 * Create a branching node for an infix operation.
 * @param {!Array<cvox.SemanticTree.Node>} children The operands.
 * @param {!cvox.SemanticTree.Node} opNode The operator.
 * @return {!cvox.SemanticTree.Node} The new branch node.
 * @private
 */
cvox.SemanticTree.prototype.makeInfixNode_ = function(children, opNode) {
  return this.makeBranchNode_(
      cvox.SemanticAttr.Type.INFIXOP, children, [opNode], opNode.textContent);
};


/**
 * Creates a node of the specified type by collapsing the given node list into
 * one content (thereby concatenating the content of each node into a single
 * content string) with the inner node as a child.
 * @param {!cvox.SemanticTree.Node} inner The inner node.
 * @param {!Array<cvox.SemanticTree.Node>} nodeList List of nodes.
 * @param {!cvox.SemanticAttr.Type} type The new type of the node.
 * @return {!cvox.SemanticTree.Node} The new branch node.
 * @private
 */
cvox.SemanticTree.prototype.makeConcatNode_ = function(inner, nodeList, type) {
  if (nodeList.length == 0) {
    return inner;
  }
  var content = nodeList.map(function(x) {return x.textContent;}).join(' ');
  var newNode = this.makeBranchNode_(type, [inner], nodeList, content);
  if (nodeList.length > 0) {
    newNode.role = cvox.SemanticAttr.Role.MULTIOP;
  }
  return newNode;
};


/**
 * Wraps a node into prefix operators.
 * Example: + - a becomes (+ (- (a)))
 * Input: a  [+, -] ->  Output: content: '+ -', child: a
 * @param {!cvox.SemanticTree.Node} node The inner node.
 * @param {!Array<cvox.SemanticTree.Node>} prefixes Prefix operators
 * from the outermost to the innermost.
 * @return {!cvox.SemanticTree.Node} The new branch node.
 * @private
 */
cvox.SemanticTree.prototype.makePrefixNode_ = function(node, prefixes) {
  var negatives = cvox.SemanticTree.partitionNodes_(
      prefixes, cvox.SemanticTree.attrPred_('role', 'SUBTRACTION'));
  var newNode = this.makeConcatNode_(
      node, negatives.comp.pop(), cvox.SemanticAttr.Type.PREFIXOP);

  while (negatives.rel.length > 0) {
    newNode = this.makeConcatNode_(
        newNode, [negatives.rel.pop()], cvox.SemanticAttr.Type.PREFIXOP);
    newNode.role = cvox.SemanticAttr.Role.NEGATIVE;
    newNode = this.makeConcatNode_(
        newNode, negatives.comp.pop(), cvox.SemanticAttr.Type.PREFIXOP);
  }
  return newNode;
};


/**
 * Wraps a node into postfix operators.
 * Example: a - + becomes (((a) -) +)
 * Input: a  [-, +] ->  Output: content: '- +', child: a
 * @param {!cvox.SemanticTree.Node} node The inner node.
 * @param {!Array<cvox.SemanticTree.Node>} postfixes Postfix operators from
 * innermost to outermost.
 * @return {!cvox.SemanticTree.Node} The new branch node.
 * @private
 */
cvox.SemanticTree.prototype.makePostfixNode_ = function(node, postfixes) {
  return this.makeConcatNode_(
      node, postfixes, cvox.SemanticAttr.Type.POSTFIXOP);
};


// TODO (sorge) Separate out interspersed text before the relations in row
// heuristic otherwise we get them as implicit operations!
// Currently we handle that later in the rules, which is rather messy.
/**
 * Processes a list of nodes, combining expressions by delimiters, tables,
 * punctuation sequences, function/big operator/integral applications to
 * generate a syntax tree with relation and operator precedence.
 *
 * This is the main heuristic to rewrite a flat row of terms into a meaningful
 * term tree.
 * @param {!Array<cvox.SemanticTree.Node>} nodes The list of nodes.
 * @return {!cvox.SemanticTree.Node} The root node of the syntax tree.
 * @private
 */
cvox.SemanticTree.prototype.processRow_ = function(nodes) {
  if (nodes.length == 0) {
    return this.makeEmptyNode_();
  }
  nodes = this.getFencesInRow_(nodes);
  nodes = this.processTablesInRow_(nodes);
  nodes = this.getPunctuationInRow_(nodes);
  nodes = this.getFunctionsInRow_(nodes);
  return this.processRelationsInRow_(nodes);
};


/**
 * Constructs a syntax tree with relation and operator precedence from a list
 * of nodes.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes The list of nodes.
 * @return {!cvox.SemanticTree.Node} The root node of the syntax tree.
 * @private
 */
cvox.SemanticTree.prototype.processRelationsInRow_ = function(nodes) {
  var partition = cvox.SemanticTree.partitionNodes_(
      nodes, cvox.SemanticTree.attrPred_('type', 'RELATION'));
  var firstRel = partition.rel[0];

  if (!firstRel) {
    return this.processOperationsInRow_(nodes);
  }
  if (nodes.length == 1) {
    return nodes[0];
  }
  var children = partition.comp.map(
      goog.bind(this.processOperationsInRow_, this));
  if (partition.rel.every(
         function(x) {return x.textContent == firstRel.textContent;})) {
    return this.makeBranchNode_(
        cvox.SemanticAttr.Type.RELSEQ, children, partition.rel,
        firstRel.textContent);
  }
  return this.makeBranchNode_(
      cvox.SemanticAttr.Type.MULTIREL, children, partition.rel);
};


/**
 * Constructs a syntax tree with operator precedence from a list nodes.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes The list of nodes.
 * @return {!cvox.SemanticTree.Node} The root node of the syntax tree.
 * @private
 */
cvox.SemanticTree.prototype.processOperationsInRow_ = function(nodes) {
  if (nodes.length == 0) {
    return this.makeEmptyNode_();
  }
  if (nodes.length == 1) {
    return nodes[0];
  }

  var prefix = [];
  while (nodes.length > 0 &&
      nodes[0].type == cvox.SemanticAttr.Type.OPERATOR) {
    prefix.push(nodes.shift());
  }
  // Pathological case: only operators in row.
  if (nodes.length == 0) {
    return this.makePrefixNode_(prefix.pop(), prefix);
  }
  if (nodes.length == 1) {
    return this.makePrefixNode_(nodes[0], prefix);
  }

  var split = cvox.SemanticTree.sliceNodes_(
      nodes, cvox.SemanticTree.attrPred_('type', 'OPERATOR'));
  // At this point, we know that split.head is not empty!
  var node = this.makePrefixNode_(
      this.makeImplicitNode_(
          /** @type {!Array<!cvox.SemanticTree.Node>} */ (split.head)),
      prefix);
  if (!split.div) {
    return node;
  }
  return this.makeOperationsTree_(split.tail, node, split.div);
};


/**
 * Recursively constructs syntax tree with operator precedence from a list nodes
 * given a initial root node.
 * @param {!Array<cvox.SemanticTree.Node>} nodes The list of nodes.
 * @param {!cvox.SemanticTree.Node} root Initial tree.
 * @param {!cvox.SemanticTree.Node} lastop Last operator that has not been
 * processed yet.
 * @param {Array<cvox.SemanticTree.Node>=} prefixes Operator nodes that will
 * become prefix operation (or postfix in case they come after last operand).
 * @return {!cvox.SemanticTree.Node} The root node of the syntax tree.
 * @private
 */
cvox.SemanticTree.prototype.makeOperationsTree_ = function(
    nodes, root, lastop, prefixes) {
  prefixes = prefixes || [];

  if (nodes.length == 0) {
    // Left over prefixes become postfixes.
    prefixes.unshift(lastop);
    if (root.type == cvox.SemanticAttr.Type.INFIXOP) {
      // We assume prefixes bind stronger than postfixes.
      var node = this.makePostfixNode_(
          // Here we know that the childNodes are not empty!
          /** @type {!cvox.SemanticTree.Node} */ (root.childNodes.pop()),
          prefixes);
      root.appendChild_(node);
      return root;
    }
    return this.makePostfixNode_(root, prefixes);
  }

  var split = cvox.SemanticTree.sliceNodes_(
      nodes, cvox.SemanticTree.attrPred_('type', 'OPERATOR'));

  if (split.head.length == 0) {
    prefixes.push(split.div);
    return this.makeOperationsTree_(split.tail, root, lastop, prefixes);
  }

  var node = this.makePrefixNode_(
      this.makeImplicitNode_(split.head), prefixes);
  var newNode = this.appendOperand_(root, lastop, node);
  if (!split.div) {
    return newNode;
  }

  return this.makeOperationsTree_(split.tail, newNode, split.div, []);
};

// TODO (sorge) The following four functions could be combined into
// a single one. Currently it is clearer the way it is, though.
/**
 * Appends an operand at the right place in an operator tree.
 * @param {!cvox.SemanticTree.Node} root The operator tree.
 * @param {!cvox.SemanticTree.Node} op The operator node.
 * @param {!cvox.SemanticTree.Node} node The node to be added.
 * @return {!cvox.SemanticTree.Node} The modified root node.
 * @private
 */
cvox.SemanticTree.prototype.appendOperand_ = function(root, op, node) {
  // In general our operator tree will have the form that additions and
  // subtractions are stacked, while multiplications are subordinate.
  if (root.type != cvox.SemanticAttr.Type.INFIXOP) {
    return this.makeInfixNode_([root, node], op);
  }
  if (this.appendExistingOperator_(root, op, node)) {
    return root;
  }
  return op.role == cvox.SemanticAttr.Role.MULTIPLICATION ?
      this.appendMultiplicativeOp_(root, op, node) :
          this.appendAdditiveOp_(root, op, node);
};


/**
 * Appends a multiplicative operator and operand.
 * @param {!cvox.SemanticTree.Node} root The root node.
 * @param {!cvox.SemanticTree.Node} op The operator node.
 * @param {!cvox.SemanticTree.Node} node The operand node to be added.
 * @return {!cvox.SemanticTree.Node} The modified root node.
 * @private
 */
cvox.SemanticTree.prototype.appendMultiplicativeOp_ = function(root, op, node) {
  var lastRoot = root;
  var lastChild = root.childNodes[root.childNodes.length - 1];
  while (lastChild && lastChild.type == cvox.SemanticAttr.Type.INFIXOP) {
    lastRoot = lastChild;
    lastChild = lastRoot.childNodes[root.childNodes.length - 1];
  }
  var newNode = this.makeInfixNode_([lastRoot.childNodes.pop(), node], op);
  lastRoot.appendChild_(newNode);
  return root;
};


/**
 * Appends an additive/substractive operator and operand.
 * @param {!cvox.SemanticTree.Node} root The old root node.
 * @param {!cvox.SemanticTree.Node} op The operator node.
 * @param {!cvox.SemanticTree.Node} node The operand node to be added.
 * @return {!cvox.SemanticTree.Node} The new root node.
 * @private
 */
cvox.SemanticTree.prototype.appendAdditiveOp_ = function(root, op, node) {
  return this.makeInfixNode_([root, node], op);
};


/**
 * Adds an operand to an operator node if it is the continuation of an existing
 * operation.
 * @param {!cvox.SemanticTree.Node} root The root node.
 * @param {!cvox.SemanticTree.Node} op The operator node.
 * @param {!cvox.SemanticTree.Node} node The operand node to be added.
 * @return {boolean} True if operator was successfully appended.
 * @private
 */
cvox.SemanticTree.prototype.appendExistingOperator_ = function(root, op, node) {
  if (!root || root.type != cvox.SemanticAttr.Type.INFIXOP) {
    return false;
  }
  if (root.textContent == op.textContent) {
    root.appendContentNode_(op);
    root.appendChild_(node);
    return true;
  }
  this.appendExistingOperator_(
      // Again, if this is an INFIXOP node, we know it has a child!
      /** @type {!cvox.SemanticTree.Node} */
      (root.childNodes[root.childNodes.length - 1]),
      op, node);
  return false;
};


// TODO (sorge) The following procedure needs a rational reconstruction. It
// contains a number of similar cases which should be combined.
/**
 * Combines delimited expressions in a list of nodes.
 *
 * The basic idea of the heuristic is as follows:
 * 1. Opening and closing delimiters are matched regardless of the actual shape
 *    of the fence. These are turned into fenced nodes.
 * 2. Neutral fences are matched only with neutral fences of the same shape.
 * 3. For a collection of unmatched neutral fences we try to get a maximum
 *    number of matching fences. E.g. || a|b || would be turned into a fenced
 *    node with fences || and content a|b.
 * 4. Any remaining unmatched delimiters are turned into punctuation nodes.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes The list of nodes.
 * @return {!Array<!cvox.SemanticTree.Node>} The new list of nodes.
 * @private
 */
cvox.SemanticTree.prototype.getFencesInRow_ = function(nodes) {
  var partition = cvox.SemanticTree.partitionNodes_(
      nodes, cvox.SemanticTree.attrPred_('type', 'FENCE'));
  var felem = partition.comp.shift();
  return this.processFences_(partition.rel, partition.comp, [], [felem]);
};


/**
 * Recursively processes a list of nodes and combines all the fenced expressions
 * into single nodes. It also processes singular fences, building expressions
 * that are only fenced left or right.
 * @param {!Array<cvox.SemanticTree.Node>} fences FIFO queue of fence nodes.
 * @param {!Array<!Array<cvox.SemanticTree.Node>>} content FIFO queue content
 *     between fences.
 * @param {!Array<cvox.SemanticTree.Node>} openStack LIFO stack of open fences.
 * @param {!Array<!Array<cvox.SemanticTree.Node>>} contentStack LIFO stack of
 *     content between fences yet to be processed.
 * @return {!Array<cvox.SemanticTree.Node>} A list of nodes with all fenced
 *     expressions processed.
 * @private
 */
cvox.SemanticTree.prototype.processFences_ = function(
  fences, content, openStack, contentStack) {
  // Base case 1: Everything is used up.
  if (fences.length == 0 && openStack.length == 0) {
    return contentStack[0];
    }
  var openPred = cvox.SemanticTree.attrPred_('role', 'OPEN');
  // Base case 2: Only open and neutral fences are left on the stack.
  if (fences.length == 0) {
    // Basic idea:
    // - make punctuation nodes from open fences
    // - combine as many neutral fences as possible, if the are not separated by
    //   open fences.
    // The idea is to allow for things like case statements etc. and not bury
    // them inside a neutral fenced expression.
    //
    // 0. We process the list from left to right. Hence the first element on the
    //    content stack are actually left most elements in the expression.
    // 1. Slice at open fence.
    // 2. On tail optimize for neutral fences.
    // 3. Repeat until fence stack is exhausted.
    // Push rightmost elements onto the result.
    var result = contentStack.shift();
    while (openStack.length > 0) {
      if (openPred(openStack[0])) {
        var firstOpen = openStack.shift();
        cvox.SemanticTree.fenceToPunct_(firstOpen);
        result.push(firstOpen);
      } else {
        var split = cvox.SemanticTree.sliceNodes_(openStack, openPred);
        var cutLength = split.head.length - 1;
        var innerNodes = this.processNeutralFences_(
            split.head, contentStack.slice(0, cutLength));
        contentStack = contentStack.slice(cutLength);
        //var rightContent = contentStack.shift();
        result.push.apply(result, innerNodes);
        //result.push.apply(result, rightContent);
        if (split.div) {
          split.tail.unshift(split.div);
        }
        openStack = split.tail;
      }
      result.push.apply(result, contentStack.shift());
    }
    return result;
  }
  var lastOpen = openStack[openStack.length - 1];
  var firstRole = fences[0].role;
  // General opening case.
  // Either we have an open fence.
  if (firstRole == cvox.SemanticAttr.Role.OPEN ||
      // Or we have a neutral fence that does not have a counter part.
          (firstRole == cvox.SemanticAttr.Role.NEUTRAL &&
              (!lastOpen ||
                  fences[0].textContent != lastOpen.textContent))) {
    openStack.push(fences.shift());
    contentStack.push(content.shift());
    return this.processFences_(fences, content, openStack, contentStack);
  }
  // General closing case.
  if (lastOpen && (
      // Closing fence for some opening fence.
      (firstRole == cvox.SemanticAttr.Role.CLOSE &&
          lastOpen.role == cvox.SemanticAttr.Role.OPEN) ||
              // Netural fence with exact counter part.
              (firstRole == cvox.SemanticAttr.Role.NEUTRAL &&
                  fences[0].textContent == lastOpen.textContent))) {
    var fenced = this.makeHorizontalFencedNode_(
        openStack.pop(), fences.shift(), contentStack.pop());
    contentStack.push(contentStack.pop().concat([fenced], content.shift()));
    return this.processFences_(fences, content, openStack, contentStack);
  }
  // Closing with a neutral fence on the stack.
  if (lastOpen && firstRole == cvox.SemanticAttr.Role.CLOSE &&
      lastOpen.role == cvox.SemanticAttr.Role.NEUTRAL &&
          openStack.some(openPred)) {
    // Steps of the algorithm:
    // 1. Split list at right most opening bracket.
    // 2. Cut content list at corresponding length.
    // 3. Optimise the neutral fences.
    // 4. Make fenced node.
    //
    // Careful, this reverses openStack!
    var split = cvox.SemanticTree.sliceNodes_(openStack, openPred, true);
    // We know that
    // (a) div & tail exist,
    // (b) all are combined in this step into a single fenced node,
    // (c) head is the new openStack,
    // (d) the new contentStack is remainder of contentStack + new fenced node +
    // shift of content.
    var rightContent = contentStack.pop();
    var cutLength = contentStack.length - split.tail.length + 1;
    var innerNodes = this.processNeutralFences_(
        split.tail, contentStack.slice(cutLength));
    contentStack = contentStack.slice(0, cutLength);
    var fenced = this.makeHorizontalFencedNode_(
        split.div, fences.shift(),
        contentStack.pop().concat(innerNodes, rightContent));
    contentStack.push(contentStack.pop().concat([fenced], content.shift()));
    return this.processFences_(fences, content, split.head, contentStack);
  }
  // Final Case: A singular closing fence.
  // We turn the fence into a punctuation.
  var fenced = fences.shift();
  cvox.SemanticTree.fenceToPunct_(fenced);
  contentStack.push(contentStack.pop().concat([fenced], content.shift()));
  return this.processFences_(fences, content, openStack, contentStack);
};


// TODO (sorge) The following could be done with linear programming.
/**
 * Trys to combine neutral fences as much as possible.
 * @param {!Array<!cvox.SemanticTree.Node>} fences A list of neutral fences.
 * @param {!Array<!Array<cvox.SemanticTree.Node>>} content Intermediate
 *     content. Observe that |content| = |fences| - 1
 * @return {!Array<cvox.SemanticTree.Node>} List of node with fully fenced
 *     nodes.
 * @private
 */
cvox.SemanticTree.prototype.processNeutralFences_ = function(fences, content) {
  if (fences.length == 0) {
    return fences;
  }
  if (fences.length == 1) {
    cvox.SemanticTree.fenceToPunct_(fences[0]);
    return fences;
    }
  var firstFence = fences.shift();
  var split = cvox.SemanticTree.sliceNodes_(
      fences, function(x) {return x.textContent == firstFence.textContent;});
  if (!split.div) {
    cvox.SemanticTree.fenceToPunct_(firstFence);
    var restContent = content.shift();
    restContent.unshift(firstFence);
    return restContent.concat(this.processNeutralFences_(fences, content));
  }
  var newContent = this.combineFencedContent_(
      firstFence, split.div, split.head, content);
  if (split.tail.length > 0) {
    var leftContent = newContent.shift();
    var result = this.processNeutralFences_(split.tail, newContent);
    return leftContent.concat(result);
  }
  return newContent[0];
};


/**
 * Combines nodes framed by two matching fences using the given content.
 * Example: leftFence: [, rightFence: ], midFences: |, |
 *          content: c1, c2, c3, c4, ... cn
 *          return: [c1 | c2 | c3 ], c4, ... cn
 * @param {!cvox.SemanticTree.Node} leftFence The left fence.
 * @param {!cvox.SemanticTree.Node} rightFence The right fence.
 * @param {!Array<cvox.SemanticTree.Node>} midFences A list of intermediate
 *     fences.
 * @param {!Array<!Array<cvox.SemanticTree.Node>>} content Intermediate
 *     content. Observe that |content| = |fences| - 1 + k where k >= 0 is the
 *     remainder.
 * @return {!Array<!Array<cvox.SemanticTree.Node>>} List of content nodes
 *     where the first is the fully fenced node wrt. the given left and right
 *     fence.
 * @private
 */
cvox.SemanticTree.prototype.combineFencedContent_ = function(
    leftFence, rightFence, midFences, content) {

  if (midFences.length == 0) {
    var fenced = this.makeHorizontalFencedNode_(
        leftFence, rightFence, content.shift());
    content.unshift(fenced);
    return content;
  }

  var leftContent = content.shift();
  var cutLength = midFences.length - 1;
  var midContent = content.slice(0, cutLength);
  content = content.slice(cutLength);
  var rightContent = content.shift();
  var innerNodes = this.processNeutralFences_(midFences, midContent);
  leftContent.push.apply(leftContent, innerNodes);
  leftContent.push.apply(leftContent, rightContent);
  var fenced = this.makeHorizontalFencedNode_(
      leftFence, rightFence, leftContent);
  if (content.length > 0) {
    content[0].unshift(fenced);
  } else {
    content = [[fenced]];
  }
  return content;
 };


/**
 * Rewrite fences into punctuation. This is done with any "leftover" fence.
 * @param {cvox.SemanticTree.Node} fence Fence.
 * @private
 */
cvox.SemanticTree.fenceToPunct_ = function(fence) {
  fence.type = cvox.SemanticAttr.Type.PUNCTUATION;
  switch (fence.role) {
    case cvox.SemanticAttr.Role.NEUTRAL:
    fence.role = cvox.SemanticAttr.Role.VBAR;
    break;
    case cvox.SemanticAttr.Role.OPEN:
    fence.role = cvox.SemanticAttr.Role.OPENFENCE;
    break;
    case cvox.SemanticAttr.Role.CLOSE:
    fence.role = cvox.SemanticAttr.Role.CLOSEFENCE;
    break;
  }
};


/**
 * Create a fenced node.
 * @param {cvox.SemanticTree.Node} ofence Opening fence.
 * @param {cvox.SemanticTree.Node} cfence Closing fence.
 * @param {!Array<cvox.SemanticTree.Node>} content The content
 *     between the fences.
 * @return {!cvox.SemanticTree.Node} The new node.
 * @private
 */
cvox.SemanticTree.prototype.makeHorizontalFencedNode_ = function(
    ofence, cfence, content) {
  var childNode = this.processRow_(content);
  var newNode = this.makeBranchNode_(
      cvox.SemanticAttr.Type.FENCED, [childNode], [ofence, cfence]);
  if (ofence.role == cvox.SemanticAttr.Role.OPEN) {
    newNode.role = cvox.SemanticAttr.Role.LEFTRIGHT;
  } else {
    newNode.role = ofence.role;
  }
  return newNode;
};


/**
 * Combines sequences of punctuated expressions in a list of nodes.
 * @param {!Array<cvox.SemanticTree.Node>} nodes The list of nodes.
 * @return {!Array<cvox.SemanticTree.Node>} The new list of nodes.
 * @private
 */
cvox.SemanticTree.prototype.getPunctuationInRow_ = function(nodes) {
  // For now we just make a punctuation node with a particular role. This is
  // similar to an mrow. The only exception are ellipses, which we assume to be
  // in lieu of identifiers.
  // In addition we keep the single punctuation nodes as content.
  var partition = cvox.SemanticTree.partitionNodes_(
      nodes, function(x) {
        return cvox.SemanticTree.attrPred_('type', 'PUNCTUATION')(x) &&
            !cvox.SemanticTree.attrPred_('role', 'ELLIPSIS')(x);});
  if (partition.rel.length == 0) {
    return nodes;
  }
  var newNodes = [];
  var firstComp = partition.comp.shift();
  if (firstComp.length > 0) {
    newNodes.push(this.processRow_(firstComp));
  }
  var relCounter = 0;
  while (partition.comp.length > 0) {
    newNodes.push(partition.rel[relCounter++]);
    firstComp = partition.comp.shift();
    if (firstComp.length > 0) {
      newNodes.push(this.processRow_(firstComp));
    }
  }
  return [this.makePunctuatedNode_(newNodes, partition.rel)];
};


/**
 * Create a punctuated node.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes List of all nodes separated
 * by punctuations.
 * @param {!Array<!cvox.SemanticTree.Node>} punctuations List of all separating
 * punctations. Observe that punctations is a subset of nodes.
 * @return {!cvox.SemanticTree.Node}
 * @private
 */
cvox.SemanticTree.prototype.makePunctuatedNode_ = function(
    nodes, punctuations) {
  var newNode = this.makeBranchNode_(
      cvox.SemanticAttr.Type.PUNCTUATED, nodes, punctuations);

  if (punctuations.length == 1 &&
      nodes[0].type == cvox.SemanticAttr.Type.PUNCTUATION) {
    newNode.role = cvox.SemanticAttr.Role.STARTPUNCT;
  } else if (punctuations.length == 1 &&
      nodes[nodes.length - 1].type == cvox.SemanticAttr.Type.PUNCTUATION) {
    newNode.role = cvox.SemanticAttr.Role.ENDPUNCT;
  } else {
    newNode.role = cvox.SemanticAttr.Role.SEQUENCE;
  }
  return newNode;
};


/**
 * Creates a limit node from a sub/superscript or over/under node if the central
 * element is a big operator. Otherwise it creates the standard elements.
 * @param {string} mmlTag The tag name of the original node.
 * @param {!Array<!cvox.SemanticTree.Node>} children The children of the
 *     original node.
 * @return {!cvox.SemanticTree.Node} The newly created limit node.
 * @private
 */
cvox.SemanticTree.prototype.makeLimitNode_ = function(mmlTag, children) {
  var center = children[0];
  var isFunction = cvox.SemanticTree.attrPred_('type', 'FUNCTION')(center);
  // TODO (sorge) Put this into a single function.
  var isLimit = cvox.SemanticTree.attrPred_('type', 'LARGEOP')(center) ||
      cvox.SemanticTree.attrPred_('type', 'LIMBOTH')(center) ||
      cvox.SemanticTree.attrPred_('type', 'LIMLOWER')(center) ||
      cvox.SemanticTree.attrPred_('type', 'LIMUPPER')(center) ||
      (isFunction && cvox.SemanticTree.attrPred_('role', 'LIMFUNC')(center));
  var type = cvox.SemanticAttr.Type.UNKNOWN;
  // TODO (sorge) Make use of the difference in information on sub vs under etc.
  if (isLimit) {
    switch (mmlTag) {
      case 'MSUB':
      case 'MUNDER':
      type = cvox.SemanticAttr.Type.LIMLOWER;
      break;
      case 'MSUP':
      case 'MOVER':
      type = cvox.SemanticAttr.Type.LIMUPPER;
      break;
      case 'MSUBSUP':
      case 'MUNDEROVER':
      type = cvox.SemanticAttr.Type.LIMBOTH;
      break;
    }
  } else {
    switch (mmlTag) {
      case 'MSUB':
      type = cvox.SemanticAttr.Type.SUBSCRIPT;
      break;
      case 'MSUP':
      type = cvox.SemanticAttr.Type.SUPERSCRIPT;
      break;
      case 'MSUBSUP':
      var innerNode = this.makeBranchNode_(cvox.SemanticAttr.Type.SUBSCRIPT,
                                      [center, children[1]], []);
      innerNode.role = center.role;
      children = [innerNode, children[2]];
      type = cvox.SemanticAttr.Type.SUPERSCRIPT;
      break;
      case 'MOVER':
      type = cvox.SemanticAttr.Type.OVERSCORE;
      break;
      case 'MUNDER':
      type = cvox.SemanticAttr.Type.UNDERSCORE;
      break;
      case 'MUNDEROVER':
      default:
      var innerNode = this.makeBranchNode_(cvox.SemanticAttr.Type.UNDERSCORE,
                                      [center, children[1]], []);
      innerNode.role = center.role;
      children = [innerNode, children[2]];
      type = cvox.SemanticAttr.Type.OVERSCORE;
      break;
    }
  }
  var newNode = this.makeBranchNode_(type, children, []);
  newNode.role = center.role;
  return newNode;
};


/**
 * Recursive method to accumulate function expressions.
 *
 * The idea is to process functions in a row from left to right combining them
 * with there arguments. Thereby we take the notion of a function rather broadly
 * as a functional expressions that consists of a prefix and some arguments.
 * In particular we distinguish four types of functional expressions:
 * - integral: Integral expression.
 * - bigop: A big operator expression like a sum.
 * - prefix: A well defined prefix function such as sin, cos or a limit
 *           functions like lim, max.
 * - simple: An expression consisting of letters that are potentially a function
 *           symbol. If we have an explicit function application symbol
 *           following the expression we turn into a prefix function. Otherwise
 *           we decide heuristically if we could have a function application.
 * @param {!Array<cvox.SemanticTree.Node>} restNodes The remainder list of
 *     nodes.
 * @param {!Array<cvox.SemanticTree.Node>=} result The result node list.
 * @return {!Array<!cvox.SemanticTree.Node>} The fully processed list.
 * @private
 */
cvox.SemanticTree.prototype.getFunctionsInRow_ = function(restNodes, result) {
  result = result || [];
  // Base case.
  if (restNodes.length == 0) {
    return result;
  }
  var firstNode = /** @type {!cvox.SemanticTree.Node} */ (restNodes.shift());
  var heuristic = cvox.SemanticTree.classifyFunction_(firstNode, restNodes);
  // First node is not a function node.
  if (!heuristic) {
    result.push(firstNode);
    return this.getFunctionsInRow_(restNodes, result);
  }
  // Combine functions in the rest of the row.
  var processedRest = this.getFunctionsInRow_(restNodes, []);
  var newRest = this.getFunctionArgs_(firstNode, processedRest, heuristic);
  return result.concat(newRest);
};


/**
 * Classifies a function wrt. the heuristic that should be applied.
 * @param {!cvox.SemanticTree.Node} funcNode The node to be classified.
 * @param {!Array<cvox.SemanticTree.Node>} restNodes The remainder list of
 *     nodes. They can useful to look ahead if there is an explicit function
 *     application. If there is one, it will be destructively removed!
 * @return {!string} The string specifying the heuristic.
 * @private
 */
cvox.SemanticTree.classifyFunction_ = function(funcNode, restNodes) {
  //  We do not allow double function application. This is not lambda calculus!
  if (funcNode.type == cvox.SemanticAttr.Type.APPL ||
      funcNode.type == cvox.SemanticAttr.Type.BIGOP ||
          funcNode.type == cvox.SemanticAttr.Type.INTEGRAL) {
    return '';
  }
  // Find and remove explicit function applications.
  // We now treat funcNode as a prefix function, regardless of what its actual
  // content is.
  if (restNodes[0] &&
      restNodes[0].textContent == cvox.SemanticAttr.functionApplication()) {
    // Remove explicit function application. This is destructive on the
    // underlying list.
    restNodes.shift();
    cvox.SemanticTree.propagatePrefixFunc_(funcNode);
    return 'prefix';
  }
  switch (funcNode.role) {
    case cvox.SemanticAttr.Role.INTEGRAL:
    return 'integral';
    break;
    case cvox.SemanticAttr.Role.SUM:
    return 'bigop';
    break;
    case cvox.SemanticAttr.Role.PREFIXFUNC:
    case cvox.SemanticAttr.Role.LIMFUNC:
    return 'prefix';
    break;
    default:
    if (funcNode.type == cvox.SemanticAttr.Type.IDENTIFIER) {
      return 'simple';
    }
  }
  return '';
};


/**
 * Propagates a prefix function role in a node.
 * @param {cvox.SemanticTree.Node} funcNode The node whose role is to be
 * rewritten.
 * @private
 */
cvox.SemanticTree.propagatePrefixFunc_ = function(funcNode) {
  if (funcNode) {
    funcNode.role = cvox.SemanticAttr.Role.PREFIXFUNC;
    cvox.SemanticTree.propagatePrefixFunc_(funcNode.childNodes[0]);
  }
};


/**
 * Computes the arguments for a function from a list of nodes depending on the
 * given heuristic.
 * @param {!cvox.SemanticTree.Node} func A function node.
 * @param {!Array<cvox.SemanticTree.Node>} rest List of nodes to choose
 *     arguments from.
 * @param {string} heuristic The heuristic to follow.
 * @return {!Array<!cvox.SemanticTree.Node>} The function and the remainder of
 *     the rest list.
 * @private
 */
cvox.SemanticTree.prototype.getFunctionArgs_ = function(func, rest, heuristic) {
  switch (heuristic) {
    case 'integral':
    var components = this.getIntegralArgs_(rest);
    var integrand = this.processRow_(components.integrand);
    var funcNode = this.makeIntegralNode_(func, integrand, components.intvar);
    components.rest.unshift(funcNode);
    return components.rest;
    break;
    case 'prefix':
    if (rest[0] && rest[0].type == cvox.SemanticAttr.Type.FENCED) {
      funcNode = this.makeFunctionNode_(
          func, /** @type {!cvox.SemanticTree.Node} */ (rest.shift()));
      rest.unshift(funcNode);
      return rest;
    }
    case 'bigop':
    var partition = cvox.SemanticTree.sliceNodes_(
        rest, cvox.SemanticTree.prefixFunctionBoundary_);
    var arg = this.processRow_(partition.head);
    if (heuristic == 'prefix') {
      funcNode = this.makeFunctionNode_(func, arg);
    } else {
      funcNode = this.makeBigOpNode_(func, arg);
    }
    if (partition.div) {
      partition.tail.unshift(partition.div);
    }
    partition.tail.unshift(funcNode);
    return partition.tail;
    break;
    case 'simple':
    if (rest.length == 0) {
      return [func];
    }
    var firstArg = rest[0];
    if (firstArg.type == cvox.SemanticAttr.Type.FENCED &&
        firstArg.role != cvox.SemanticAttr.Role.NEUTRAL &&
            this.simpleFunctionHeuristic_(firstArg)) {
      funcNode = this.makeFunctionNode_(
          func, /** @type {!cvox.SemanticTree.Node} */ (rest.shift()));
      rest.unshift(funcNode);
      return rest;
    }
    rest.unshift(func);
    return rest;
    break;
  }
  return [];
};


/**
 * Tail recursive function to obtain integral arguments.
 * @param {!Array<cvox.SemanticTree.Node>} nodes List of nodes to take
 * arguments from.
 * @param {Array<cvox.SemanticTree.Node>=} args List of integral arguments.
 * @return {{integrand: !Array<cvox.SemanticTree.Node>,
 *     intvar: cvox.SemanticTree.Node,
 *     rest: !Array<cvox.SemanticTree.Node>}}
 *     Result split into integrand, integral variable and the remaining
 *     elements.
 * @private
 */
cvox.SemanticTree.prototype.getIntegralArgs_ = function(nodes, args) {
  args = args || [];
  if (nodes.length == 0) {
    return {integrand: args, intvar: null, rest: nodes};
  }
  var firstNode = nodes[0];
  if (cvox.SemanticTree.generalFunctionBoundary_(firstNode)) {
    return {integrand: args, intvar: null, rest: nodes};
  }
  if (cvox.SemanticTree.integralDxBoundarySingle_(firstNode)) {
    return {integrand: args, intvar: firstNode, rest: nodes.slice(1)};
  }
  if (nodes[1] && cvox.SemanticTree.integralDxBoundary_(firstNode, nodes[1])) {
    var comma = this.createNode_();
    comma.updateContent_(cvox.SemanticAttr.invisibleComma());
    var intvar = this.makePunctuatedNode_(
        [firstNode, comma, nodes[1]], [comma]);
    intvar.role = cvox.SemanticAttr.Role.INTEGRAL;
    return {integrand: args, intvar: intvar, rest: nodes.slice(2)};
  }
  args.push(nodes.shift());
  return this.getIntegralArgs_(nodes, args);
};


/**
 * Create a function node.
 * @param {!cvox.SemanticTree.Node} func The function operator.
 * @param {!cvox.SemanticTree.Node} arg The argument.
 * @return {!cvox.SemanticTree.Node} The new function node.
 * @private
 */
cvox.SemanticTree.prototype.makeFunctionNode_ = function(func, arg) {
  var applNode = this.createNode_();
  applNode.updateContent_(cvox.SemanticAttr.functionApplication());
  applNode.type = cvox.SemanticAttr.Type.PUNCTUATION;
  applNode.role = cvox.SemanticAttr.Role.APPLICATION;
  var newNode = this.makeBranchNode_(cvox.SemanticAttr.Type.APPL, [func, arg],
                                [applNode]);
  newNode.role = func.role;
  return newNode;
};


/**
 * Create a big operator node.
 * @param {!cvox.SemanticTree.Node} bigOp The big operator.
 * @param {!cvox.SemanticTree.Node} arg The argument.
 * @return {!cvox.SemanticTree.Node} The new big operator node.
 * @private
 */
cvox.SemanticTree.prototype.makeBigOpNode_ = function(bigOp, arg) {
  var newNode = this.makeBranchNode_(
      cvox.SemanticAttr.Type.BIGOP, [bigOp, arg], []);
  newNode.role = bigOp.role;
  return newNode;
};


/**
 * Create an integral node. It has three children: integral, integrand and
 * integration variable. The latter two can be omitted.
 * @param {!cvox.SemanticTree.Node} integral The integral operator.
 * @param {cvox.SemanticTree.Node} integrand The integrand.
 * @param {cvox.SemanticTree.Node} intvar The integral variable.
 * @return {!cvox.SemanticTree.Node} The new integral node.
 * @private
 */
cvox.SemanticTree.prototype.makeIntegralNode_ = function(
    integral, integrand, intvar) {
  integrand = integrand || this.makeEmptyNode_();
  intvar = intvar || this.makeEmptyNode_();
  var newNode = this.makeBranchNode_(cvox.SemanticAttr.Type.INTEGRAL,
                                [integral, integrand, intvar], []);
  newNode.role = integral.role;
  return newNode;
};


/**
 * Predicate implementing the boundary criteria for simple functions:
 *
 * @param {!cvox.SemanticTree.Node} node A semantic node of type fenced.
 * @return {boolean} True if the node meets the boundary criteria.
 * @private
 */
cvox.SemanticTree.prototype.simpleFunctionHeuristic_ = function(node) {
  var children = node.childNodes;
  if (children.length == 0) {
    return true;
  }
  if (children.length > 1) {
    return false;
  }
  var child = children[0];
  if (child.type == cvox.SemanticAttr.Type.INFIXOP) {
    if (child.role != cvox.SemanticAttr.Role.IMPLICIT) {
      return false;
    }
    if (child.childNodes.some(cvox.SemanticTree.attrPred_('type', 'INFIXOP'))) {
      return false;
    }
  }
  return true;
};


/**
 * Predicate implementing the boundary criteria for prefix functions and big
 * operators:
 * 1. an explicit operator,
 * 2. a relation symbol, or
 * 3. some punctuation.
 * @param {cvox.SemanticTree.Node} node A semantic node.
 * @return {boolean} True if the node meets the boundary criteria.
 * @private
 */
cvox.SemanticTree.prefixFunctionBoundary_ = function(node) {
  return cvox.SemanticTree.attrPred_('type', 'OPERATOR')(node) ||
      cvox.SemanticTree.generalFunctionBoundary_(node);
};


/**
 * Predicate implementing the boundary criteria for integrals dx on two nodes.
 * @param {cvox.SemanticTree.Node} firstNode A semantic node.
 * @param {cvox.SemanticTree.Node} secondNode The direct neighbour of first
 *     Node.
 * @return {boolean} True if the second node exists and the first node is a 'd'.
 * @private
 */
cvox.SemanticTree.integralDxBoundary_ = function(
    firstNode, secondNode) {
  return !!secondNode &&
      cvox.SemanticTree.attrPred_('type', 'IDENTIFIER')(secondNode) &&
          cvox.SemanticAttr.isCharacterD(firstNode.textContent);
};


/**
 * Predicate implementing the boundary criteria for integrals dx on a single
 * node.
 * @param {cvox.SemanticTree.Node} node A semantic node.
 * @return {boolean} True if the node meets the boundary criteria.
 * @private
 */
cvox.SemanticTree.integralDxBoundarySingle_ = function(node) {
  if (cvox.SemanticTree.attrPred_('type', 'IDENTIFIER')(node)) {
    var firstChar = node.textContent[0];
    return firstChar && node.textContent[1] &&
        cvox.SemanticAttr.isCharacterD(firstChar);
  }
  return false;
};


/**
 * Predicate implementing the general boundary criteria for function operators:
 * 1. a relation symbol,
 * 2. some punctuation.
 * @param {cvox.SemanticTree.Node} node A semantic node.
 * @return {boolean} True if the node meets the boundary criteria.
 * @private
 */
cvox.SemanticTree.generalFunctionBoundary_ = function(node) {
  return cvox.SemanticTree.attrPred_('type', 'RELATION')(node) ||
      cvox.SemanticTree.attrPred_('type', 'PUNCTUATION')(node);
};


/**
 * Rewrites tables into matrices or case statements in a list of nodes.
 * @param {!Array<cvox.SemanticTree.Node>} nodes List of nodes to rewrite.
 * @return {!Array<cvox.SemanticTree.Node>} The new list of nodes.
 * @private
 */
cvox.SemanticTree.prototype.processTablesInRow_ = function(nodes) {
  // First we process all matrices:
  var partition = cvox.SemanticTree.partitionNodes_(
      nodes, cvox.SemanticTree.tableIsMatrixOrVector_);
  var result = [];
  for (var i = 0, matrix; matrix = partition.rel[i]; i++) {
    result = result.concat(partition.comp.shift());
    result.push(this.tableToMatrixOrVector_(matrix));
  }
  result = result.concat(partition.comp.shift());
  // Process the remaining tables for cases.
  partition = cvox.SemanticTree.partitionNodes_(
      result, cvox.SemanticTree.isTableOrMultiline_);
  result = [];
  for (var i = 0, table; table = partition.rel[i]; i++) {
    var prevNodes = partition.comp.shift();
    if (cvox.SemanticTree.tableIsCases_(table, prevNodes)) {
      this.tableToCases_(
          table, /** @type {!cvox.SemanticTree.Node} */ (prevNodes.pop()));
    }
    result = result.concat(prevNodes);
    result.push(table);
  }
  return result.concat(partition.comp.shift());
};


/**
 * Decides if a node is a table or multiline element.
 * @param {cvox.SemanticTree.Node} node A node.
 * @return {boolean} True if node is either table or multiline.
 * @private
 */
cvox.SemanticTree.isTableOrMultiline_ = function(node) {
  return !!node && (cvox.SemanticTree.attrPred_('type', 'TABLE')(node) ||
      cvox.SemanticTree.attrPred_('type', 'MULTILINE')(node));
};


/**
 * Heuristic to decide if we have a matrix: An expression fenced on both sides
 * without any other content is considered a fenced node.
 * @param {cvox.SemanticTree.Node} node A node.
 * @return {boolean} True if we believe we have a matrix.
 * @private
 */
cvox.SemanticTree.tableIsMatrixOrVector_ = function(node) {
  return !!node && cvox.SemanticTree.attrPred_('type', 'FENCED')(node) &&
      cvox.SemanticTree.attrPred_('role', 'LEFTRIGHT')(node) &&
          node.childNodes.length == 1 &&
              cvox.SemanticTree.isTableOrMultiline_(node.childNodes[0]);
};


/**
 * Replaces a fenced node by a matrix or vector node.
 * @param {!cvox.SemanticTree.Node} node The fenced node to be rewritten.
 * @return {!cvox.SemanticTree.Node} The matrix or vector node.
 * @private
 */
cvox.SemanticTree.prototype.tableToMatrixOrVector_ = function(node) {
  var matrix = node.childNodes[0];
  var type = cvox.SemanticTree.attrPred_('type', 'MULTILINE')(matrix) ?
      'VECTOR' : 'MATRIX';
  matrix.type = cvox.SemanticAttr.Type[type];
  node.contentNodes.forEach(goog.bind(matrix.appendContentNode_, matrix));
  for (var i = 0, row; row = matrix.childNodes[i]; i++) {
    cvox.SemanticTree.assignRoleToRow_(row, cvox.SemanticAttr.Role[type]);
  }
  return matrix;
};


/**
 * Heuristic to decide if we have a case statement: An expression with a
 * singular open fence before it.
 * @param {!cvox.SemanticTree.Node} table A table node.
 * @param {!Array<cvox.SemanticTree.Node>} prevNodes A list of previous nodes.
 * @return {boolean} True if we believe we have a case statement.
 * @private
 */
cvox.SemanticTree.tableIsCases_ = function(table, prevNodes) {
  return prevNodes.length > 0 &&
      cvox.SemanticTree.attrPred_('role', 'OPENFENCE')(
          prevNodes[prevNodes.length - 1]);
};


/**
 * Makes case node out of a table and a fence.
 * @param {!cvox.SemanticTree.Node} table The table containing the cases.
 * @param {!cvox.SemanticTree.Node} openFence The left delimiter of the case
 *     statement.
 * @return {!cvox.SemanticTree.Node} The cases node.
 * @private
 */
cvox.SemanticTree.prototype.tableToCases_ = function(table, openFence) {
  for (var i = 0, row; row = table.childNodes[i]; i++) {
    cvox.SemanticTree.assignRoleToRow_(row, cvox.SemanticAttr.Role.CASES);
    // }
  }
  table.type = cvox.SemanticAttr.Type.CASES;
  table.appendContentNode_(openFence);
  return table;
};


// TODO (sorge) This heuristic is very primitive. We could start reworking
// multilines, by combining all cells, semantically rewriting the entire line
// and see if there are any similarities. Alternatively, we could look for
// similarities in columns (e.g., single relation symbols, like equalities or
// inequalities in the same column could indicate an equation array).
/**
 * Heuristic to decide if we have a multiline formula. A table is considered a
 * multiline formula if it does not have any separate cells.
 * @param {!cvox.SemanticTree.Node} table A table node.
 * @return {boolean} True if we believe we have a mulitline formula.
 * @private
 */
cvox.SemanticTree.tableIsMultiline_ = function(table) {
  return table.childNodes.every(
      function(row) {
        var length = row.childNodes.length;
        return length <= 1;});
};


/**
 * Rewrites a table to multiline structure, simplifying it by getting rid of the
 * cell hierarchy level.
 * @param {!cvox.SemanticTree.Node} table The node to be rewritten a multiline.
 * @private
 */
cvox.SemanticTree.prototype.tableToMultiline_ = function(table) {
  table.type = cvox.SemanticAttr.Type.MULTILINE;
  for (var i = 0, row; row = table.childNodes[i]; i++) {
    cvox.SemanticTree.rowToLine_(row, cvox.SemanticAttr.Role.MULTILINE);
  }
};


/**
 * Converts a row that only contains one cell into a single line.
 * @param {!cvox.SemanticTree.Node} row The row to convert.
 * @param {cvox.SemanticAttr.Role=} role The new role for the line.
 * @private
 */
cvox.SemanticTree.rowToLine_ = function(row, role) {
  role = role || cvox.SemanticAttr.Role.UNKNOWN;
  if (cvox.SemanticTree.attrPred_('type', 'ROW')(row) &&
      row.childNodes.length == 1 &&
          cvox.SemanticTree.attrPred_('type', 'CELL')(row.childNodes[0])) {
    row.type = cvox.SemanticAttr.Type.LINE;
    row.role = role;
    row.childNodes = row.childNodes[0].childNodes;
  }
};


/**
 * Assign a row and its contained cells a new role value.
 * @param {!cvox.SemanticTree.Node} row The row to be updated.
 * @param {!cvox.SemanticAttr.Role} role The new role for the row and its cells.
 * @private
 */
cvox.SemanticTree.assignRoleToRow_ = function(row, role) {
  if (cvox.SemanticTree.attrPred_('type', 'LINE')(row)) {
    row.role = role;
    return;
  }
  if (cvox.SemanticTree.attrPred_('type', 'ROW')(row)) {
    row.role = role;
    var cellPred = cvox.SemanticTree.attrPred_('type', 'CELL');
    row.childNodes.forEach(function(cell) {
      if (cellPred(cell)) {
        cell.role = role;
      }
    });
  }
};


/**
 * Splits a list of nodes wrt. to a given predicate.
 * @param {Array<cvox.SemanticTree.Node>} nodes A list of nodes.
 * @param {!function(cvox.SemanticTree.Node): boolean} pred Predicate for the
 *    partitioning relation.
 * @param {boolean=} reverse If true slicing is done from the end.
 * @return {{head: !Array<cvox.SemanticTree.Node>,
 *           div: cvox.SemanticTree.Node,
 *           tail: !Array<cvox.SemanticTree.Node>}} The split list.
 * @private
 */
cvox.SemanticTree.sliceNodes_ = function(nodes, pred, reverse) {
  if (reverse) {
    nodes.reverse();
  }
  var head = [];
  for (var i = 0, node; node = nodes[i]; i++) {
    if (pred(node)) {
      if (reverse) {
        return {head: nodes.slice(i + 1).reverse(),
                div: node,
                tail: head.reverse()};
      }
      return {head: head,
              div: node,
              tail: nodes.slice(i + 1)};
    }
    head.push(node);
  }
  if (reverse) {
    return {head: [], div: null, tail: head.reverse()};
  }
  return {head: head, div: null, tail: []};
};


/**
 * Partitions a list of nodes wrt. to a given predicate. Effectively works like
 * a PER on the ordered set of nodes.
 * @param {!Array<!cvox.SemanticTree.Node>} nodes A list of nodes.
 * @param {!function(cvox.SemanticTree.Node): boolean} pred Predicate for the
 *    partitioning relation.
 * @return {{rel: !Array<cvox.SemanticTree.Node>,
 *           comp: !Array<!Array<cvox.SemanticTree.Node>>}}
 *    The partitioning given in terms of a collection of elements satisfying
 *    the predicate and a collection of complementary sets lying inbetween the
 *    related elements. Observe that we always have |comp| = |rel| + 1.
 *
 * Example: On input [a, r_1, b, c, r_2, d, e, r_3] where P(r_i) holds, we
 *    get as output: {rel: [r_1, r_2, r_3], comp: [[a], [b, c], [d, e], []].
 * @private
 */
cvox.SemanticTree.partitionNodes_ = function(nodes, pred) {
  var restNodes = nodes;
  var rel = [];
  var comp = [];

  do {
    var result = cvox.SemanticTree.sliceNodes_(restNodes, pred);
    comp.push(result.head);
    rel.push(result.div);
    restNodes = result.tail;
  } while (result.div);
  rel.pop();
  return {rel: rel, comp: comp};
};


/**
 * Constructs a predicate to check the semantic attribute of a node.
 * @param {!string} prop The property of a node.
 * @param {!string} attr The attribute.
 * @return {function(cvox.SemanticTree.Node): boolean} The predicate.
 * @private
 */

cvox.SemanticTree.attrPred_ = function(prop, attr) {
  var getAttr = function(prop) {
    switch (prop) {
      case 'type': return cvox.SemanticAttr.Type[attr];
      case 'role': return cvox.SemanticAttr.Role[attr];
      case 'font': return cvox.SemanticAttr.Font[attr];
    }
  };

  return function(node) {return node[prop] == getAttr(prop);};
};
