// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.jni_zero.sample;

import android.app.Activity;
import android.os.Bundle;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

public class SampleActivity extends Activity {
    static {
        System.loadLibrary("jni_zero_sample");
    }

    public void onCreate(Bundle b) {
        super.onCreate(b);
        Sample.doSingleBasicCall();
        createButtons();
    }

    private void createButtons() {
        LinearLayout.LayoutParams basicLayoutParams =
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        LinearLayout layout = new LinearLayout(this);
        layout.setLayoutParams(basicLayoutParams);

        Button twoWayButton = new Button(this);
        twoWayButton.setText("Two way JNI call");
        twoWayButton.setOnClickListener(v -> Sample.doTwoWayCalls());
        layout.addView(twoWayButton);

        Button paramsButton = new Button(this);
        paramsButton.setText("Multi parameter JNI call");
        paramsButton.setOnClickListener(v -> Sample.doParameterCalls());
        layout.addView(paramsButton);

        addContentView(layout, basicLayoutParams);
    }
}
