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
export declare class MDCFoundation<AdapterType extends {} = {}> {
    protected adapter: AdapterType;
    static get cssClasses(): {
        [key: string]: string;
    };
    static get strings(): {
        [key: string]: string;
    };
    static get numbers(): {
        [key: string]: number;
    };
    static get defaultAdapter(): {};
    constructor(adapter?: AdapterType);
    init(): void;
    destroy(): void;
}
/**
 * The constructor for MDCFoundation.
 */
export interface MDCFoundationConstructor<AdapterType extends object = any> {
    new (adapter: AdapterType): MDCFoundation<AdapterType>;
    readonly prototype: MDCFoundation<AdapterType>;
}
/**
 * The deprecated constructor for MDCFoundation.
 */
export interface MDCFoundationDeprecatedConstructor<AdapterType extends object = any> {
    readonly cssClasses: Record<string, string>;
    readonly strings: Record<string, string>;
    readonly numbers: Record<string, number>;
    readonly defaultAdapter: AdapterType;
    new (adapter?: Partial<AdapterType>): MDCFoundation<AdapterType>;
    readonly prototype: MDCFoundation<AdapterType>;
}
/**
 * Retrieves the AdapaterType from the provided MDCFoundation generic type.
 */
export declare type MDCFoundationAdapter<T> = T extends MDCFoundation<infer A> ? A : never;
export default MDCFoundation;
