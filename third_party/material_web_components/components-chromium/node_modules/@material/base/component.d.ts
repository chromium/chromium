/**
 * @license
 * Copyright 2016 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
import { MDCFoundation } from './foundation';
import { CustomEventListener, EventType, SpecificEventListener } from './types';
export declare class MDCComponent<FoundationType extends MDCFoundation> {
    root: Element;
    static attachTo(root: Element): MDCComponent<MDCFoundation<{}>>;
    protected foundation: FoundationType;
    constructor(root: Element, foundation?: FoundationType, ...args: unknown[]);
    initialize(..._args: Array<unknown>): void;
    getDefaultFoundation(): FoundationType;
    initialSyncWithDOM(): void;
    destroy(): void;
    /**
     * Wrapper method to add an event listener to the component's root element. This is most useful when
     * listening for custom events.
     */
    listen<K extends EventType>(evtType: K, handler: SpecificEventListener<K>, options?: AddEventListenerOptions | boolean): void;
    listen<E extends Event>(evtType: string, handler: CustomEventListener<E>, options?: AddEventListenerOptions | boolean): void;
    /**
     * Wrapper method to remove an event listener to the component's root element. This is most useful when
     * unlistening for custom events.
     */
    unlisten<K extends EventType>(evtType: K, handler: SpecificEventListener<K>, options?: AddEventListenerOptions | boolean): void;
    unlisten<E extends Event>(evtType: string, handler: CustomEventListener<E>, options?: AddEventListenerOptions | boolean): void;
    /**
     * Fires a cross-browser-compatible custom event from the component root of the given type, with the given data.
     */
    emit<T extends object>(evtType: string, evtData: T, shouldBubble?: boolean): void;
}
export default MDCComponent;
