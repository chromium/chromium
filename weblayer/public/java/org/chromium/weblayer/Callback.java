// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

/**
 * Simple callback that takes a generic value.
 * @param <T> The type of value passed into the callback.
 */
public interface Callback<T> { void onResult(T result); }
