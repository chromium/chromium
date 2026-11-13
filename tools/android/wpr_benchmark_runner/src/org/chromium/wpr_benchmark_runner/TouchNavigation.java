// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.wpr_benchmark_runner;

import android.app.UiAutomation;

import org.json.JSONObject;

import org.chromium.build.annotations.NullMarked;

@NullMarked
final class TouchNavigation implements Action {
    private static final long TAP_DURATION_MS = 50;
    private static final String SETUP_SCRIPT =
            """
            document.documentElement.style.height = '100vh';
            window.__ll_nav_triggered = false;
            const triggerNav = () => {
              if (window.__ll_nav_triggered) return;
              window.__ll_nav_triggered = true;
              %s
            };
            for (const event of ['pointerup', 'touchend', 'click']) {
              window.addEventListener(event, triggerNav, {once: true});
              document.addEventListener(event, triggerNav, {once: true});
            }
            """;

    private static final String POSITION_SCRIPT =
            """
            let bottomInset = 0;
            if (screenY === 0 && outerHeight === screen.height) {
              const probe = document.createElement('div');
              probe.style.cssText =
                  'position:fixed;top:0;left:0;width:0;visibility:hidden;' +
                  'pointer-events:none;contain:layout style paint;' +
                  'height:calc(env(safe-area-max-inset-bottom, 0px) - ' +
                  'env(safe-area-inset-bottom, 0px))';
              document.documentElement.appendChild(probe);
              bottomInset = probe.getBoundingClientRect().height;
              probe.remove();
            }
            const zoom = outerWidth / visualViewport.width;
            const density = devicePixelRatio / Math.max(1, zoom);
            const scale = devicePixelRatio * Math.min(1, zoom);
            const left = Math.round(screenX * density);
            const top = Math.round(screenY * density) + Math.max(0,
                Math.round((outerHeight - bottomInset) * density) -
                Math.round(visualViewport.height * scale));
            const rect = document.documentElement.getBoundingClientRect();
            const x = left + rect.left * scale;
            const y = top + rect.top * scale;
            return {
              x: x + Math.min(rect.width * scale, screen.width * density - x) / 2,
              y: y + Math.min(rect.height * scale, screen.height * density - y) / 2,
            };
            """;

    private final UiAutomation mAutomation;
    private final CdpSocket mCdpSocket;
    private final Deadline mDeadline;
    private final String mScript;
    private final long mTimeoutMillis;

    TouchNavigation(
            UiAutomation automation,
            CdpSocket cdpSocket,
            Deadline deadline,
            String script,
            long timeoutMillis) {
        mAutomation = automation;
        mCdpSocket = cdpSocket;
        mDeadline = deadline;
        mScript = script;
        mTimeoutMillis = timeoutMillis;
    }

    @Override
    public void run() throws Exception {
        Deadline deadline = mDeadline.limit(mTimeoutMillis);
        JSONObject coordinates =
                (JSONObject)
                        mCdpSocket.evaluate(
                                SETUP_SCRIPT.formatted(mScript) + POSITION_SCRIPT,
                                true,
                                false,
                                deadline);
        new Tap(
                        mAutomation,
                        deadline,
                        (float) coordinates.getDouble("x"),
                        (float) coordinates.getDouble("y"),
                        TAP_DURATION_MS)
                .run();
    }
}
