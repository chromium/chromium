/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { html } from 'lit';
import { SegmentedButton } from './segmented-button.js';
/** @soyCompatible */
export class OutlinedSegmentedButton extends SegmentedButton {
    /** @soyTemplate */
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-segmented-button--outlined': true,
        };
    }
    /** @soyTemplate */
    renderOutline() {
        return html `<span class="md3-segmented-button__outline"></span>`;
    }
}
//# sourceMappingURL=outlined-segmented-button.js.map