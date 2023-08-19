// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Definitions related to the navigation.connection.type API which
 *  is used to identify if a Chromebook is currently using an LTE connection.
 */

type ConnectionType =|'bluetooth'|'cellular'|'ethernet'|'mixed'|'none'|'other'|
    'unknown'|'wifi'|'wimax';

interface Navigator {
  connection: {type: ConnectionType};
}
