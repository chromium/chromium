/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { SegmentedButtonSet } from './segmented-button-set.js';
/**
 * b/265346443 - add docs
 */
export declare class OutlinedSegmentedButtonSet extends SegmentedButtonSet {
    protected getRenderClasses(): {
        'md3-segmented-button-set--outlined': boolean;
    };
}
