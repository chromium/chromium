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
declare global {
    var JSCompiler_renameProperty: <P extends PropertyKey>(prop: P, _obj: unknown) => P;
    interface Window {
        JSCompiler_renameProperty: typeof JSCompiler_renameProperty;
    }
}
/**
 * Converts property values to and from attribute values.
 */
export interface ComplexAttributeConverter<Type = unknown, TypeHint = unknown> {
    /**
     * Function called to convert an attribute value to a property
     * value.
     */
    fromAttribute?(value: string | null, type?: TypeHint): Type;
    /**
     * Function called to convert a property value to an attribute
     * value.
     *
     * It returns unknown instead of string, to be compatible with
     * https://github.com/WICG/trusted-types (and similar efforts).
     */
    toAttribute?(value: Type, type?: TypeHint): unknown;
}
declare type AttributeConverter<Type = unknown, TypeHint = unknown> = ComplexAttributeConverter<Type> | ((value: string | null, type?: TypeHint) => Type);
/**
 * Defines options for a property accessor.
 */
export interface PropertyDeclaration<Type = unknown, TypeHint = unknown> {
    /**
     * Indicates how and whether the property becomes an observed attribute.
     * If the value is `false`, the property is not added to `observedAttributes`.
     * If true or absent, the lowercased property name is observed (e.g. `fooBar`
     * becomes `foobar`). If a string, the string value is observed (e.g
     * `attribute: 'foo-bar'`).
     */
    readonly attribute?: boolean | string;
    /**
     * Indicates the type of the property. This is used only as a hint for the
     * `converter` to determine how to convert the attribute
     * to/from a property.
     */
    readonly type?: TypeHint;
    /**
     * Indicates how to convert the attribute to/from a property. If this value
     * is a function, it is used to convert the attribute value a the property
     * value. If it's an object, it can have keys for `fromAttribute` and
     * `toAttribute`. If no `toAttribute` function is provided and
     * `reflect` is set to `true`, the property value is set directly to the
     * attribute. A default `converter` is used if none is provided; it supports
     * `Boolean`, `String`, `Number`, `Object`, and `Array`. Note,
     * when a property changes and the converter is used to update the attribute,
     * the property is never updated again as a result of the attribute changing,
     * and vice versa.
     */
    readonly converter?: AttributeConverter<Type, TypeHint>;
    /**
     * Indicates if the property should reflect to an attribute.
     * If `true`, when the property is set, the attribute is set using the
     * attribute name determined according to the rules for the `attribute`
     * property option and the value of the property converted using the rules
     * from the `converter` property option.
     */
    readonly reflect?: boolean;
    /**
     * A function that indicates if a property should be considered changed when
     * it is set. The function should take the `newValue` and `oldValue` and
     * return `true` if an update should be requested.
     */
    hasChanged?(value: Type, oldValue: Type): boolean;
    /**
     * Indicates whether an accessor will be created for this property. By
     * default, an accessor will be generated for this property that requests an
     * update when set. If this flag is `true`, no accessor will be created, and
     * it will be the user's responsibility to call
     * `this.requestUpdate(propertyName, oldValue)` to request an update when
     * the property changes.
     */
    readonly noAccessor?: boolean;
}
/**
 * Map of properties to PropertyDeclaration options. For each property an
 * accessor is made, and the property is processed according to the
 * PropertyDeclaration options.
 */
export interface PropertyDeclarations {
    readonly [key: string]: PropertyDeclaration;
}
/**
 * Map of changed properties with old values. Takes an optional generic
 * interface corresponding to the declared element properties.
 */
export declare type PropertyValues<T = any> = keyof T extends PropertyKey ? Map<keyof T, unknown> : never;
export declare const defaultConverter: ComplexAttributeConverter;
export interface HasChanged {
    (value: unknown, old: unknown): boolean;
}
/**
 * Change function that returns true if `value` is different from `oldValue`.
 * This method is used as the default for a property's `hasChanged` function.
 */
export declare const notEqual: HasChanged;
/**
 * The Closure JS Compiler doesn't currently have good support for static
 * property semantics where "this" is dynamic (e.g.
 * https://github.com/google/closure-compiler/issues/3177 and others) so we use
 * this hack to bypass any rewriting by the compiler.
 */
declare const finalized = "finalized";
/**
 * Base element class which manages element properties and attributes. When
 * properties change, the `update` method is asynchronously called. This method
 * should be supplied by subclassers to render updates as desired.
 * @noInheritDoc
 */
export declare abstract class UpdatingElement extends HTMLElement {
    /**
     * Maps attribute names to properties; for example `foobar` attribute to
     * `fooBar` property. Created lazily on user subclasses when finalizing the
     * class.
     */
    private static _attributeToPropertyMap;
    /**
     * Marks class as having finished creating properties.
     */
    protected static [finalized]: boolean;
    /**
     * Memoized list of all class properties, including any superclass properties.
     * Created lazily on user subclasses when finalizing the class.
     */
    private static _classProperties?;
    /**
     * User-supplied object that maps property names to `PropertyDeclaration`
     * objects containing options for configuring the property.
     */
    static properties: PropertyDeclarations;
    /*
    * Returns a list of attributes corresponding to the registered properties.
    * @nocollapse
    */
    static readonly observedAttributes: string[];
    /**
     * Ensures the private `_classProperties` property metadata is created.
     * In addition to `finalize` this is also called in `createProperty` to
     * ensure the `@property` decorator can add property metadata.
     */
    /** @nocollapse */
    private static _ensureClassProperties;
    /**
     * Creates a property accessor on the element prototype if one does not exist
     * and stores a PropertyDeclaration for the property with the given options.
     * The property setter calls the property's `hasChanged` property option
     * or uses a strict identity check to determine whether or not to request
     * an update.
     *
     * This method may be overridden to customize properties; however,
     * when doing so, it's important to call `super.createProperty` to ensure
     * the property is setup correctly. This method calls
     * `getPropertyDescriptor` internally to get a descriptor to install.
     * To customize what properties do when they are get or set, override
     * `getPropertyDescriptor`. To customize the options for a property,
     * implement `createProperty` like this:
     *
     * static createProperty(name, options) {
     *   options = Object.assign(options, {myOption: true});
     *   super.createProperty(name, options);
     * }
     *
     * @nocollapse
     */
    static createProperty(name: PropertyKey, options?: PropertyDeclaration): void;
    /**
     * Returns a property descriptor to be defined on the given named property.
     * If no descriptor is returned, the property will not become an accessor.
     * For example,
     *
     *   class MyElement extends LitElement {
     *     static getPropertyDescriptor(name, key, options) {
     *       const defaultDescriptor =
     *           super.getPropertyDescriptor(name, key, options);
     *       const setter = defaultDescriptor.set;
     *       return {
     *         get: defaultDescriptor.get,
     *         set(value) {
     *           setter.call(this, value);
     *           // custom action.
     *         },
     *         configurable: true,
     *         enumerable: true
     *       }
     *     }
     *   }
     *
     * @nocollapse
     */
    protected static getPropertyDescriptor(name: PropertyKey, key: string | symbol, options: PropertyDeclaration): {
        get(): any;
        set(this: UpdatingElement, value: unknown): void;
        configurable: boolean;
        enumerable: boolean;
    };
    /**
     * Returns the property options associated with the given property.
     * These options are defined with a PropertyDeclaration via the `properties`
     * object or the `@property` decorator and are registered in
     * `createProperty(...)`.
     *
     * Note, this method should be considered "final" and not overridden. To
     * customize the options for a given property, override `createProperty`.
     *
     * @nocollapse
     * @final
     */
    protected static getPropertyOptions(name: PropertyKey): PropertyDeclaration<unknown, unknown>;
    /**
     * Creates property accessors for registered properties and ensures
     * any superclasses are also finalized.
     * @nocollapse
     */
    protected static finalize(): void;
    /**
     * Returns the property name for the given attribute `name`.
     * @nocollapse
     */
    private static _attributeNameForProperty;
    /**
     * Returns true if a property should request an update.
     * Called when a property value is set and uses the `hasChanged`
     * option for the property if present or a strict identity check.
     * @nocollapse
     */
    private static _valueHasChanged;
    /**
     * Returns the property value for the given attribute value.
     * Called via the `attributeChangedCallback` and uses the property's
     * `converter` or `converter.fromAttribute` property option.
     * @nocollapse
     */
    private static _propertyValueFromAttribute;
    /**
     * Returns the attribute value for the given property value. If this
     * returns undefined, the property will *not* be reflected to an attribute.
     * If this returns null, the attribute will be removed, otherwise the
     * attribute will be set to the value.
     * This uses the property's `reflect` and `type.toAttribute` property options.
     * @nocollapse
     */
    private static _propertyValueToAttribute;
    private _updateState;
    private _instanceProperties?;
    private _updatePromise;
    private _enableUpdatingResolver;
    /**
     * Map with keys for any properties that have changed since the last
     * update cycle with previous values.
     */
    private _changedProperties;
    /**
     * Map with keys of properties that should be reflected when updated.
     */
    private _reflectingProperties?;
    constructor();
    /**
     * Performs element initialization. By default captures any pre-set values for
     * registered properties.
     */
    protected initialize(): void;
    /**
     * Fixes any properties set on the instance before upgrade time.
     * Otherwise these would shadow the accessor and break these properties.
     * The properties are stored in a Map which is played back after the
     * constructor runs. Note, on very old versions of Safari (<=9) or Chrome
     * (<=41), properties created for native platform properties like (`id` or
     * `name`) may not have default values set in the element constructor. On
     * these browsers native properties appear on instances and therefore their
     * default value will overwrite any element default (e.g. if the element sets
     * this.id = 'id' in the constructor, the 'id' will become '' since this is
     * the native platform default).
     */
    private _saveInstanceProperties;
    /**
     * Applies previously saved instance properties.
     */
    private _applyInstanceProperties;
    connectedCallback(): void;
    protected enableUpdating(): void;
    /**
     * Allows for `super.disconnectedCallback()` in extensions while
     * reserving the possibility of making non-breaking feature additions
     * when disconnecting at some point in the future.
     */
    disconnectedCallback(): void;
    /**
     * Synchronizes property values when attributes change.
     */
    attributeChangedCallback(name: string, old: string | null, value: string | null): void;
    private _propertyToAttribute;
    private _attributeToProperty;
    /**
     * This protected version of `requestUpdate` does not access or return the
     * `updateComplete` promise. This promise can be overridden and is therefore
     * not free to access.
     */
    protected requestUpdateInternal(name?: PropertyKey, oldValue?: unknown, options?: PropertyDeclaration): void;
    /**
     * Requests an update which is processed asynchronously. This should
     * be called when an element should update based on some state not triggered
     * by setting a property. In this case, pass no arguments. It should also be
     * called when manually implementing a property setter. In this case, pass the
     * property `name` and `oldValue` to ensure that any configured property
     * options are honored. Returns the `updateComplete` Promise which is resolved
     * when the update completes.
     *
     * @param name {PropertyKey} (optional) name of requesting property
     * @param oldValue {any} (optional) old value of requesting property
     * @returns {Promise} A Promise that is resolved when the update completes.
     */
    requestUpdate(name?: PropertyKey, oldValue?: unknown): Promise<unknown>;
    /**
     * Sets up the element to asynchronously update.
     */
    private _enqueueUpdate;
    private readonly _hasRequestedUpdate: any;
    protected readonly hasUpdated: number;
    /**
     * Performs an element update. Note, if an exception is thrown during the
     * update, `firstUpdated` and `updated` will not be called.
     *
     * You can override this method to change the timing of updates. If this
     * method is overridden, `super.performUpdate()` must be called.
     *
     * For instance, to schedule updates to occur just before the next frame:
     *
     * ```
     * protected async performUpdate(): Promise<unknown> {
     *   await new Promise((resolve) => requestAnimationFrame(() => resolve()));
     *   super.performUpdate();
     * }
     * ```
     */
    protected performUpdate(): void | Promise<unknown>;
    private _markUpdated;
    /*
    * Returns a Promise that resolves when the element has completed updating.
    * The Promise value is a boolean that is `true` if the element completed the
    * update without triggering another update. The Promise result is `false` if
    * a property was set inside `updated()`. If the Promise is rejected, an
    * exception was thrown during the update.
    *
    * To await additional asynchronous work, override the `_getUpdateComplete`
    * method. For example, it is sometimes useful to await a rendered element
    * before fulfilling this Promise. To do this, first await
    * `super._getUpdateComplete()`, then any subsequent state.
    *
    * @returns {Promise} The Promise returns a boolean that indicates if the
    * update resolved without triggering another update.
    */
    readonly updateComplete: Promise<unknown>;
    /**
     * Override point for the `updateComplete` promise.
     *
     * It is not safe to override the `updateComplete` getter directly due to a
     * limitation in TypeScript which means it is not possible to call a
     * superclass getter (e.g. `super.updateComplete.then(...)`) when the target
     * language is ES5 (https://github.com/microsoft/TypeScript/issues/338).
     * This method should be overridden instead. For example:
     *
     *   class MyElement extends LitElement {
     *     async _getUpdateComplete() {
     *       await super._getUpdateComplete();
     *       await this._myChild.updateComplete;
     *     }
     *   }
     */
    protected _getUpdateComplete(): Promise<unknown>;
    /**
     * Controls whether or not `update` should be called when the element requests
     * an update. By default, this method always returns `true`, but this can be
     * customized to control when to update.
     *
     * @param _changedProperties Map of changed properties with old values
     */
    protected shouldUpdate(_changedProperties: PropertyValues): boolean;
    /**
     * Updates the element. This method reflects property values to attributes.
     * It can be overridden to render and keep updated element DOM.
     * Setting properties inside this method will *not* trigger
     * another update.
     *
     * @param _changedProperties Map of changed properties with old values
     */
    protected update(_changedProperties: PropertyValues): void;
    /**
     * Invoked whenever the element is updated. Implement to perform
     * post-updating tasks via DOM APIs, for example, focusing an element.
     *
     * Setting properties inside this method will trigger the element to update
     * again after this update cycle completes.
     *
     * @param _changedProperties Map of changed properties with old values
     */
    protected updated(_changedProperties: PropertyValues): void;
    /**
     * Invoked when the element is first updated. Implement to perform one time
     * work on the element after update.
     *
     * Setting properties inside this method will trigger the element to update
     * again after this update cycle completes.
     *
     * @param _changedProperties Map of changed properties with old values
     */
    protected firstUpdated(_changedProperties: PropertyValues): void;
}
export {};
//# sourceMappingURL=updating-element.d.ts.map
