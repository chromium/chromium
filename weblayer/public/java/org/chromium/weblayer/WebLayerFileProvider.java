// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer;

import androidx.core.content.FileProvider;

/**
 * Subclass of FileProvider which prevents conflicts with the embedding application manifest.
 */
public class WebLayerFileProvider extends FileProvider {}
