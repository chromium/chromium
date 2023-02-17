/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { MenuSurface } from './lib/menu-surface.js';
declare global {
    interface HTMLElementTagNameMap {
        'md-menu-surface': MdMenuSurface;
    }
}
export declare class MdMenuSurface extends MenuSurface {
    static styles: import("lit").CSSResult[];
}
