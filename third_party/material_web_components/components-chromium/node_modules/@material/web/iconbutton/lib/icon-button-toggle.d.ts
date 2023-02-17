/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/focus-ring.js';
import '../../icon/icon.js';
import '../../ripple/ripple.js';
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { IconButton } from './icon-button.js';
/**
 * @fires change {Event}
 * Dispatched whenever `selected` is changed via user click
 *
 * @fires input {InputEvent}
 * Dispatched whenever `selected` is changed via user click
 */
export declare class IconButtonToggle extends IconButton {
    /**
     * The `aria-label` of the button when the toggle button is selected.
     */
    ariaLabelSelected: string;
    /**
     * Sets the selected state. When false, displays the default icon. When true,
     * displays the `selectedIcon`, or the default icon If no `selectedIcon` is
     * provided.
     */
    selected: boolean;
    protected render(): TemplateResult;
    protected renderSelectedIcon(): TemplateResult<1>;
    protected getRenderClasses(): ClassInfo;
    protected handleClick(): void;
}
