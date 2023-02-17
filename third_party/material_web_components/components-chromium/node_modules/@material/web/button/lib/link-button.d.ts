/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { ClassInfo } from 'lit/directives/class-map.js';
import { Button } from './button.js';
export declare abstract class LinkButton extends Button {
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    href: string;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `target` attribute.
     */
    target: string;
    /**
     * Link buttons cannot be disabled.
     */
    disabled: boolean;
    protected getRenderClasses(): ClassInfo;
    protected render(): TemplateResult;
}
