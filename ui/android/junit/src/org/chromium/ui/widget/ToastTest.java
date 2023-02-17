// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import static org.junit.Assert.assertEquals;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.R;
import org.chromium.ui.widget.Toast.ToastPriority;

/** Tests for {@link ToastManager}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(Mode.PAUSED)
public class ToastTest {
    private Context mContext;

    @Test
    public void makeTextWithPriority() {
        mContext = ApplicationProvider.getApplicationContext();
        Toast toast = Toast.makeText(mContext, "Message", 0);
        assertEquals("Toast priority should be NORMAL", ToastPriority.NORMAL, toast.getPriority());

        toast = Toast.makeTextWithPriority(mContext, R.string.toast_text, 0, ToastPriority.HIGH);
        assertEquals("Toast priority should be HIGH", ToastPriority.HIGH, toast.getPriority());
    }
}
