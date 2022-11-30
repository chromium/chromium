/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Determines whether a node is an element.
 *
 * @param node Node to check
 */
export declare const isNodeElement: (node: Node) => node is Element;
export declare type Constructor<T> = new (...args: any[]) => T;
export declare function addHasRemoveClass(element: HTMLElement): {
    addClass: (className: string) => void;
    removeClass: (className: string) => void;
    hasClass: (className: string) => boolean;
};
/**
 * Do event listeners suport the `passive` option?
 */
export declare const supportsPassiveEventListener = false;
export declare const deepActiveElementPath: (doc?: Document) => Element[];
export declare const doesElementContainFocus: (element: HTMLElement) => boolean;
export interface RippleInterface {
    startPress: (e?: Event) => void;
    endPress: () => void;
    startFocus: () => void;
    endFocus: () => void;
    startHover: () => void;
    endHover: () => void;
}
