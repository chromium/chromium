// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Page navigation utility code.
 */

export enum Page {
  LOCAL_CERTS = 'localcerts',
  CLIENT_CERTS = 'clientcerts',
  CRS_CERTS = 'crscerts',
  // Sub-pages
  ADMIN_CERTS = 'admincerts',
  PLATFORM_CERTS = 'platformcerts',
  PLATFORM_CLIENT_CERTS = 'platformclientcerts',
}
