// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PermissionType} from './types.mojom-webui.js';

export {PermissionType, PermissionValue, TriState} from './types.mojom-webui.js';

export type PermissionTypeIndex = keyof typeof PermissionType;
