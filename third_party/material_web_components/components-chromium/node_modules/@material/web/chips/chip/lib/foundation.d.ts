/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MDCChipActionFocusBehavior, MDCChipActionType } from '../../action/lib/constants.js';
import { MDCChipAdapter } from './adapter.js';
import { MDCChipAnimation } from './constants.js';
import { ActionInteractionEvent, ActionNavigationEvent } from './types.js';
/**
 * MDCChipFoundation provides a foundation for all chips.
 */
export declare class MDCChipFoundation {
    private readonly adapter;
    static get defaultAdapter(): MDCChipAdapter;
    private readonly animFrame;
    constructor(adapter?: Partial<MDCChipAdapter>);
    destroy(): void;
    getElementID(): string;
    setDisabled(isDisabled: boolean): void;
    isDisabled(): boolean;
    getActions(): MDCChipActionType[];
    isActionFocusable(action: MDCChipActionType): boolean;
    isActionSelectable(action: MDCChipActionType): boolean;
    isActionSelected(action: MDCChipActionType): boolean;
    setActionFocus(action: MDCChipActionType, focus: MDCChipActionFocusBehavior): void;
    setActionSelected(action: MDCChipActionType, isSelected: boolean): void;
    startAnimation(animation: MDCChipAnimation): void;
    handleAnimationEnd(event: AnimationEvent): void;
    handleTransitionEnd(): void;
    handleActionInteraction({ detail }: ActionInteractionEvent): void;
    handleActionNavigation({ detail }: ActionNavigationEvent): void;
    private directionFromKey;
    private navigateActions;
    private shouldRemove;
    private getRemovedAnnouncement;
    private getAddedAnnouncement;
    private animateSelection;
    private resetAnimationStyles;
    private updateSelectionStyles;
}
