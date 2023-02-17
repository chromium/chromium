/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { TemplateResult } from 'lit';
import { IconButton } from './icon-button.js';
export declare class LinkIconButton extends IconButton {
    /**
     * Sets the underlying `HTMLAnchorElement`'s `href` resource attribute.
     */
    linkHref: string;
    /**
     * Sets the underlying `HTMLAnchorElement`'s `target` attribute.
     */
    linkTarget: string;
    /**
     * Link buttons cannot be disabled.
     */
    disabled: boolean;
    willUpdate(): void;
    protected render(): TemplateResult;
}
