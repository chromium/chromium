// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.gfx;

import android.annotation.SuppressLint;

import androidx.annotation.Nullable;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

import java.lang.reflect.Method;

@JNINamespace("gfx")
class AdpfRenderingStageScheduler {
    private static final String TAG = "Adpf";
    private static final String HINT_SERVICE = "performance_hint";

    // TODO(crbug.com/1157620): Remove reflection once SDK is public.
    private static final boolean sEnabled;
    private static Method sHintManagerCreateHintSession;
    private static Method sHintSessionUpdateTargetWorkDuration;
    private static Method sHintSessionReportActualWorkDuration;
    private static Method sHintSessionClose;

    static {
        boolean enabled = false;
        if (BuildInfo.isAtLeastS()) {
            try {
                Class hintManagerClazz = Class.forName("android.os.PerformanceHintManager");
                sHintManagerCreateHintSession =
                        hintManagerClazz.getMethod("createHintSession", int[].class, long.class);
                Class hintSessionClazz = Class.forName("android.os.PerformanceHintManager$Session");
                sHintSessionUpdateTargetWorkDuration =
                        hintSessionClazz.getMethod("updateTargetWorkDuration", long.class);
                sHintSessionReportActualWorkDuration =
                        hintSessionClazz.getMethod("reportActualWorkDuration", long.class);
                sHintSessionClose = hintSessionClazz.getMethod("close");
                enabled = true;
            } catch (ReflectiveOperationException e) {
                Log.d(TAG, "PerformanceHintManager reflection exception", e);
            }
        }
        sEnabled = enabled;
    }

    private Object mHintSession;

    @SuppressLint("WrongConstant") // For using HINT_SERVICE
    @Nullable
    @CalledByNative
    private static AdpfRenderingStageScheduler create(int[] threadIds, long targetDurationNanos)
            throws ReflectiveOperationException {
        if (!sEnabled) return null;
        Object hintManager = ContextUtils.getApplicationContext().getSystemService(HINT_SERVICE);
        if (hintManager == null) {
            Log.d(TAG, "Null hint manager");
            return null;
        }
        Object hintSession =
                sHintManagerCreateHintSession.invoke(hintManager, threadIds, targetDurationNanos);
        if (hintSession == null) {
            Log.d(TAG, "Null hint session");
            return null;
        }
        return new AdpfRenderingStageScheduler(hintSession);
    }

    private AdpfRenderingStageScheduler(Object hintSession) throws ReflectiveOperationException {
        mHintSession = hintSession;
    }

    @CalledByNative
    private void reportCpuCompletionTime(long durationNanos) throws ReflectiveOperationException {
        sHintSessionReportActualWorkDuration.invoke(mHintSession, durationNanos);
    }

    @CalledByNative
    private void destroy() throws ReflectiveOperationException {
        sHintSessionClose.invoke(mHintSession);
        mHintSession = null;
    }
}
