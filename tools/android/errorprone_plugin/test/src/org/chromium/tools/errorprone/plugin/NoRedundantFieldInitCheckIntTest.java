// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

/** |mBar| initialization should cause 'NoRedundantFieldInitCheck' errorprone warning. */
public class NoRedundantFieldInitCheckIntTest {
    private int mBar = 0;

    public void foo() {
        System.out.println("" + mBar);
    }
}
