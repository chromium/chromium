/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { SegmentedButtonSet } from './segmented-button-set.js';
/**
 * b/265346443 - add docs
 */
export class OutlinedSegmentedButtonSet extends SegmentedButtonSet {
    getRenderClasses() {
        return {
            ...super.getRenderClasses(),
            'md3-segmented-button-set--outlined': true,
        };
    }
}
//# sourceMappingURL=outlined-segmented-button-set.js.map