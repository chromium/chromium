// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;
import android.os.SystemClock;
import android.view.InputDevice;
import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;

import java.io.IOException;

@NullMarked
final class Tap implements Action {
    private final UiAutomation mAutomation;
    private final Deadline mDeadline;
    private final float mX;
    private final float mY;

    Tap(UiAutomation automation, Deadline deadline, float x, float y) {
        mAutomation = automation;
        mDeadline = deadline;
        mX = x;
        mY = y;
    }

    @Override
    public void run() throws Exception {
        mDeadline.remainingMillis();
        long downTime = SystemClock.uptimeMillis();
        if (!inject(MotionEvent.ACTION_DOWN, downTime, mX, mY)) {
            throw new IOException("Touch-down injection failed");
        }
        boolean released;
        try {
            Thread.sleep(10);
        } finally {
            released = inject(MotionEvent.ACTION_UP, downTime, mX, mY);
        }
        if (!released) {
            throw new IOException("Touch-up injection failed");
        }
    }

    private boolean inject(int action, long downTime, float x, float y) {
        MotionEvent event =
                MotionEvent.obtain(downTime, SystemClock.uptimeMillis(), action, x, y, 0);
        event.setSource(InputDevice.SOURCE_TOUCHSCREEN);
        try {
            return mAutomation.injectInputEvent(event, true);
        } finally {
            event.recycle();
        }
    }
}
