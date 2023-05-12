// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.text;

import android.text.Editable;
import android.text.TextWatcher;

/** Simple no-op default interface that allows subclasses to only implement methods as needed. */
public interface EmptyTextWatcher extends TextWatcher {
    @Override
    default void beforeTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

    @Override
    default void onTextChanged(CharSequence charSequence, int i, int i1, int i2) {}

    @Override
    default void afterTextChanged(Editable editable) {}
}
