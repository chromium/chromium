/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { SegmentedButton } from './segmented-button.js';
/**
 * b/265346443 - add docs
 */
export declare class OutlinedSegmentedButton extends SegmentedButton {
    protected getRenderClasses(): {
        'md3-segmented-button--outlined': boolean;
        'md3-segmented-button--selected': boolean;
        'md3-segmented-button--unselected': boolean;
        'md3-segmented-button--with-label': boolean;
        'md3-segmented-button--without-label': boolean;
        'md3-segmented-button--with-icon': boolean;
        'md3-segmented-button--with-checkmark': boolean;
        'md3-segmented-button--without-checkmark': boolean;
        'md3-segmented-button--selecting': boolean;
        'md3-segmented-button--deselecting': boolean;
    };
    protected renderOutline(): import("lit-html").TemplateResult<1>;
}
