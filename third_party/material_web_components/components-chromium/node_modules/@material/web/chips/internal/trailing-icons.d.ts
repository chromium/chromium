/**
 * @license
 * Copyright 2023 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../focus/md-focus-ring.js';
import '../../ripple/ripple.js';
interface RemoveButtonProperties {
    ariaLabel: string | null;
    disabled: boolean;
    focusListener: EventListener;
    tabbable?: boolean;
}
/** @protected */
export declare function renderRemoveButton({ ariaLabel, disabled, focusListener, tabbable, }: RemoveButtonProperties): import("lit-html").TemplateResult<1>;
export {};
