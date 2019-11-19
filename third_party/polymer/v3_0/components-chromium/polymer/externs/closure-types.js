/**
 * @fileoverview Generated typings for Polymer mixins
 * @externs
 * @suppress {checkPrototypalTypes}
 *
 * @license
 * Copyright (c) 2017 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at http://polymer.github.io/LICENSE.txt
 * The complete set of authors may be found at http://polymer.github.io/AUTHORS.txt
 * The complete set of contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt
 * Code distributed by Google as part of the polymer project is also
 * subject to an additional IP rights grant found at http://polymer.github.io/PATENTS.txt
 */
/* eslint-disable */
/**
* @interface
*/
function Polymer_PropertiesChanged(){}
/** @type {undefined} */
Polymer_PropertiesChanged.prototype.__dataEnabled;

/**
* @override
* @param {string} property Name of the property
* @param {boolean=} readOnly When true, no setter is created; the
  protected `_setProperty` function must be used to set the property
* @return {void}
*/
Polymer_PropertiesChanged.prototype._createPropertyAccessor = function(property, readOnly){};
/**
* @override
* @param {string} property Name of the property
* @return {void}
*/
Polymer_PropertiesChanged.prototype._addPropertyToAttributeMap = function(property){};
/**
* @override
* @param {string} property Name of the property
* @param {boolean=} readOnly When true, no setter is created
* @return {void}
*/
Polymer_PropertiesChanged.prototype._definePropertyAccessor = function(property, readOnly){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesChanged.prototype.ready = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesChanged.prototype._initializeProperties = function(){};
/**
* @override
* @param {Object} props Bag of property values that were overwritten
  when creating property accessors.
* @return {void}
*/
Polymer_PropertiesChanged.prototype._initializeInstanceProperties = function(props){};
/**
* @override
* @param {string} property Name of the property
* @param {*} value Value to set
* @return {void}
*/
Polymer_PropertiesChanged.prototype._setProperty = function(property, value){};
/**
* @override
* @param {string} property Name of property
* @return {*}
*/
Polymer_PropertiesChanged.prototype._getProperty = function(property){};
/**
* @override
* @param {string} property Name of the property
* @param {*} value Value to set
* @param {boolean=} ext Not used here; affordance for closure
* @return {boolean}
*/
Polymer_PropertiesChanged.prototype._setPendingProperty = function(property, value, ext){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesChanged.prototype._invalidateProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesChanged.prototype._enableProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesChanged.prototype._flushProperties = function(){};
/**
* @override
* @param {!Object} currentProps Bag of all current accessor values
* @param {?Object} changedProps Bag of properties changed since the last
  call to `_propertiesChanged`
* @param {?Object} oldProps Bag of previous values for each property
  in `changedProps`
* @return {boolean}
*/
Polymer_PropertiesChanged.prototype._shouldPropertiesChange = function(currentProps, changedProps, oldProps){};
/**
* @override
* @param {!Object} currentProps Bag of all current accessor values
* @param {?Object} changedProps Bag of properties changed since the last
  call to `_propertiesChanged`
* @param {?Object} oldProps Bag of previous values for each property
  in `changedProps`
* @return {void}
*/
Polymer_PropertiesChanged.prototype._propertiesChanged = function(currentProps, changedProps, oldProps){};
/**
* @override
* @param {string} property Property name
* @param {*} value New property value
* @param {*} old Previous property value
* @return {boolean}
*/
Polymer_PropertiesChanged.prototype._shouldPropertyChange = function(property, value, old){};
/**
* @override
* @param {string} name Name of attribute that changed
* @param {?string} old Old attribute value
* @param {?string} value New attribute value
* @param {?string=} namespace Attribute namespace.
* @return {void}
*/
Polymer_PropertiesChanged.prototype.attributeChangedCallback = function(name, old, value, namespace){};
/**
* @override
* @param {string} attribute Name of attribute to deserialize.
* @param {?string} value of the attribute.
* @param {*=} type type to deserialize to, defaults to the value
returned from `typeForProperty`
* @return {void}
*/
Polymer_PropertiesChanged.prototype._attributeToProperty = function(attribute, value, type){};
/**
* @override
* @param {string} property Property name to reflect.
* @param {string=} attribute Attribute name to reflect to.
* @param {*=} value Property value to refect.
* @return {void}
*/
Polymer_PropertiesChanged.prototype._propertyToAttribute = function(property, attribute, value){};
/**
* @override
* @param {Element} node Element to set attribute to.
* @param {*} value Value to serialize.
* @param {string} attribute Attribute name to serialize to.
* @return {void}
*/
Polymer_PropertiesChanged.prototype._valueToNodeAttribute = function(node, value, attribute){};
/**
* @override
* @param {*} value Property value to serialize.
* @return {(string | undefined)}
*/
Polymer_PropertiesChanged.prototype._serializeValue = function(value){};
/**
* @override
* @param {?string} value Value to deserialize.
* @param {*=} type Type to deserialize the string to.
* @return {*}
*/
Polymer_PropertiesChanged.prototype._deserializeValue = function(value, type){};
/**
* @param {!Object} props Object whose keys are names of accessors.
* @return {void}
*/
Polymer_PropertiesChanged.createProperties = function(props){};
/**
* @param {string} property Property to convert
* @return {string}
*/
Polymer_PropertiesChanged.attributeNameForProperty = function(property){};
/**
* @param {string} name Name of property
* @return {void}
*/
Polymer_PropertiesChanged.typeForProperty = function(name){};
/**
* @interface
* @extends {Polymer_PropertiesChanged}
*/
function Polymer_PropertyAccessors(){}
/**
* @override
* @param {string} property Name of the property
* @param {boolean=} readOnly When true, no setter is created

When calling on a prototype, any overwritten values are saved in
`__dataProto`, and it is up to the subclasser to decide how/when
to set those properties back into the accessor.  When calling on an
instance, the overwritten value is set via `_setPendingProperty`,
and the user should call `_invalidateProperties` or `_flushProperties`
for the values to take effect.
* @return {void}
*/
Polymer_PropertyAccessors.prototype._definePropertyAccessor = function(property, readOnly){};
/**
* @override
* @return {void}
*/
Polymer_PropertyAccessors.prototype._initializeProperties = function(){};
/**
* @override
* @param {*} value Property value to serialize.
* @return {(string | undefined)}
*/
Polymer_PropertyAccessors.prototype._serializeValue = function(value){};
/**
* @override
* @param {?string} value Attribute value to deserialize.
* @param {*=} type Type to deserialize the string to.
* @return {*}
*/
Polymer_PropertyAccessors.prototype._deserializeValue = function(value, type){};
/**
* @override
* @param {Object} props Bag of property values that were overwritten
  when creating property accessors.
* @return {void}
*/
Polymer_PropertyAccessors.prototype._initializeProtoProperties = function(props){};
/**
* @override
* @param {string} attribute Name of attribute to ensure is set.
* @param {string} value of the attribute.
* @return {void}
*/
Polymer_PropertyAccessors.prototype._ensureAttribute = function(attribute, value){};
/**
* @override
* @param {string} property Property name
* @return {boolean}
*/
Polymer_PropertyAccessors.prototype._hasAccessor = function(property){};
/**
* @override
* @param {string} prop Property name
* @return {boolean}
*/
Polymer_PropertyAccessors.prototype._isPropertyPending = function(prop){};
/**
* @param {string} property Property to convert
* @return {string}
*/
Polymer_PropertyAccessors.attributeNameForProperty = function(property){};
/**
* @return {void}
*/
Polymer_PropertyAccessors.createPropertiesForAttributes = function(){};
/**
* @interface
*/
function Polymer_TemplateStamp(){}
/**
* @override
* @param {!HTMLTemplateElement} template Template to stamp
* @return {!StampedTemplate}
*/
Polymer_TemplateStamp.prototype._stampTemplate = function(template){};
/**
* @override
* @param {!EventTarget} node Node to add listener on
* @param {string} eventName Name of event
* @param {string} methodName Name of method
* @param {*=} context Context the method will be called on (defaults
  to `node`)
* @return {Function}
*/
Polymer_TemplateStamp.prototype._addMethodEventListenerToNode = function(node, eventName, methodName, context){};
/**
* @override
* @param {!EventTarget} node Node to add event listener to
* @param {string} eventName Name of event
* @param {function (!Event): void} handler Listener function to add
* @return {void}
*/
Polymer_TemplateStamp.prototype._addEventListenerToNode = function(node, eventName, handler){};
/**
* @override
* @param {!EventTarget} node Node to remove event listener from
* @param {string} eventName Name of event
* @param {function (!Event): void} handler Listener function to remove
* @return {void}
*/
Polymer_TemplateStamp.prototype._removeEventListenerFromNode = function(node, eventName, handler){};
/**
* @param {!HTMLTemplateElement} template Template to parse
* @param {TemplateInfo=} outerTemplateInfo Template metadata from the outer
  template, for parsing nested templates
* @return {!TemplateInfo}
*/
Polymer_TemplateStamp._parseTemplate = function(template, outerTemplateInfo){};
/**
* @param {*} template 
* @param {*} templateInfo 
* @param {*} nodeInfo 
*/
Polymer_TemplateStamp._parseTemplateContent = function(template, templateInfo, nodeInfo){};
/**
* @param {Node} node Node to parse
* @param {!TemplateInfo} templateInfo Template metadata for current template
* @param {!NodeInfo} nodeInfo Node metadata for current template.
* @return {boolean}
*/
Polymer_TemplateStamp._parseTemplateNode = function(node, templateInfo, nodeInfo){};
/**
* @param {Node} root Root node whose `childNodes` will be parsed
* @param {!TemplateInfo} templateInfo Template metadata for current template
* @param {!NodeInfo} nodeInfo Node metadata for current template.
* @return {void}
*/
Polymer_TemplateStamp._parseTemplateChildNodes = function(root, templateInfo, nodeInfo){};
/**
* @param {HTMLTemplateElement} node Node to parse (a <template>)
* @param {TemplateInfo} outerTemplateInfo Template metadata for current template
  that includes the template `node`
* @param {!NodeInfo} nodeInfo Node metadata for current template.
* @return {boolean}
*/
Polymer_TemplateStamp._parseTemplateNestedTemplate = function(node, outerTemplateInfo, nodeInfo){};
/**
* @param {Element} node Node to parse
* @param {TemplateInfo} templateInfo Template metadata for current template
* @param {NodeInfo} nodeInfo Node metadata for current template.
* @return {boolean}
*/
Polymer_TemplateStamp._parseTemplateNodeAttributes = function(node, templateInfo, nodeInfo){};
/**
* @param {Element} node Node to parse
* @param {!TemplateInfo} templateInfo Template metadata for current template
* @param {!NodeInfo} nodeInfo Node metadata for current template.
* @param {string} name Attribute name
* @param {string} value Attribute value
* @return {boolean}
*/
Polymer_TemplateStamp._parseTemplateNodeAttribute = function(node, templateInfo, nodeInfo, name, value){};
/**
* @param {HTMLTemplateElement} template Template to retrieve `content` for
* @return {DocumentFragment}
*/
Polymer_TemplateStamp._contentForTemplate = function(template){};
/**
* @interface
* @extends {Polymer_TemplateStamp}
* @extends {Polymer_PropertyAccessors}
*/
function Polymer_PropertyEffects(){}
/** @type {boolean} */
Polymer_PropertyEffects.prototype.__dataClientsReady;

/** @type {Array} */
Polymer_PropertyEffects.prototype.__dataPendingClients;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__dataToNotify;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__dataLinkedPaths;

/** @type {boolean} */
Polymer_PropertyEffects.prototype.__dataHasPaths;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__dataCompoundStorage;

/** @type {Polymer_PropertyEffects} */
Polymer_PropertyEffects.prototype.__dataHost;

/** @type {!Object} */
Polymer_PropertyEffects.prototype.__dataTemp;

/** @type {boolean} */
Polymer_PropertyEffects.prototype.__dataClientsInitialized;

/** @type {!Object} */
Polymer_PropertyEffects.prototype.__data;

/** @type {(!Object | null)} */
Polymer_PropertyEffects.prototype.__dataPending;

/** @type {!Object} */
Polymer_PropertyEffects.prototype.__dataOld;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__computeEffects;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__reflectEffects;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__notifyEffects;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__propagateEffects;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__observeEffects;

/** @type {Object} */
Polymer_PropertyEffects.prototype.__readOnly;

/** @type {!TemplateInfo} */
Polymer_PropertyEffects.prototype.__templateInfo;

/** @type {undefined} */
Polymer_PropertyEffects.prototype.PROPERTY_EFFECT_TYPES;

/**
* @override
* @param {!HTMLTemplateElement} template Template to stamp
* @return {!StampedTemplate}
*/
Polymer_PropertyEffects.prototype._stampTemplate = function(template){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype.ready = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype._initializeProperties = function(){};
/**
* @override
* @param {Object} props Properties to initialize on the instance
* @return {void}
*/
Polymer_PropertyEffects.prototype._initializeInstanceProperties = function(props){};
/**
* @override
* @param {string} property Name of the property
* @param {*} value Value to set
* @return {void}
*/
Polymer_PropertyEffects.prototype._setProperty = function(property, value){};
/**
* @override
* @param {string} property Name of the property
* @param {*} value Value to set
* @param {boolean=} shouldNotify True if property should fire notification
  event (applies only for `notify: true` properties)
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._setPendingProperty = function(property, value, shouldNotify){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype._invalidateProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype._flushProperties = function(){};
/**
* @override
* @param {!Object} currentProps Bag of all current accessor values
* @param {?Object} changedProps Bag of properties changed since the last
  call to `_propertiesChanged`
* @param {?Object} oldProps Bag of previous values for each property
  in `changedProps`
* @return {void}
*/
Polymer_PropertyEffects.prototype._propertiesChanged = function(currentProps, changedProps, oldProps){};
/**
* @override
* @param {Object} props Properties to initialize on the prototype
* @return {void}
*/
Polymer_PropertyEffects.prototype._initializeProtoProperties = function(props){};
/**
* @override
* @param {string} property Property that should trigger the effect
* @param {string} type Effect type, from this.PROPERTY_EFFECT_TYPES
* @param {Object=} effect Effect metadata object
* @return {void}
*/
Polymer_PropertyEffects.prototype._addPropertyEffect = function(property, type, effect){};
/**
* @override
* @param {string} property Property the effect was associated with
* @param {string} type Effect type, from this.PROPERTY_EFFECT_TYPES
* @param {Object=} effect Effect metadata object to remove
* @return {void}
*/
Polymer_PropertyEffects.prototype._removePropertyEffect = function(property, type, effect){};
/**
* @override
* @param {string} property Property name
* @param {string=} type Effect type, from this.PROPERTY_EFFECT_TYPES
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._hasPropertyEffect = function(property, type){};
/**
* @override
* @param {string} property Property name
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._hasReadOnlyEffect = function(property){};
/**
* @override
* @param {string} property Property name
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._hasNotifyEffect = function(property){};
/**
* @override
* @param {string} property Property name
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._hasReflectEffect = function(property){};
/**
* @override
* @param {string} property Property name
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._hasComputedEffect = function(property){};
/**
* @override
* @param {(string | !Array.<(number | string)>)} path Path to set
* @param {*} value Value to set
* @param {boolean=} shouldNotify Set to true if this change should
 cause a property notification event dispatch
* @param {boolean=} isPathNotification If the path being set is a path
  notification of an already changed value, as opposed to a request
  to set and notify the change.  In the latter `false` case, a dirty
  check is performed and then the value is set to the path before
  enqueuing the pending property change.
* @return {boolean}
*/
Polymer_PropertyEffects.prototype._setPendingPropertyOrPath = function(path, value, shouldNotify, isPathNotification){};
/**
* @override
* @param {!Node} node The node to set a property on
* @param {string} prop The property to set
* @param {*} value The value to set
* @return {void}
*/
Polymer_PropertyEffects.prototype._setUnmanagedPropertyToNode = function(node, prop, value){};
/**
* @override
* @param {Object} client PropertyEffects client to enqueue
* @return {void}
*/
Polymer_PropertyEffects.prototype._enqueueClient = function(client){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype._flushClients = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertyEffects.prototype._readyClients = function(){};
/**
* @override
* @param {Object} props Bag of one or more key-value pairs whose key is
  a property and value is the new value to set for that property.
* @param {boolean=} setReadOnly When true, any private values set in
  `props` will be set. By default, `setProperties` will not set
  `readOnly: true` root properties.
* @return {void}
*/
Polymer_PropertyEffects.prototype.setProperties = function(props, setReadOnly){};
/**
* @override
* @param {Object} changedProps Bag of changed properties
* @param {Object} oldProps Bag of previous values for changed properties
* @param {boolean} hasPaths True with `props` contains one or more paths
* @return {void}
*/
Polymer_PropertyEffects.prototype._propagatePropertyChanges = function(changedProps, oldProps, hasPaths){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} to Target path to link.
* @param {(string | !Array.<(string | number)>)} from Source path to link.
* @return {void}
*/
Polymer_PropertyEffects.prototype.linkPaths = function(to, from){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Target path to unlink.
* @return {void}
*/
Polymer_PropertyEffects.prototype.unlinkPaths = function(path){};
/**
* @override
* @param {string} path Path that should be notified.
* @param {Array} splices Array of splice records indicating ordered
  changes that occurred to the array. Each record should have the
  following fields:
   * index: index at which the change occurred
   * removed: array of items that were removed from this index
   * addedCount: number of new items added at this index
   * object: a reference to the array in question
   * type: the string literal 'splice'

  Note that splice records _must_ be normalized such that they are
  reported in index order (raw results from `Object.observe` are not
  ordered and must be normalized/merged before notifying).
* @return {void}
*/
Polymer_PropertyEffects.prototype.notifySplices = function(path, splices){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to the value
  to read.  The path may be specified as a string (e.g. `foo.bar.baz`)
  or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
  bracketed expressions are not supported; string-based path parts
  *must* be separated by dots.  Note that when dereferencing array
  indices, the index may be used as a dotted part directly
  (e.g. `users.12.name` or `['users', 12, 'name']`).
* @param {Object=} root Root object from which the path is evaluated.
* @return {*}
*/
Polymer_PropertyEffects.prototype.get = function(path, root){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to the value
  to write.  The path may be specified as a string (e.g. `'foo.bar.baz'`)
  or an array of path parts (e.g. `['foo.bar', 'baz']`).  Note that
  bracketed expressions are not supported; string-based path parts
  *must* be separated by dots.  Note that when dereferencing array
  indices, the index may be used as a dotted part directly
  (e.g. `'users.12.name'` or `['users', 12, 'name']`).
* @param {*} value Value to set at the specified path.
* @param {Object=} root Root object from which the path is evaluated.
  When specified, no notification will occur.
* @return {void}
*/
Polymer_PropertyEffects.prototype.set = function(path, value, root){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to array.
* @param {...*} items Items to push onto array
* @return {number}
*/
Polymer_PropertyEffects.prototype.push = function(path, items){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to array.
* @return {*}
*/
Polymer_PropertyEffects.prototype.pop = function(path){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to array.
* @param {number} start Index from which to start removing/inserting.
* @param {number=} deleteCount Number of items to remove.
* @param {...*} items Items to insert into array.
* @return {Array}
*/
Polymer_PropertyEffects.prototype.splice = function(path, start, deleteCount, items){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to array.
* @return {*}
*/
Polymer_PropertyEffects.prototype.shift = function(path){};
/**
* @override
* @param {(string | !Array.<(string | number)>)} path Path to array.
* @param {...*} items Items to insert info array
* @return {number}
*/
Polymer_PropertyEffects.prototype.unshift = function(path, items){};
/**
* @override
* @param {string} path Path that should be notified.
* @param {*=} value Value at the path (optional).
* @return {void}
*/
Polymer_PropertyEffects.prototype.notifyPath = function(path, value){};
/**
* @override
* @param {string} property Property name
* @param {boolean=} protectedSetter Creates a custom protected setter
  when `true`.
* @return {void}
*/
Polymer_PropertyEffects.prototype._createReadOnlyProperty = function(property, protectedSetter){};
/**
* @override
* @param {string} property Property name
* @param {(string | function (*, *))} method Function or name of observer method
    to call
* @param {boolean=} dynamicFn Whether the method name should be included as
  a dependency to the effect.
* @return {void}
*/
Polymer_PropertyEffects.prototype._createPropertyObserver = function(property, method, dynamicFn){};
/**
* @override
* @param {string} expression Method expression
* @param {(boolean | Object)=} dynamicFn Boolean or object map indicating
  whether method names should be included as a dependency to the effect.
* @return {void}
*/
Polymer_PropertyEffects.prototype._createMethodObserver = function(expression, dynamicFn){};
/**
* @override
* @param {string} property Property name
* @return {void}
*/
Polymer_PropertyEffects.prototype._createNotifyingProperty = function(property){};
/**
* @override
* @param {string} property Property name
* @return {void}
*/
Polymer_PropertyEffects.prototype._createReflectedProperty = function(property){};
/**
* @override
* @param {string} property Name of computed property to set
* @param {string} expression Method expression
* @param {(boolean | Object)=} dynamicFn Boolean or object map indicating
  whether method names should be included as a dependency to the effect.
* @return {void}
*/
Polymer_PropertyEffects.prototype._createComputedProperty = function(property, expression, dynamicFn){};
/**
* @override
* @param {!HTMLTemplateElement} template Template containing binding
  bindings
* @param {boolean=} instanceBinding When false (default), performs
  "prototypical" binding of the template and overwrites any previously
  bound template for the class. When true (as passed from
  `_stampTemplate`), the template info is instanced and linked into
  the list of bound templates.
* @return {!TemplateInfo}
*/
Polymer_PropertyEffects.prototype._bindTemplate = function(template, instanceBinding){};
/**
* @override
* @param {!StampedTemplate} dom DocumentFragment previously returned
  from `_stampTemplate` associated with the nodes to be removed
* @return {void}
*/
Polymer_PropertyEffects.prototype._removeBoundDom = function(dom){};
/**
* @param {Node} node Node to parse
* @param {TemplateInfo} templateInfo Template metadata for current template
* @param {NodeInfo} nodeInfo Node metadata for current template node
* @return {boolean}
*/
Polymer_PropertyEffects._parseTemplateNode = function(node, templateInfo, nodeInfo){};
/**
* @param {Node} node Node to parse
* @param {TemplateInfo} templateInfo Template metadata for current template
* @param {NodeInfo} nodeInfo Node metadata for current template node
* @return {boolean}
*/
Polymer_PropertyEffects._parseTemplateNestedTemplate = function(node, templateInfo, nodeInfo){};
/**
* @param {Element} node Node to parse
* @param {TemplateInfo} templateInfo Template metadata for current template
* @param {NodeInfo} nodeInfo Node metadata for current template node
* @param {string} name Attribute name
* @param {string} value Attribute value
* @return {boolean}
*/
Polymer_PropertyEffects._parseTemplateNodeAttribute = function(node, templateInfo, nodeInfo, name, value){};
/**
* @param {string} property Property that should trigger the effect
* @param {string} type Effect type, from this.PROPERTY_EFFECT_TYPES
* @param {Object=} effect Effect metadata object
* @return {void}
*/
Polymer_PropertyEffects.addPropertyEffect = function(property, type, effect){};
/**
* @param {string} property Property name
* @param {(string | function (*, *))} method Function or name of observer method to call
* @param {boolean=} dynamicFn Whether the method name should be included as
  a dependency to the effect.
* @return {void}
*/
Polymer_PropertyEffects.createPropertyObserver = function(property, method, dynamicFn){};
/**
* @param {string} expression Method expression
* @param {(boolean | Object)=} dynamicFn Boolean or object map indicating
* @return {void}
*/
Polymer_PropertyEffects.createMethodObserver = function(expression, dynamicFn){};
/**
* @param {string} property Property name
* @return {void}
*/
Polymer_PropertyEffects.createNotifyingProperty = function(property){};
/**
* @param {string} property Property name
* @param {boolean=} protectedSetter Creates a custom protected setter
  when `true`.
* @return {void}
*/
Polymer_PropertyEffects.createReadOnlyProperty = function(property, protectedSetter){};
/**
* @param {string} property Property name
* @return {void}
*/
Polymer_PropertyEffects.createReflectedProperty = function(property){};
/**
* @param {string} property Name of computed property to set
* @param {string} expression Method expression
* @param {(boolean | Object)=} dynamicFn Boolean or object map indicating whether
  method names should be included as a dependency to the effect.
* @return {void}
*/
Polymer_PropertyEffects.createComputedProperty = function(property, expression, dynamicFn){};
/**
* @param {!HTMLTemplateElement} template Template containing binding
  bindings
* @return {!TemplateInfo}
*/
Polymer_PropertyEffects.bindTemplate = function(template){};
/**
* @param {Object} templateInfo Template metadata to add effect to
* @param {string} prop Property that should trigger the effect
* @param {Object=} effect Effect metadata object
* @return {void}
*/
Polymer_PropertyEffects._addTemplatePropertyEffect = function(templateInfo, prop, effect){};
/**
* @param {string} text Text to parse from attribute or textContent
* @param {Object} templateInfo Current template metadata
* @return {Array.<!BindingPart>}
*/
Polymer_PropertyEffects._parseBindings = function(text, templateInfo){};
/**
* @param {!Polymer_PropertyEffects} inst Element that should be used as
    scope for binding dependencies
* @param {BindingPart} part Binding part metadata
* @param {string} path Property/path that triggered this effect
* @param {Object} props Bag of current property changes
* @param {Object} oldProps Bag of previous values for changed properties
* @param {boolean} hasPaths True with `props` contains one or more paths
* @return {*}
*/
Polymer_PropertyEffects._evaluateBinding = function(inst, part, path, props, oldProps, hasPaths){};
/**
* @interface
* @extends {Polymer_PropertiesChanged}
*/
function Polymer_PropertiesMixin(){}
/**
* @override
* @return {void}
*/
Polymer_PropertiesMixin.prototype._initializeProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesMixin.prototype.connectedCallback = function(){};
/**
* @override
* @return {void}
*/
Polymer_PropertiesMixin.prototype.disconnectedCallback = function(){};
/**
* @param {string} name Name of property
* @return {*}
*/
Polymer_PropertiesMixin.typeForProperty = function(name){};
/**
* @return {void}
*/
Polymer_PropertiesMixin.finalize = function(){};
/**
* @return {void}
*/
Polymer_PropertiesMixin._finalizeClass = function(){};
/**
* @interface
* @extends {Polymer_PropertyEffects}
* @extends {Polymer_PropertiesMixin}
*/
function Polymer_ElementMixin(){}
/** @type {HTMLTemplateElement} */
Polymer_ElementMixin.prototype._template;

/** @type {string} */
Polymer_ElementMixin.prototype._importPath;

/** @type {string} */
Polymer_ElementMixin.prototype.rootPath;

/** @type {string} */
Polymer_ElementMixin.prototype.importPath;

/** @type {(StampedTemplate | HTMLElement | ShadowRoot)} */
Polymer_ElementMixin.prototype.root;

/** @type {!Object.<string, !Element>} */
Polymer_ElementMixin.prototype.$;

/**
* @override
* @return {void}
*/
Polymer_ElementMixin.prototype.ready = function(){};
/**
* @override
* @return {void}
*/
Polymer_ElementMixin.prototype._initializeProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_ElementMixin.prototype._readyClients = function(){};
/**
* @override
* @return {void}
*/
Polymer_ElementMixin.prototype.connectedCallback = function(){};
/**
* @override
* @param {StampedTemplate} dom to attach to the element.
* @return {ShadowRoot}
*/
Polymer_ElementMixin.prototype._attachDom = function(dom){};
/**
* @override
* @param {Object=} properties Bag of custom property key/values to
  apply to this element.
* @return {void}
*/
Polymer_ElementMixin.prototype.updateStyles = function(properties){};
/**
* @override
* @param {string} url URL to resolve.
* @param {string=} base Optional base URL to resolve against, defaults
to the element's `importPath`
* @return {string}
*/
Polymer_ElementMixin.prototype.resolveUrl = function(url, base){};
/**
* @param {!HTMLTemplateElement} template Template
* @param {!TemplateInfo} templateInfo Template metadata for current template
* @param {!NodeInfo} nodeInfo Node metadata for current template.
* @return {boolean}
*/
Polymer_ElementMixin._parseTemplateContent = function(template, templateInfo, nodeInfo){};
/**
* @param {!Object} props .
* @return {void}
*/
Polymer_ElementMixin.createProperties = function(props){};
/**
* @param {Object} templateInfo Template metadata to add effect to
* @param {string} prop Property that should trigger the effect
* @param {Object=} effect Effect metadata object
* @return {void}
*/
Polymer_ElementMixin._addTemplatePropertyEffect = function(templateInfo, prop, effect){};
/**
* @return {void}
*/
Polymer_ElementMixin._finalizeClass = function(){};
/**
* @return {void}
*/
Polymer_ElementMixin._prepareTemplate = function(){};
/**
* @param {Object} observers Array of observer descriptors for
  this class
* @param {Object} dynamicFns Object containing keys for any properties
  that are functions and should trigger the effect when the function
  reference is changed
* @return {void}
*/
Polymer_ElementMixin.createObservers = function(observers, dynamicFns){};
/**
* @param {string} cssText Text containing styling to process
* @param {string} baseURI Base URI to rebase CSS paths against
* @return {string}
*/
Polymer_ElementMixin._processStyleText = function(cssText, baseURI){};
/**
* @param {string} is Tag name (or type extension name) for this element
* @return {void}
*/
Polymer_ElementMixin._finalizeTemplate = function(is){};
/**
* @interface
*/
function Polymer_GestureEventListeners(){}
/**
* @override
* @param {!EventTarget} node Node to add event listener to
* @param {string} eventName Name of event
* @param {function (!Event): void} handler Listener function to add
* @return {void}
*/
Polymer_GestureEventListeners.prototype._addEventListenerToNode = function(node, eventName, handler){};
/**
* @override
* @param {!EventTarget} node Node to remove event listener from
* @param {string} eventName Name of event
* @param {function (!Event): void} handler Listener function to remove
* @return {void}
*/
Polymer_GestureEventListeners.prototype._removeEventListenerFromNode = function(node, eventName, handler){};
/**
* @interface
* @extends {Polymer_PropertyAccessors}
*/
function Polymer_DirMixin(){}
/** @type {boolean} */
Polymer_DirMixin.prototype.__autoDirOptOut;

/**
* @override
* @return {void}
*/
Polymer_DirMixin.prototype.ready = function(){};
/**
* @override
* @return {void}
*/
Polymer_DirMixin.prototype.connectedCallback = function(){};
/**
* @override
* @return {void}
*/
Polymer_DirMixin.prototype.disconnectedCallback = function(){};
/**
* @param {string} cssText .
* @param {string} baseURI .
* @return {string}
*/
Polymer_DirMixin._processStyleText = function(cssText, baseURI){};
/**
* @param {string} text CSS text to replace DIR
* @return {string}
*/
Polymer_DirMixin._replaceDirInCssText = function(text){};
/**
* @interface
* @extends {Polymer_ElementMixin}
* @extends {Polymer_GestureEventListeners}
*/
function Polymer_LegacyElementMixin(){}
/** @type {boolean} */
Polymer_LegacyElementMixin.prototype.isAttached;

/** @type {?WeakMap.<!Element, !Object.<string, !Function>>} */
Polymer_LegacyElementMixin.prototype.__boundListeners;

/** @type {?Object.<string, ?Function>} */
Polymer_LegacyElementMixin.prototype._debouncers;

/** @type {undefined} */
Polymer_LegacyElementMixin.prototype.domHost;

/** @type {string} */
Polymer_LegacyElementMixin.prototype.is;

/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.ready = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._initializeProperties = function(){};
/**
* @override
* @param {string} name Name of attribute.
* @param {?string} old Old value of attribute.
* @param {?string} value Current value of attribute.
* @param {?string} namespace Attribute namespace.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.attributeChangedCallback = function(name, old, value, namespace){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.connectedCallback = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.disconnectedCallback = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.created = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.attached = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.detached = function(){};
/**
* @override
* @param {string} name Name of attribute.
* @param {?string} old Old value of attribute.
* @param {?string} value Current value of attribute.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.attributeChanged = function(name, old, value){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._registered = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._ensureAttributes = function(){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._applyListeners = function(){};
/**
* @override
* @param {*} value Value to deserialize
* @return {(string | undefined)}
*/
Polymer_LegacyElementMixin.prototype.serialize = function(value){};
/**
* @override
* @param {string} value String to deserialize
* @param {*} type Type to deserialize the string to
* @return {*}
*/
Polymer_LegacyElementMixin.prototype.deserialize = function(value, type){};
/**
* @override
* @param {string} property Property name to reflect.
* @param {string=} attribute Attribute name to reflect.
* @param {*=} value Property value to reflect.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.reflectPropertyToAttribute = function(property, attribute, value){};
/**
* @override
* @param {*} value Value to serialize.
* @param {string} attribute Attribute name to serialize to.
* @param {Element} node Element to set attribute to.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.serializeValueToAttribute = function(value, attribute, node){};
/**
* @override
* @param {Object} prototype Target object to copy properties to.
* @param {Object} api Source object to copy properties from.
* @return {Object}
*/
Polymer_LegacyElementMixin.prototype.extend = function(prototype, api){};
/**
* @override
* @param {!Object} target Target object to copy properties to.
* @param {!Object} source Source object to copy properties from.
* @return {!Object}
*/
Polymer_LegacyElementMixin.prototype.mixin = function(target, source){};
/**
* @override
* @param {Object} object The object on which to set the prototype.
* @param {Object} prototype The prototype that will be set on the given
`object`.
* @return {Object}
*/
Polymer_LegacyElementMixin.prototype.chainObject = function(object, prototype){};
/**
* @override
* @param {HTMLTemplateElement} template HTML template element to instance.
* @return {!DocumentFragment}
*/
Polymer_LegacyElementMixin.prototype.instanceTemplate = function(template){};
/**
* @override
* @param {string} type Name of event type.
* @param {*=} detail Detail value containing event-specific
  payload.
* @param {{bubbles: (boolean | undefined), cancelable: (boolean | undefined), composed: (boolean | undefined)}=} options Object specifying options.  These may include:
 `bubbles` (boolean, defaults to `true`),
 `cancelable` (boolean, defaults to false), and
 `node` on which to fire the event (HTMLElement, defaults to `this`).
* @return {!Event}
*/
Polymer_LegacyElementMixin.prototype.fire = function(type, detail, options){};
/**
* @override
* @param {?EventTarget} node Element to add event listener to.
* @param {string} eventName Name of event to listen for.
* @param {string} methodName Name of handler method on `this` to call.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.listen = function(node, eventName, methodName){};
/**
* @override
* @param {?EventTarget} node Element to remove event listener from.
* @param {string} eventName Name of event to stop listening to.
* @param {string} methodName Name of handler method on `this` to not call
     anymore.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.unlisten = function(node, eventName, methodName){};
/**
* @override
* @param {string=} direction Direction to allow scrolling
Defaults to `all`.
* @param {Element=} node Element to apply scroll direction setting.
Defaults to `this`.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.setScrollDirection = function(direction, node){};
/**
* @override
* @param {string} slctr Selector to run on this local DOM scope
* @return {Element}
*/
Polymer_LegacyElementMixin.prototype.$$ = function(slctr){};
/**
* @override
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.distributeContent = function(){};
/**
* @override
* @return {!Array.<!Node>}
*/
Polymer_LegacyElementMixin.prototype.getEffectiveChildNodes = function(){};
/**
* @override
* @param {string} selector Selector to run.
* @return {!Array.<!Node>}
*/
Polymer_LegacyElementMixin.prototype.queryDistributedElements = function(selector){};
/**
* @override
* @return {!Array.<!Node>}
*/
Polymer_LegacyElementMixin.prototype.getEffectiveChildren = function(){};
/**
* @override
* @return {string}
*/
Polymer_LegacyElementMixin.prototype.getEffectiveTextContent = function(){};
/**
* @override
* @param {string} selector Selector to run.
* @return {Node}
*/
Polymer_LegacyElementMixin.prototype.queryEffectiveChildren = function(selector){};
/**
* @override
* @param {string} selector Selector to run.
* @return {!Array.<!Node>}
*/
Polymer_LegacyElementMixin.prototype.queryAllEffectiveChildren = function(selector){};
/**
* @override
* @param {string=} slctr CSS selector to choose the desired
  `<slot>`.  Defaults to `content`.
* @return {!Array.<!Node>}
*/
Polymer_LegacyElementMixin.prototype.getContentChildNodes = function(slctr){};
/**
* @override
* @param {string=} slctr CSS selector to choose the desired
  `<content>`.  Defaults to `content`.
* @return {!Array.<!HTMLElement>}
*/
Polymer_LegacyElementMixin.prototype.getContentChildren = function(slctr){};
/**
* @override
* @param {?Node} node The element to be checked.
* @return {boolean}
*/
Polymer_LegacyElementMixin.prototype.isLightDescendant = function(node){};
/**
* @override
* @param {!Element} node The element to be checked.
* @return {boolean}
*/
Polymer_LegacyElementMixin.prototype.isLocalDescendant = function(node){};
/**
* @override
* @param {*} container Unused
* @param {*} shouldObserve Unused
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.scopeSubtree = function(container, shouldObserve){};
/**
* @override
* @param {string} property The css property name.
* @return {string}
*/
Polymer_LegacyElementMixin.prototype.getComputedStyleValue = function(property){};
/**
* @override
* @param {string} jobName String to identify the debounce job.
* @param {function (): void} callback Function that is called (with `this`
  context) when the wait time elapses.
* @param {number=} wait Optional wait time in milliseconds (ms) after the
  last signal that must elapse before invoking `callback`
* @return {!Object}
*/
Polymer_LegacyElementMixin.prototype.debounce = function(jobName, callback, wait){};
/**
* @override
* @param {string} jobName The name of the debouncer started with `debounce`
* @return {boolean}
*/
Polymer_LegacyElementMixin.prototype.isDebouncerActive = function(jobName){};
/**
* @override
* @param {string} jobName The name of the debouncer started with `debounce`
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.flushDebouncer = function(jobName){};
/**
* @override
* @param {string} jobName The name of the debouncer started with `debounce`
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.cancelDebouncer = function(jobName){};
/**
* @override
* @param {!Function} callback The callback function to run, bound to
    `this`.
* @param {number=} waitTime Time to wait before calling the
  `callback`.  If unspecified or 0, the callback will be run at microtask
  timing (before paint).
* @return {number}
*/
Polymer_LegacyElementMixin.prototype.async = function(callback, waitTime){};
/**
* @override
* @param {number} handle Handle returned from original `async` call to
  cancel.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.cancelAsync = function(handle){};
/**
* @override
* @param {string} tag HTML element tag to create.
* @param {Object=} props Object of properties to configure on the
   instance.
* @return {!Element}
*/
Polymer_LegacyElementMixin.prototype.create = function(tag, props){};
/**
* @override
* @param {string} selector Selector to test.
* @param {!Element=} node Element to test the selector against.
* @return {boolean}
*/
Polymer_LegacyElementMixin.prototype.elementMatches = function(selector, node){};
/**
* @override
* @param {string} name HTML attribute name
* @param {boolean=} bool Boolean to force the attribute on or off.
   When unspecified, the state of the attribute will be reversed.
* @return {boolean}
*/
Polymer_LegacyElementMixin.prototype.toggleAttribute = function(name, bool){};
/**
* @override
* @param {string} name CSS class name
* @param {boolean=} bool Boolean to force the class on or off.
   When unspecified, the state of the class will be reversed.
* @param {Element=} node Node to target.  Defaults to `this`.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.toggleClass = function(name, bool, node){};
/**
* @override
* @param {string} transformText Transform setting.
* @param {Element=} node Element to apply the transform to.
Defaults to `this`
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.transform = function(transformText, node){};
/**
* @override
* @param {number} x X offset.
* @param {number} y Y offset.
* @param {number} z Z offset.
* @param {Element=} node Element to apply the transform to.
Defaults to `this`.
* @return {void}
*/
Polymer_LegacyElementMixin.prototype.translate3d = function(x, y, z, node){};
/**
* @override
* @param {(string | !Array.<(number | string)>)} arrayOrPath Path to array from
    which to remove the item
  (or the array itself).
* @param {*} item Item to remove.
* @return {Array}
*/
Polymer_LegacyElementMixin.prototype.arrayDelete = function(arrayOrPath, item){};
/**
* @override
* @param {string} level One of 'log', 'warn', 'error'
* @param {Array} args Array of strings or objects to log
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._logger = function(level, args){};
/**
* @override
* @param {...*} args Array of strings or objects to log
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._log = function(args){};
/**
* @override
* @param {...*} args Array of strings or objects to log
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._warn = function(args){};
/**
* @override
* @param {...*} args Array of strings or objects to log
* @return {void}
*/
Polymer_LegacyElementMixin.prototype._error = function(args){};
/**
* @override
* @param {string} methodName Method name to associate with message
* @param {...*} args Array of strings or objects to log
* @return {Array}
*/
Polymer_LegacyElementMixin.prototype._logf = function(methodName, args){};
/**
* @interface
*/
function Polymer_MutableData(){}
/**
* @param {string} property Property name
* @param {*} value New property value
* @param {*} old Previous property value
* @return {boolean}
*/
Polymer_MutableData.prototype._shouldPropertyChange = function(property, value, old){};
/**
* @interface
*/
function Polymer_OptionalMutableData(){}
/** @type {boolean | null | undefined} */
Polymer_OptionalMutableData.prototype.mutableData;

/**
* @param {string} property Property name
* @param {*} value New property value
* @param {*} old Previous property value
* @return {boolean}
*/
Polymer_OptionalMutableData.prototype._shouldPropertyChange = function(property, value, old){};
/**
* @interface
* @extends {Polymer_ElementMixin}
*/
function Polymer_ArraySelectorMixin(){}
/** @type {Array | null | undefined} */
Polymer_ArraySelectorMixin.prototype.items;

/** @type {boolean | null | undefined} */
Polymer_ArraySelectorMixin.prototype.multi;

/** @type {(?Object | ?Array.<!Object>)} */
Polymer_ArraySelectorMixin.prototype.selected;

/** @type {?Object} */
Polymer_ArraySelectorMixin.prototype.selectedItem;

/** @type {boolean | null | undefined} */
Polymer_ArraySelectorMixin.prototype.toggle;

/**
* @override
* @return {void}
*/
Polymer_ArraySelectorMixin.prototype.clearSelection = function(){};
/**
* @override
* @param {*} item Item from `items` array to test
* @return {boolean}
*/
Polymer_ArraySelectorMixin.prototype.isSelected = function(item){};
/**
* @override
* @param {number} idx Index from `items` array to test
* @return {boolean}
*/
Polymer_ArraySelectorMixin.prototype.isIndexSelected = function(idx){};
/**
* @override
* @param {*} item Item from `items` array to deselect
* @return {void}
*/
Polymer_ArraySelectorMixin.prototype.deselect = function(item){};
/**
* @override
* @param {number} idx Index from `items` array to deselect
* @return {void}
*/
Polymer_ArraySelectorMixin.prototype.deselectIndex = function(idx){};
/**
* @override
* @param {*} item Item from `items` array to select
* @return {void}
*/
Polymer_ArraySelectorMixin.prototype.select = function(item){};
/**
* @override
* @param {number} idx Index from `items` array to select
* @return {void}
*/
Polymer_ArraySelectorMixin.prototype.selectIndex = function(idx){};
/**
* @interface
* @extends {Polymer_PropertyEffects}
*/
function Polymer_StrictBindingParser(){}
/**
* @param {string} text Text to parse from attribute or textContent
* @param {Object} templateInfo Current template metadata
* @return {Array.<!BindingPart>}
*/
Polymer_StrictBindingParser._parseBindings = function(text, templateInfo){};
/**
* @interface
* @extends {Polymer_ElementMixin}
*/
function Polymer_DisableUpgradeMixin(){}
/**
* @override
* @return {void}
*/
Polymer_DisableUpgradeMixin.prototype._initializeProperties = function(){};
/**
* @override
* @return {void}
*/
Polymer_DisableUpgradeMixin.prototype._enableProperties = function(){};
/**
* @override
* @param {string} name Attribute name.
* @param {?string} old The previous value for the attribute.
* @param {?string} value The new value for the attribute.
* @param {?string=} namespace The XML namespace for the attribute.
* @return {undefined}
*/
Polymer_DisableUpgradeMixin.prototype.attributeChangedCallback = function(name, old, value, namespace){};
/**
* @override
* @return {void}
*/
Polymer_DisableUpgradeMixin.prototype.connectedCallback = function(){};
/**
* @override
* @return {void}
*/
Polymer_DisableUpgradeMixin.prototype.disconnectedCallback = function(){};
/**
* @interface
*/
function Polymer_LegacyDataMixin(){}
/**
* @interface
*/
function Polymer_TemplatizeMixin(){}