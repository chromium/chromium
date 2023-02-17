/**
 * @license
 * Copyright 2021 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
import { ClassInfo } from 'lit/directives/class-map.js';
import { TextField } from './text-field.js';
/** @soyCompatible */
export declare abstract class OutlinedTextField extends TextField {
    /** @soyTemplate */
    protected getRenderClasses(): ClassInfo;
}
