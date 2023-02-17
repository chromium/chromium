/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import '../../elevation/elevation.js';
import { LitElement, PropertyValues, TemplateResult } from 'lit';
/** @soyCompatible */
export declare class NavigationDrawer extends LitElement {
    ariaDescribedBy: string | undefined;
    ariaLabel: string;
    ariaModal: 'true' | 'false';
    ariaLabelledBy: string | undefined;
    opened: boolean;
    pivot: 'start' | 'end';
    /** @soyTemplate */
    render(): TemplateResult;
    /** @soyTemplate classMap */
    protected getRenderClasses(): import("lit-html/directive.js").DirectiveResult<typeof import("lit-html/directives/class-map.js").ClassMapDirective>;
    protected updated(changedProperties: PropertyValues<NavigationDrawer>): void;
}
