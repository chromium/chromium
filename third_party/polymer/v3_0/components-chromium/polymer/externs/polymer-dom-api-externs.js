/**
 * @externs
 * @fileoverview Externs for PolymerDomApi for backwards compatibility with
 * the Polymer 1 externs.
 */

/**
 * A Polymer DOM API for manipulating DOM such that local DOM and light DOM
 * trees are properly maintained.
 *
 * This type exists only to provide compatibility between compiled hybrid
 * Polymer V1 and V2 code. Polymer V2 only code should simply use the DomApi
 * class type.
 *
 * @interface
 */
var PolymerDomApi = function() {};

/**
 * @param {?Node} node
 * @return {boolean}
 */
PolymerDomApi.prototype.deepContains = function(node) {};

/** @param {!Node} node */
PolymerDomApi.prototype.appendChild = function(node) {};

/**
 * @param {!Node} oldNode
 * @param {!Node} newNode
 */
PolymerDomApi.prototype.replaceChild = function(oldNode, newNode) {};

/**
 * @param {!Node} node
 * @param {?Node} beforeNode
 */
PolymerDomApi.prototype.insertBefore = function(node, beforeNode) {};

/** @param {!Node} node */
PolymerDomApi.prototype.removeChild = function(node) {};

/** @type {!Array<!HTMLElement>|!NodeList<!HTMLElement>} */
PolymerDomApi.prototype.children;

/** @type {!Array<!Node>|!NodeList<!Node>} */
PolymerDomApi.prototype.childNodes;

/** @type {?Node} */
PolymerDomApi.prototype.parentNode;

/** @type {?Node} */
PolymerDomApi.prototype.firstChild;

/** @type {?Node} */
PolymerDomApi.prototype.lastChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.firstElementChild;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.lastElementChild;

/** @type {?Node} */
PolymerDomApi.prototype.previousSibling;

/** @type {?Node} */
PolymerDomApi.prototype.nextSibling;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.previousElementSibling;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.nextElementSibling;

/** @type {string} */
PolymerDomApi.prototype.textContent;

/** @type {string} */
PolymerDomApi.prototype.innerHTML;

/** @type {?HTMLElement} */
PolymerDomApi.prototype.activeElement;

/**
 * @param {string} selector
 * @return {?Element}
 */
PolymerDomApi.prototype.querySelector = function(selector) {};

/**
 * @param {string} selector
 * @return {!Array<!Element>|!NodeList<!Element>}
 */
PolymerDomApi.prototype.querySelectorAll = function(selector) {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDistributedNodes = function() {};

/** @return {!Array<!Node>} */
PolymerDomApi.prototype.getDestinationInsertionPoints = function() {};

/** @return {?Node} */
PolymerDomApi.prototype.getOwnerRoot = function() {};

/** @type {!Node} */
PolymerDomApi.prototype.node;

/**
 * @param {string} attribute
 * @param {string} value
 */
PolymerDomApi.prototype.setAttribute = function(attribute, value) {};

/** @param {string} attribute */
PolymerDomApi.prototype.removeAttribute = function(attribute) {};

/**
 * @typedef {function(!PolymerDomApi.ObserveInfo)}
 */
PolymerDomApi.ObserveCallback;

/**
 * @typedef {{
 *   target: !Node,
 *   addedNodes: !Array<!Node>,
 *   removedNodes: !Array<!Node>
 * }}
 */
PolymerDomApi.ObserveInfo;

/**
 * A virtual type for observer callback handles.
 *
 * @interface
 */
PolymerDomApi.ObserveHandle = function() {};

/**
 * @return {void}
 */
PolymerDomApi.ObserveHandle.prototype.disconnect = function() {};

/**
 * Notifies callers about changes to the element's effective child nodes,
 * the same list as returned by `getEffectiveChildNodes`.
 *
 * @param {!PolymerDomApi.ObserveCallback} callback The supplied callback
 * is called with an `info` argument which is an object that provides
 * the `target` on which the changes occurred, a list of any nodes
 * added in the `addedNodes` array, and nodes removed in the
 * `removedNodes` array.
 *
 * @return {!PolymerDomApi.ObserveHandle} Handle which is the argument to
 * `unobserveNodes`.
 */
PolymerDomApi.prototype.observeNodes = function(callback) {};

/**
 * Stops observing changes to the element's effective child nodes.
 *
 * @param {!PolymerDomApi.ObserveHandle} handle The handle for the
 * callback that should no longer receive notifications. This
 * handle is returned from `observeNodes`.
 */
PolymerDomApi.prototype.unobserveNodes = function(handle) {};

/** @type {?DOMTokenList} */
PolymerDomApi.prototype.classList;

/**
 * @param {string} selector
 * @return {!Array<!HTMLElement>}
 */
PolymerDomApi.prototype.queryDistributedElements = function(selector) {};

/**
 * Returns a list of effective child nodes for this element.
 *
 * @return {!Array<!HTMLElement>}
 */
PolymerDomApi.prototype.getEffectiveChildNodes = function() {};
