/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * Event properties used by the adapter and foundation.
 */
export interface MDCMenuItemEventDetail {
    index: number;
}
/**
 * Event properties specific to the default component implementation.
 */
export interface MDCMenuItemComponentEventDetail extends MDCMenuItemEventDetail {
    item: Element;
}
export interface MDCMenuItemEvent extends Event {
    readonly detail: MDCMenuItemEventDetail;
}
export interface MDCMenuItemComponentEvent extends Event {
    readonly detail: MDCMenuItemComponentEventDetail;
}
