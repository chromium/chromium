/**
 * @license
 * Copyright (c) 2017 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt
 * The complete set of authors may be found at
 * http://polymer.github.io/AUTHORS.txt
 * The complete set of contributors may be found at
 * http://polymer.github.io/CONTRIBUTORS.txt
 * Code distributed by Google as part of the polymer project is also
 * subject to an additional IP rights grant found at
 * http://polymer.github.io/PATENTS.txt
 */
import { Part } from './part.js';
import { RenderOptions } from './render-options.js';
export declare type Primitive = null | undefined | boolean | number | string | symbol | bigint;
export declare const isPrimitive: (value: unknown) => value is Primitive;
export declare const isIterable: (value: unknown) => value is Iterable<unknown>;
/**
 * Writes attribute values to the DOM for a group of AttributeParts bound to a
 * single attribute. The value is only set once even if there are multiple parts
 * for an attribute.
 */
export declare class AttributeCommitter {
    readonly element: Element;
    readonly name: string;
    readonly strings: ReadonlyArray<string>;
    readonly parts: ReadonlyArray<AttributePart>;
    dirty: boolean;
    constructor(element: Element, name: string, strings: ReadonlyArray<string>);
    /**
     * Creates a single part. Override this to create a differnt type of part.
     */
    protected _createPart(): AttributePart;
    protected _getValue(): unknown;
    commit(): void;
}
/**
 * A Part that controls all or part of an attribute value.
 */
export declare class AttributePart implements Part {
    readonly committer: AttributeCommitter;
    value: unknown;
    constructor(committer: AttributeCommitter);
    setValue(value: unknown): void;
    commit(): void;
}
/**
 * A Part that controls a location within a Node tree. Like a Range, NodePart
 * has start and end locations and can set and update the Nodes between those
 * locations.
 *
 * NodeParts support several value types: primitives, Nodes, TemplateResults,
 * as well as arrays and iterables of those types.
 */
export declare class NodePart implements Part {
    readonly options: RenderOptions;
    startNode: Node;
    endNode: Node;
    value: unknown;
    private __pendingValue;
    constructor(options: RenderOptions);
    /**
     * Appends this part into a container.
     *
     * This part must be empty, as its contents are not automatically moved.
     */
    appendInto(container: Node): void;
    /**
     * Inserts this part after the `ref` node (between `ref` and `ref`'s next
     * sibling). Both `ref` and its next sibling must be static, unchanging nodes
     * such as those that appear in a literal section of a template.
     *
     * This part must be empty, as its contents are not automatically moved.
     */
    insertAfterNode(ref: Node): void;
    /**
     * Appends this part into a parent part.
     *
     * This part must be empty, as its contents are not automatically moved.
     */
    appendIntoPart(part: NodePart): void;
    /**
     * Inserts this part after the `ref` part.
     *
     * This part must be empty, as its contents are not automatically moved.
     */
    insertAfterPart(ref: NodePart): void;
    setValue(value: unknown): void;
    commit(): void;
    private __insert;
    private __commitNode;
    private __commitText;
    private __commitTemplateResult;
    private __commitIterable;
    clear(startNode?: Node): void;
}
/**
 * Implements a boolean attribute, roughly as defined in the HTML
 * specification.
 *
 * If the value is truthy, then the attribute is present with a value of
 * ''. If the value is falsey, the attribute is removed.
 */
export declare class BooleanAttributePart implements Part {
    readonly element: Element;
    readonly name: string;
    readonly strings: readonly string[];
    value: unknown;
    private __pendingValue;
    constructor(element: Element, name: string, strings: readonly string[]);
    setValue(value: unknown): void;
    commit(): void;
}
/**
 * Sets attribute values for PropertyParts, so that the value is only set once
 * even if there are multiple parts for a property.
 *
 * If an expression controls the whole property value, then the value is simply
 * assigned to the property under control. If there are string literals or
 * multiple expressions, then the strings are expressions are interpolated into
 * a string first.
 */
export declare class PropertyCommitter extends AttributeCommitter {
    readonly single: boolean;
    constructor(element: Element, name: string, strings: ReadonlyArray<string>);
    protected _createPart(): PropertyPart;
    protected _getValue(): unknown;
    commit(): void;
}
export declare class PropertyPart extends AttributePart {
}
declare type EventHandlerWithOptions = EventListenerOrEventListenerObject & Partial<AddEventListenerOptions>;
export declare class EventPart implements Part {
    readonly element: Element;
    readonly eventName: string;
    readonly eventContext?: EventTarget;
    value: undefined | EventHandlerWithOptions;
    private __options?;
    private __pendingValue;
    private readonly __boundHandleEvent;
    constructor(element: Element, eventName: string, eventContext?: EventTarget);
    setValue(value: undefined | EventHandlerWithOptions): void;
    commit(): void;
    handleEvent(event: Event): void;
}
export {};
//# sourceMappingURL=parts.d.ts.map
