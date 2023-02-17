/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { LitElement } from 'lit';
/**
 * A component for elevation.
 */
export declare class Elevation extends LitElement {
    /**
     * Whether or not the elevation level should display a shadow.
     */
    shadow: boolean;
    /**
     * Whether or not the elevation level should display a surface tint color.
     */
    surface: boolean;
    render(): import("lit-html").TemplateResult<1>;
}
