// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.jni_zero.samples;

import android.app.Activity;
import android.os.Bundle;

public class SampleActivity extends Activity {
    static {
        try {
            System.loadLibrary("sample_lib");
        } catch (UnsatisfiedLinkError e) {
            // This is a hacky workaround for the Chromium build system - in some configurations,
            // we use "component build", and instead of properly supporting this, we can just
            // try/catch for this sample app.
            System.loadLibrary("sample_lib.cr");
        }
    }

    public void onCreate(Bundle b) {
        super.onCreate(b);
        SampleForAnnotationProcessor.test();
    }
}
