import { ReactiveControllerHost } from '@lit/reactive-element/reactive-controller.js';
export interface TaskFunctionOptions {
    signal: AbortSignal;
}
export type TaskFunction<D extends ReadonlyArray<unknown>, R = unknown> = (args: D, options: TaskFunctionOptions) => R | typeof initialState | Promise<R | typeof initialState>;
export type ArgsFunction<D extends ReadonlyArray<unknown>> = () => D;
export { ArgsFunction as DepsFunction };
/**
 * States for task status
 */
export declare const TaskStatus: {
    readonly INITIAL: 0;
    readonly PENDING: 1;
    readonly COMPLETE: 2;
    readonly ERROR: 3;
};
/**
 * A special value that can be returned from task functions to reset the task
 * status to INITIAL.
 */
export declare const initialState: unique symbol;
export type TaskStatus = (typeof TaskStatus)[keyof typeof TaskStatus];
export type StatusRenderer<R> = {
    initial?: () => unknown;
    pending?: () => unknown;
    complete?: (value: R) => unknown;
    error?: (error: unknown) => unknown;
};
export interface TaskConfig<T extends ReadonlyArray<unknown>, R> {
    task: TaskFunction<T, R>;
    args?: ArgsFunction<T>;
    /**
     * Determines if the task is run automatically when arguments change after a
     * host update. Default to `true`.
     *
     * If `true`, the task checks arguments during the host update (after
     * `willUpdate()` and before `update()` in Lit) and runs if they change. For
     * a task to see argument changes they must be done in `willUpdate()` or
     * earlier. The host element can see task status changes caused by its own
     * current update.
     *
     * If `'afterUpdate'`, the task checks arguments and runs _after_ the host
     * update. This means that the task can see host changes done in update, such
     * as rendered DOM. The host element can not see task status changes caused
     * by its own update, so the task must trigger a second host update to make
     * those changes renderable.
     *
     * Note: `'afterUpdate'` is unlikely to be SSR compatible in the future.
     *
     * If `false`, the task is not run automatically, and must be run with the
     * {@linkcode run} method.
     */
    autoRun?: boolean | 'afterUpdate';
    /**
     * A function that determines if the current arg and previous args arrays are
     * equal. If the argsEqual function returns true, the task will not auto-run.
     *
     * The default is {@linkcode shallowArrayEquals}. {@linkcode deepArrayEquals}
     * is also available.
     */
    argsEqual?: (oldArgs: T, newArgs: T) => boolean;
    /**
     * If initialValue is provided, the task is initialized to the COMPLETE
     * status and the value is set to initialData.
     *
     * Initial args should be coherent with the initialValue, since they are
     * assumed to be the args that would produce that value. When autoRun is
     * `true` the task will not auto-run again until the args change.
     */
    initialValue?: R;
    onComplete?: (value: R) => unknown;
    onError?: (error: unknown) => unknown;
}
/**
 * A controller that performs an asynchronous task (like a fetch) when its
 * host element updates.
 *
 * Task requests an update on the host element when the task starts and
 * completes so that the host can render the task status, value, and error as
 * the task runs.
 *
 * The task function must be supplied and can take a list of arguments. The
 * arguments are given to the Task as a function that returns a list of values,
 * which is run and checked for changes on every host update.
 *
 * The `value` property reports the completed value, and the `error` property
 * an error state if one occurs. The `status` property can be checked for
 * status and is of type `TaskStatus` which has states for initial, pending,
 * complete, and error.
 *
 * The `render` method accepts an object with optional methods corresponding
 * to the task statuses to easily render different templates for each task
 * status.
 *
 * The task is run automatically when its arguments change; however, this can
 * be customized by setting `autoRun` to false and calling `run` explicitly
 * to run the task.
 *
 * For a task to see state changes in the current update pass of the host
 * element, those changes must be made in `willUpdate()`. State changes in
 * `update()` or `updated()` will not be visible to the task until the next
 * update pass.
 *
 * @example
 *
 * ```ts
 * class MyElement extends LitElement {
 *   url = 'example.com/api';
 *   id = 0;
 *
 *   task = new Task(
 *     this,
 *     {
 *       task: async ([url, id]) => {
 *         const response = await fetch(`${this.url}?id=${this.id}`);
 *         if (!response.ok) {
 *           throw new Error(response.statusText);
 *         }
 *         return response.json();
 *       },
 *       args: () => [this.id, this.url],
 *     }
 *   );
 *
 *   render() {
 *     return this.task.render({
 *       pending: () => html`<p>Loading...</p>`,
 *       complete: (value) => html`<p>Result: ${value}</p>`
 *     });
 *   }
 * }
 * ```
 */
export declare class Task<T extends ReadonlyArray<unknown> = ReadonlyArray<unknown>, R = unknown> {
    private _previousArgs?;
    private _task;
    private _argsFn?;
    private _argsEqual;
    private _callId;
    private _host;
    private _value?;
    private _error?;
    private _abortController?;
    private _onComplete?;
    private _onError?;
    status: TaskStatus;
    /**
     * Determines if the task is run automatically when arguments change after a
     * host update. Default to `true`.
     *
     * @see {@link TaskConfig.autoRun} for more information.
     */
    autoRun: boolean | 'afterUpdate';
    /**
     * A Promise that resolve when the current task run is complete.
     *
     * If a new task run is started while a previous run is pending, the Promise
     * is kept and only resolved when the new run is completed.
     */
    get taskComplete(): Promise<R>;
    private _resolveTaskComplete?;
    private _rejectTaskComplete?;
    private _taskComplete?;
    constructor(host: ReactiveControllerHost, task: TaskFunction<T, R>, args?: ArgsFunction<T>);
    constructor(host: ReactiveControllerHost, task: TaskConfig<T, R>);
    hostUpdate(): void;
    hostUpdated(): void;
    private _getArgs;
    /**
     * Determines if the task should run when it's triggered because of a
     * host update, and runs the task if it should.
     *
     * A task should run when its arguments change from the previous run, based on
     * the args equality function.
     *
     * This method is side-effectful: it stores the new args as the previous args.
     */
    private _performTask;
    /**
     * Runs a task manually.
     *
     * This can be useful for running tasks in response to events as opposed to
     * automatically running when host element state changes.
     *
     * @param args an optional set of arguments to use for this task run. If args
     *     is not given, the args function is called to get the arguments for
     *     this run.
     */
    run(args?: T): Promise<void>;
    /**
     * Aborts the currently pending task run by aborting the AbortSignal
     * passed to the task function.
     *
     * Aborting a task does nothing if the task is not running: ie, in the
     * complete, error, or initial states.
     *
     * Aborting a task does not automatically cancel the task function. The task
     * function must be written to accept the AbortSignal and either forward it
     * to other APIs like `fetch()`, or handle cancellation manually by using
     * [`signal.throwIfAborted()`]{@link https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/throwIfAborted}
     * or the
     * [`abort`]{@link https://developer.mozilla.org/en-US/docs/Web/API/AbortSignal/abort_event}
     * event.
     *
     * @param reason The reason for aborting. Passed to
     *     `AbortController.abort()`.
     */
    abort(reason?: unknown): void;
    /**
     * The result of the previous task run, if it resolved.
     *
     * Is `undefined` if the task has not run yet, or if the previous run errored.
     */
    get value(): R | undefined;
    /**
     * The error from the previous task run, if it rejected.
     *
     * Is `undefined` if the task has not run yet, or if the previous run
     * completed successfully.
     */
    get error(): unknown;
    render<T extends StatusRenderer<R>>(renderer: T): MaybeReturnType<T["initial"]> | MaybeReturnType<T["pending"]> | MaybeReturnType<T["complete"]> | MaybeReturnType<T["error"]>;
}
type MaybeReturnType<F> = F extends (...args: never[]) => infer R ? R : undefined;
export declare const shallowArrayEquals: <T extends readonly unknown[]>(oldArgs: T, newArgs: T) => boolean;
//# sourceMappingURL=task.d.ts.map