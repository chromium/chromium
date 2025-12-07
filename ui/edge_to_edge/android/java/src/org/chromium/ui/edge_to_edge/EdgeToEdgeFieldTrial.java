// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.edge_to_edge;

import androidx.annotation.UiThread;

import org.chromium.build.annotations.NullMarked;

/**
 * Field trial override that gives different min versions according to their manufacturer. For
 * manufacturer not listed, it'll use the default (Android R, API 30). This class is UI thread only.
 *
 * <p>Example usage:
 *
 * <pre>{@code
 * param {
 *   e2e_field_trial_oem_list = "foo,bar"
 *   e2e_field_trial_oem_min_versions = "31,32"
 * }
 * }</pre>
 *
 * <p>For manufacturer "foo", edge to edge should be enabled on API 31+; for manufacturer "bar",
 * edge to edge should be enabled on API 32+; for unspecified manufacturer(s), edge to edge will be
 * enabled on API 30+.
 */
@UiThread
@NullMarked
public interface EdgeToEdgeFieldTrial {
    /** Whether the feature should be enabled according to the field trial min version override. */
    boolean isEnabledForManufacturerVersion();
}
