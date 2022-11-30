/**
 * @license
 * Copyright 2018 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Observer function type.
 */
export interface Observer {
    (value: any, old: any): void;
}
/**
 * Specifies an observer callback that is run when the decorated property
 * changes. The observer receives the current and old value as arguments.
 */
export declare const observer: (observer: Observer) => (proto: any, propName: PropertyKey) => void;
