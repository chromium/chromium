// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

/**
 * This interface intentionally has no methods, and instances of this should
 * be created from class ObjectWrapper only.  This is used as a way of passing
 * objects that descend from the system classes via AIDL across classloaders
 * without serializing them.
 */
interface IObjectWrapper {
}
