/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An implementation of {@link goog.events.Listenable} that does
 * not need to be disposed.
 */

goog.provide('goog.labs.events.NonDisposableEventTarget');


goog.require('goog.array');
goog.require('goog.asserts');
goog.require('goog.events.Event');
goog.require('goog.events.Listenable');
goog.require('goog.events.ListenerMap');
goog.require('goog.object');



/**
 * An implementation of `goog.events.Listenable` with full W3C
 * EventTarget-like support (capture/bubble mechanism, stopping event
 * propagation, preventing default actions).
 *
 * You may subclass this class to turn your class into a Listenable.
 *
 * Unlike {@link goog.events.EventTarget}, this class does not implement
 * {@link goog.disposable.IDisposable}. Instances of this class that have had
 * It is not necessary to call {@link goog.dispose}
 * or {@link #removeAllListeners} in order for an instance of this class
 * to be garbage collected.
 *
 * Unless propagation is stopped, an event dispatched by an
 * EventTarget will bubble to the parent returned by
 * `getParentEventTarget`. To set the parent, call
 * `setParentEventTarget`. Subclasses that don't support
 * changing the parent can override the setter to throw an error.
 *
 * Example usage:
 * <pre>
 *   var source = new goog.labs.events.NonDisposableEventTarget();
 *   function handleEvent(e) {
 *     alert('Type: ' + e.type + '; Target: ' + e.target);
 *   }
 *   source.listen('foo', handleEvent);
 *   source.dispatchEvent('foo'); // will call handleEvent
 * </pre>
 *
 * TODO(user): Consider a more modern, less viral
 * (not based on inheritance) replacement of goog.Disposable, which will allow
 * goog.events.EventTarget to not be disposable.
 *
 * @constructor
 * @implements {goog.events.Listenable}
 * @final
 */
goog.labs.events.NonDisposableEventTarget = function() {
  'use strict';
  /**
   * Maps of event type to an array of listeners.
   * @private {!goog.events.ListenerMap}
   */
  this.eventTargetListeners_ = new goog.events.ListenerMap(this);
};
goog.events.Listenable.addImplementation(
    goog.labs.events.NonDisposableEventTarget);


/**
 * An artificial cap on the number of ancestors you can have. This is mainly
 * for loop detection.
 * @const {number}
 * @private
 */
goog.labs.events.NonDisposableEventTarget.MAX_ANCESTORS_ = 1000;


/**
 * Parent event target, used during event bubbling.
 * @private {?goog.events.Listenable}
 */
goog.labs.events.NonDisposableEventTarget.prototype.parentEventTarget_ = null;


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.getParentEventTarget =
    function() {
  'use strict';
  return this.parentEventTarget_;
};


/**
 * Sets the parent of this event target to use for capture/bubble
 * mechanism.
 * @param {goog.events.Listenable} parent Parent listenable (null if none).
 */
goog.labs.events.NonDisposableEventTarget.prototype.setParentEventTarget =
    function(parent) {
  'use strict';
  this.parentEventTarget_ = parent;
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.dispatchEvent = function(
    e) {
  'use strict';
  this.assertInitialized_();
  let ancestor = this.getParentEventTarget();
  let ancestorsTree;

  if (ancestor) {
    ancestorsTree = [];
    let ancestorCount = 1;
    for (; ancestor; ancestor = ancestor.getParentEventTarget()) {
      ancestorsTree.push(ancestor);
      goog.asserts.assert(
          (++ancestorCount <
           goog.labs.events.NonDisposableEventTarget.MAX_ANCESTORS_),
          'infinite loop');
    }
  }

  return goog.labs.events.NonDisposableEventTarget.dispatchEventInternal_(
      this, e, ancestorsTree);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.listen = function(
    type, listener, opt_useCapture, opt_listenerScope) {
  'use strict';
  this.assertInitialized_();
  return this.eventTargetListeners_.add(
      String(type), listener, false /* callOnce */, opt_useCapture,
      opt_listenerScope);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.listenOnce = function(
    type, listener, opt_useCapture, opt_listenerScope) {
  'use strict';
  return this.eventTargetListeners_.add(
      String(type), listener, true /* callOnce */, opt_useCapture,
      opt_listenerScope);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.unlisten = function(
    type, listener, opt_useCapture, opt_listenerScope) {
  'use strict';
  return this.eventTargetListeners_.remove(
      String(type), listener, opt_useCapture, opt_listenerScope);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.unlistenByKey = function(
    key) {
  'use strict';
  return this.eventTargetListeners_.removeByKey(key);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.removeAllListeners =
    function(opt_type) {
  'use strict';
  return this.eventTargetListeners_.removeAll(opt_type);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.fireListeners = function(
    type, capture, eventObject) {
  'use strict';
  // TODO(chrishenry): Original code avoids array creation when there
  // is no listener, so we do the same. If this optimization turns
  // out to be not required, we can replace this with
  // getListeners(type, capture) instead, which is simpler.
  let listenerArray = this.eventTargetListeners_.listeners[String(type)];
  if (!listenerArray) {
    return true;
  }
  listenerArray = goog.array.clone(listenerArray);

  let rv = true;
  for (let i = 0; i < listenerArray.length; ++i) {
    const listener = listenerArray[i];
    // We might not have a listener if the listener was removed.
    if (listener && !listener.removed && listener.capture == capture) {
      const listenerFn = listener.listener;
      const listenerHandler = listener.handler || listener.src;

      if (listener.callOnce) {
        this.unlistenByKey(listener);
      }
      /** @suppress {missingProperties} */
      rv = listenerFn.call(listenerHandler, eventObject) !== false && rv;
    }
  }

  return rv && !eventObject.defaultPrevented;
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.getListeners = function(
    type, capture) {
  'use strict';
  return this.eventTargetListeners_.getListeners(String(type), capture);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.getListener = function(
    type, listener, capture, opt_listenerScope) {
  'use strict';
  return this.eventTargetListeners_.getListener(
      String(type), listener, capture, opt_listenerScope);
};


/** @override */
goog.labs.events.NonDisposableEventTarget.prototype.hasListener = function(
    opt_type, opt_capture) {
  'use strict';
  const id = (opt_type !== undefined) ? String(opt_type) : undefined;
  return this.eventTargetListeners_.hasListener(id, opt_capture);
};


/**
 * Asserts that the event target instance is initialized properly.
 * @private
 */
goog.labs.events.NonDisposableEventTarget.prototype.assertInitialized_ =
    function() {
  'use strict';
  goog.asserts.assert(
      this.eventTargetListeners_,
      'Event target is not initialized. Did you call the superclass ' +
          '(goog.labs.events.NonDisposableEventTarget) constructor?');
};


/**
 * Dispatches the given event on the ancestorsTree.
 *
 * TODO(chrishenry): Look for a way to reuse this logic in
 * goog.events, if possible.
 * @param {!Object} target The target to dispatch on.
 * @param {goog.events.Event|Object|string} e The event object.
 * @param {Array<goog.events.Listenable>=} opt_ancestorsTree The ancestors
 *     tree of the target, in reverse order from the closest ancestor
 *     to the root event target. May be null if the target has no ancestor.
 * @return {boolean} If anyone called preventDefault on the event object (or
 *     if any of the listeners returns false) this will also return false.
 * @private
 * @suppress {strictMissingProperties} Part of the go/strict_warnings_migration
 */
goog.labs.events.NonDisposableEventTarget.dispatchEventInternal_ = function(
    target, e, opt_ancestorsTree) {
  'use strict';
  const type = e.type || /** @type {string} */ (e);

  // If accepting a string or object, create a custom event object so that
  // preventDefault and stopPropagation work with the event.
  if (typeof e === 'string') {
    e = new goog.events.Event(e, target);
  } else if (!(e instanceof goog.events.Event)) {
    const oldEvent = e;
    e = new goog.events.Event(type, target);
    goog.object.extend(e, oldEvent);
  } else {
    e.target = e.target || target;
  }

  let currentTarget;
  let rv = true;


  // Executes all capture listeners on the ancestors, if any.
  if (opt_ancestorsTree) {
    for (let i = opt_ancestorsTree.length - 1;
         !e.hasPropagationStopped() && i >= 0; i--) {
      currentTarget = e.currentTarget = opt_ancestorsTree[i];
      rv = currentTarget.fireListeners(type, true, e) && rv;
    }
  }

  // Executes capture and bubble listeners on the target.
  if (!e.hasPropagationStopped()) {
    currentTarget = e.currentTarget = target;
    rv = currentTarget.fireListeners(type, true, e) && rv;
    if (!e.hasPropagationStopped()) {
      rv = currentTarget.fireListeners(type, false, e) && rv;
    }
  }

  // Executes all bubble listeners on the ancestors, if any.
  if (opt_ancestorsTree) {
    for (let i = 0; !e.hasPropagationStopped() && i < opt_ancestorsTree.length;
         i++) {
      currentTarget = e.currentTarget = opt_ancestorsTree[i];
      rv = currentTarget.fireListeners(type, false, e) && rv;
    }
  }

  return rv;
};
