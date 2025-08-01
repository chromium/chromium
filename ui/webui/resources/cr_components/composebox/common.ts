// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {FileUploadStatus} from './composebox_query.mojom-webui.js';

export interface ComposeboxFile {
  uuid: UnguessableToken;
  name: string;
  objectUrl: string|null;
  type: string;
  status: FileUploadStatus;
}
