/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
export const stringConverter = {
    fromAttribute(value) {
        return value ?? '';
    },
    toAttribute(value) {
        return value || null;
    },
};
//# sourceMappingURL=string-converter.js.map