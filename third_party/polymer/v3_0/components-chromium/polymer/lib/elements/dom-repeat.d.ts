// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

import {PolymerElement} from '../../polymer-element.js';

import {TemplateInstanceBase, templatize, modelForElement} from '../utils/templatize.js';

import {Debouncer} from '../utils/debounce.js';

import {enqueueDebouncer, flush} from '../utils/flush.js';

import {OptionalMutableData} from '../mixins/mutable-data.js';

import {matches, translate} from '../utils/path.js';

import {timeOut, microTask} from '../utils/async.js';

import {hideElementsGlobally} from '../utils/hide-template-controls.js';

export {DomRepeat};

/**
 * The `<dom-repeat>` element will automatically stamp and binds one instance
 * of template content to each object in a user-provided array.
 * `dom-repeat` accepts an `items` property, and one instance of the template
 * is stamped for each item into the DOM at the location of the `dom-repeat`
 * element.  The `item` property will be set on each instance's binding
 * scope, thus templates should bind to sub-properties of `item`.
 *
 * Example:
 *
 * ```html
 * <dom-module id="employee-list">
 *
 *   <template>
 *
 *     <div> Employee list: </div>
 *     <dom-repeat items="{{employees}}">
 *       <template>
 *         <div>First name: <span>{{item.first}}</span></div>
 *         <div>Last name: <span>{{item.last}}</span></div>
 *       </template>
 *     </dom-repeat>
 *
 *   </template>
 *
 * </dom-module>
 * ```
 *
 * With the following custom element definition:
 *
 * ```js
 * class EmployeeList extends PolymerElement {
 *   static get is() { return 'employee-list'; }
 *   static get properties() {
 *     return {
 *       employees: {
 *         value() {
 *           return [
 *             {first: 'Bob', last: 'Smith'},
 *             {first: 'Sally', last: 'Johnson'},
 *             ...
 *           ];
 *         }
 *       }
 *     };
 *   }
 * }
 * ```
 *
 * Notifications for changes to items sub-properties will be forwarded to template
 * instances, which will update via the normal structured data notification system.
 *
 * Mutations to the `items` array itself should be made using the Array
 * mutation API's on the PropertyEffects mixin (`push`, `pop`, `splice`,
 * `shift`, `unshift`), and template instances will be kept in sync with the
 * data in the array.
 *
 * Events caught by event handlers within the `dom-repeat` template will be
 * decorated with a `model` property, which represents the binding scope for
 * each template instance.  The model should be used to manipulate data on the
 * instance, for example `event.model.set('item.checked', true);`.
 *
 * Alternatively, the model for a template instance for an element stamped by
 * a `dom-repeat` can be obtained using the `modelForElement` API on the
 * `dom-repeat` that stamped it, for example
 * `this.$.domRepeat.modelForElement(event.target).set('item.checked', true);`.
 * This may be useful for manipulating instance data of event targets obtained
 * by event handlers on parents of the `dom-repeat` (event delegation).
 *
 * A view-specific filter/sort may be applied to each `dom-repeat` by supplying a
 * `filter` and/or `sort` property.  This may be a string that names a function on
 * the host, or a function may be assigned to the property directly.  The functions
 * should implemented following the standard `Array` filter/sort API.
 *
 * In order to re-run the filter or sort functions based on changes to sub-fields
 * of `items`, the `observe` property may be set as a space-separated list of
 * `item` sub-fields that should cause a re-filter/sort when modified.  If
 * the filter or sort function depends on properties not contained in `items`,
 * the user should observe changes to those properties and call `render` to update
 * the view based on the dependency change.
 *
 * For example, for an `dom-repeat` with a filter of the following:
 *
 * ```js
 * isEngineer(item) {
 *   return item.type == 'engineer' || item.manager.type == 'engineer';
 * }
 * ```
 *
 * Then the `observe` property should be configured as follows:
 *
 * ```html
 * <dom-repeat items="{{employees}}" filter="isEngineer" observe="type manager.type">
 * ```
 */
declare class DomRepeat extends
  OptionalMutableData(
  PolymerElement) {
  _templateInfo: TemplateInfo|null;

  /**
   * An array containing items determining how many instances of the template
   * to stamp and that that each template instance should bind to.
   */
  items: any[]|null|undefined;

  /**
   * The name of the variable to add to the binding scope for the array
   * element associated with a given template instance.
   */
  as: string|null|undefined;

  /**
   * The name of the variable to add to the binding scope with the index
   * of the instance in the sorted and filtered list of rendered items.
   * Note, for the index in the `this.items` array, use the value of the
   * `itemsIndexAs` property.
   */
  indexAs: string|null|undefined;

  /**
   * The name of the variable to add to the binding scope with the index
   * of the instance in the `this.items` array. Note, for the index of
   * this instance in the sorted and filtered list of rendered items,
   * use the value of the `indexAs` property.
   */
  itemsIndexAs: string|null|undefined;

  /**
   * A function that should determine the sort order of the items.  This
   * property should either be provided as a string, indicating a method
   * name on the element's host, or else be an actual function.  The
   * function should match the sort function passed to `Array.sort`.
   * Using a sort function has no effect on the underlying `items` array.
   */
  sort: Function|null|undefined;

  /**
   * A function that can be used to filter items out of the view.  This
   * property should either be provided as a string, indicating a method
   * name on the element's host, or else be an actual function.  The
   * function should match the sort function passed to `Array.filter`.
   * Using a filter function has no effect on the underlying `items` array.
   */
  filter: Function|null|undefined;

  /**
   * When using a `filter` or `sort` function, the `observe` property
   * should be set to a space-separated list of the names of item
   * sub-fields that should trigger a re-sort or re-filter when changed.
   * These should generally be fields of `item` that the sort or filter
   * function depends on.
   */
  observe: string|null|undefined;

  /**
   * When using a `filter` or `sort` function, the `delay` property
   * determines a debounce time in ms after a change to observed item
   * properties that must pass before the filter or sort is re-run.
   * This is useful in rate-limiting shuffling of the view when
   * item changes may be frequent.
   */
  delay: number|null|undefined;

  /**
   * Count of currently rendered items after `filter` (if any) has been applied.
   * If "chunking mode" is enabled, `renderedItemCount` is updated each time a
   * set of template instances is rendered.
   */
  readonly renderedItemCount: number|null|undefined;

  /**
   * When greater than zero, defines an initial count of template instances
   * to render after setting the `items` array, before the next paint, and
   * puts the `dom-repeat` into "chunking mode".  The remaining items (and
   * any future items as a result of pushing onto the array) will be created
   * and rendered incrementally at each animation frame thereof until all
   * instances have been rendered.
   */
  initialCount: number|null|undefined;

  /**
   * When `initialCount` is used, this property defines a frame rate (in
   * fps) to target by throttling the number of instances rendered each
   * frame to not exceed the budget for the target frame rate.  The
   * framerate is effectively the number of `requestAnimationFrame`s that
   * it tries to allow to actually fire in a given second. It does this
   * by measuring the time between `rAF`s and continuously adjusting the
   * number of items created each `rAF` to maintain the target framerate.
   * Setting this to a higher number allows lower latency and higher
   * throughput for event handlers and other tasks, but results in a
   * longer time for the remaining items to complete rendering.
   */
  targetFramerate: number|null|undefined;
  readonly _targetFrameTime: number|null|undefined;

  /**
   * When the global `suppressTemplateNotifications` setting is used, setting
   * `notifyDomChange: true` will enable firing `dom-change` events on this
   * element.
   */
  notifyDomChange: boolean|null|undefined;

  /**
   * When chunking is enabled via `initialCount` and the `items` array is
   * set to a new array, this flag controls whether the previously rendered
   * instances are reused or not.
   *
   * When `true`, any previously rendered template instances are updated in
   * place to their new item values synchronously in one shot, and then any
   * further items (if any) are chunked out.  When `false`, the list is
   * returned back to its `initialCount` (any instances over the initial
   * count are discarded) and the remainder of the list is chunked back in.
   * Set this to `true` to avoid re-creating the list and losing scroll
   * position, although note that when changing the list to completely
   * different data the render thread will be blocked until all existing
   * instances are updated to their new data.
   */
  reuseChunkedInstances: boolean|null|undefined;
  connectedCallback(): void;
  disconnectedCallback(): void;

  /**
   * Forces the element to render its content. Normally rendering is
   * asynchronous to a provoking change. This is done for efficiency so
   * that multiple changes trigger only a single render. The render method
   * should be called if, for example, template rendering is required to
   * validate application state.
   */
  render(): void;

  /**
   * Shows or hides the template instance top level child elements. For
   * text nodes, `textContent` is removed while "hidden" and replaced when
   * "shown."
   *
   * @param hidden Set to true to hide the children;
   * set to false to show them.
   */
  _showHideChildren(hidden: boolean): void;

  /**
   * Returns the item associated with a given element stamped by
   * this `dom-repeat`.
   *
   * Note, to modify sub-properties of the item,
   * `modelForElement(el).set('item.<sub-prop>', value)`
   * should be used.
   *
   * @param el Element for which to return the item.
   * @returns Item associated with the element.
   */
  itemForElement(el: HTMLElement): any;

  /**
   * Returns the inst index for a given element stamped by this `dom-repeat`.
   * If `sort` is provided, the index will reflect the sorted order (rather
   * than the original array order).
   *
   * @param el Element for which to return the index.
   * @returns Row index associated with the element (note this may
   *   not correspond to the array index if a user `sort` is applied).
   */
  indexForElement(el: HTMLElement): number|null;

  /**
   * Returns the template "model" associated with a given element, which
   * serves as the binding scope for the template instance the element is
   * contained in. A template model
   * should be used to manipulate data associated with this template instance.
   *
   * Example:
   *
   *   let model = modelForElement(el);
   *   if (model.index < 10) {
   *     model.set('item.checked', true);
   *   }
   *
   * @param el Element for which to return a template model.
   * @returns Model representing the binding scope for
   *   the element.
   */
  modelForElement(el: HTMLElement): TemplateInstanceBase|null;
}

interface DomRepeatItem<M> {
  model: {
    item: M,
    index: number,
  }
}

/** Event interface for dom-repeat events. */
export type DomRepeatEvent<M, E extends Event = Event> = DomRepeatItem<M>&E;

declare global {

  interface HTMLElementTagNameMap {
    "dom-repeat": DomRepeat;
  }
}

import {TemplateInfo} from '../../interfaces';
