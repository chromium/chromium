// tslint:disable:variable-name Describing an API that's defined elsewhere.
// tslint:disable:no-any describes the API as best we are able today

export {flush};


/**
 * Flushes all `beforeNextRender` tasks, followed by all `afterNextRender`
 * tasks.
 */
declare function flush(): void;

export {beforeNextRender};


/**
 * Enqueues a callback which will be run before the next render, at
 * `requestAnimationFrame` timing.
 *
 * This method is useful for enqueuing work that requires DOM measurement,
 * since measurement may not be reliable in custom element callbacks before
 * the first render, as well as for batching measurement tasks in general.
 *
 * Tasks in this queue may be flushed by calling `flush()`.
 */
declare function beforeNextRender(context: any, callback: (...p0: any[]) => void, args?: any[]): void;

export {afterNextRender};


/**
 * Enqueues a callback which will be run after the next render, equivalent
 * to one task (`setTimeout`) after the next `requestAnimationFrame`.
 *
 * This method is useful for tuning the first-render performance of an
 * element or application by deferring non-critical work until after the
 * first paint.  Typical non-render-critical work may include adding UI
 * event listeners and aria attributes.
 */
declare function afterNextRender(context: any, callback: (...p0: any[]) => void, args?: any[]): void;
