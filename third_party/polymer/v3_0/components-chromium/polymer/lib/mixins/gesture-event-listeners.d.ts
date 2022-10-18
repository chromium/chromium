// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {dedupingMixin} from '../utils/mixin.js';

import {addListener, removeListener} from '../utils/gestures.js';

export {GestureEventListeners};


/**
 * Element class mixin that provides API for adding Polymer's cross-platform
 * gesture events to nodes.
 *
 * The API is designed to be compatible with override points implemented
 * in `TemplateStamp` such that declarative event listeners in
 * templates will support gesture events when this mixin is applied along with
 * `TemplateStamp`.
 */
declare function GestureEventListeners<T extends new (...args: any[]) => {}>(base: T): T & GestureEventListenersConstructor;

interface GestureEventListenersConstructor {
  new(...args: any[]): GestureEventListeners;
}

export {GestureEventListenersConstructor};

interface GestureEventListeners {

  /**
   * Add the event listener to the node if it is a gestures event.
   *
   * @param node Node to add event listener to
   * @param eventName Name of event
   * @param handler Listener function to add
   */
  _addEventListenerToNode(node: EventTarget, eventName: string, handler: (p0: Event) => void): void;

  /**
   * Remove the event listener to the node if it is a gestures event.
   *
   * @param node Node to remove event listener from
   * @param eventName Name of event
   * @param handler Listener function to remove
   */
  _removeEventListenerFromNode(node: EventTarget, eventName: string, handler: (p0: Event) => void): void;
}
