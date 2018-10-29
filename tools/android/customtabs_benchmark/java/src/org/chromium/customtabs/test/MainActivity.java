// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.customtabs.test;

import android.app.Activity;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Debug;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.SystemClock;
import android.support.customtabs.CustomTabsCallback;
import android.support.customtabs.CustomTabsClient;
import android.support.customtabs.CustomTabsIntent;
import android.support.customtabs.CustomTabsServiceConnection;
import android.support.customtabs.CustomTabsSession;
import android.support.v4.app.BundleCompat;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.CheckBox;
import android.widget.EditText;
import android.widget.RadioButton;

import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Activity used to benchmark Custom Tabs PLT.
 *
 * This activity contains benchmark code for two modes:
 * 1. Comparison between a basic use of Custom Tabs and a basic use of WebView.
 * 2. Custom Tabs benchmarking under various scenarios.
 *
 * The two modes are not merged into one as the metrics we can extract in the two cases
 * are constrained for the first one by what WebView provides.
 */
public class MainActivity extends Activity implements View.OnClickListener {
    static final String TAG = "CUSTOMTABSBENCH";
    private static final String MEMORY_TAG = "CUSTOMTABSMEMORY";
    private static final String DEFAULT_URL = "https://www.android.com";
    private static final String DEFAULT_PACKAGE = "com.google.android.apps.chrome";
    private static final int NONE = -1;
    // Common key between the benchmark modes.
    private static final String URL_KEY = "url";
    private static final String PARALLEL_URL_KEY = "parallel_url";
    private static final String DEFAULT_REFERRER_URL = "https://www.google.com";
    // Keys for the WebView / Custom Tabs comparison.
    static final String INTENT_SENT_EXTRA = "intent_sent_ms";
    private static final String USE_WEBVIEW_KEY = "use_webview";
    private static final String WARMUP_KEY = "warmup";

    // extraCommand related constants.
    private static final String SET_PRERENDER_ON_CELLULAR = "setPrerenderOnCellularForSession";
    private static final String SET_SPECULATION_MODE = "setSpeculationModeForSession";
    private static final String SET_IGNORE_URL_FRAGMENTS_FOR_SESSION =
            "setIgnoreUrlFragmentsForSession";

    private static final String ADD_VERIFIED_ORIGN = "addVerifiedOriginForSession";
    private static final String ENABLE_PARALLEL_REQUEST = "enableParallelRequestForSession";
    private static final String PARALLEL_REQUEST_REFERRER_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_REFERRER";
    private static final String PARALLEL_REQUEST_URL_KEY =
            "android.support.customtabs.PARALLEL_REQUEST_URL";
    private static final int PARALLEL_REQUEST_MIN_DELAY_AFTER_WARMUP = 3000;

    private static final int NO_SPECULATION = 0;
    private static final int PRERENDER = 2;
    private static final int HIDDEN_TAB = 3;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private EditText mUrlEditText;
    private RadioButton mChromeRadioButton;
    private RadioButton mWebViewRadioButton;
    private CheckBox mWarmupCheckbox;
    private CheckBox mParallelUrlCheckBox;
    private EditText mParallelUrlEditText;
    private long mIntentSentMs;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final Intent intent = getIntent();

        setUpUi();

        // Automated mode, 1s later to leave time for the app to settle.
        if (intent.getStringExtra(URL_KEY) != null) {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    processArguments(intent);
                }
            }, 1000);
        }
    }

    /** Displays the UI and registers the click listeners. */
    private void setUpUi() {
        setContentView(R.layout.main);

        mUrlEditText = (EditText) findViewById(R.id.url_text);
        mChromeRadioButton = (RadioButton) findViewById(R.id.radio_chrome);
        mWebViewRadioButton = (RadioButton) findViewById(R.id.radio_webview);
        mWarmupCheckbox = (CheckBox) findViewById(R.id.warmup_checkbox);
        mParallelUrlCheckBox = (CheckBox) findViewById(R.id.parallel_url_checkbox);
        mParallelUrlEditText = (EditText) findViewById(R.id.parallel_url_text);

        Button goButton = (Button) findViewById(R.id.go_button);

        mUrlEditText.setOnClickListener(this);
        mChromeRadioButton.setOnClickListener(this);
        mWebViewRadioButton.setOnClickListener(this);
        mWarmupCheckbox.setOnClickListener(this);
        mParallelUrlCheckBox.setOnClickListener(this);
        mParallelUrlEditText.setOnClickListener(this);
        goButton.setOnClickListener(this);
    }

    @Override
    public void onClick(View v) {
        int id = v.getId();

        boolean warmup = mWarmupCheckbox.isChecked();
        boolean useChrome = mChromeRadioButton.isChecked();
        boolean useWebView = mWebViewRadioButton.isChecked();
        String url = mUrlEditText.getText().toString();
        boolean willRequestParallelUrl = mParallelUrlCheckBox.isChecked();
        String parallelUrl = null;
        if (willRequestParallelUrl) {
            parallelUrl = mParallelUrlEditText.getText().toString();
        }

        if (id == R.id.go_button) {
            customTabsWebViewBenchmark(url, useChrome, useWebView, warmup, parallelUrl);
        }
    }

    /** Routes to either of the benchmark modes. */
    private void processArguments(Intent intent) {
        if (intent.hasExtra(USE_WEBVIEW_KEY)) {
            startCustomTabsWebViewBenchmark(intent);
        } else {
            startCustomTabsBenchmark(intent);
        }
    }

    /** Start the CustomTabs / WebView comparison benchmark.
     *
     * NOTE: Methods below are for the first benchmark mode.
     */
    private void startCustomTabsWebViewBenchmark(Intent intent) {
        Bundle extras = intent.getExtras();
        String url = extras.getString(URL_KEY);
        String parallelUrl = extras.getString(PARALLEL_URL_KEY);
        boolean useWebView = extras.getBoolean(USE_WEBVIEW_KEY);
        boolean useChrome = !useWebView;
        boolean warmup = extras.getBoolean(WARMUP_KEY);
        customTabsWebViewBenchmark(url, useChrome, useWebView, warmup, parallelUrl);
    }

    /** Start the CustomTabs / WebView comparison benchmark. */
    private void customTabsWebViewBenchmark(
            String url, boolean useChrome, boolean useWebView, boolean warmup, String parallelUrl) {
        if (useChrome) {
            launchChrome(url, warmup, parallelUrl);
        } else {
            assert useWebView;
            launchWebView(url);
        }
    }

    private void launchWebView(String url) {
        Intent intent = new Intent();
        intent.setData(Uri.parse(url));
        intent.setClass(this, WebViewActivity.class);
        intent.putExtra(INTENT_SENT_EXTRA, now());
        startActivity(intent);
    }

    private void launchChrome(final String url, final boolean warmup, String parallelUrl) {
        CustomTabsServiceConnection connection = new CustomTabsServiceConnection() {
            @Override
            public void onCustomTabsServiceConnected(ComponentName name, CustomTabsClient client) {
                launchChromeIntent(url, warmup, client, parallelUrl);
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {}
        };
        CustomTabsClient.bindCustomTabsService(this, DEFAULT_PACKAGE, connection);
    }

    private static void maybePrepareParallelUrlRequest(String parallelUrl, CustomTabsClient client,
            CustomTabsIntent intent, IBinder sessionBinder) {
        if (parallelUrl == null || parallelUrl.length() == 0) {
            Log.w(TAG, "null or empty parallelUrl");
            return;
        }

        Uri parallelUri = Uri.parse(parallelUrl);
        Bundle params = new Bundle();
        BundleCompat.putBinder(params, "session", sessionBinder);

        Uri referrerUri = Uri.parse(DEFAULT_REFERRER_URL);
        params.putParcelable("origin", referrerUri);

        Bundle result = client.extraCommand(ADD_VERIFIED_ORIGN, params);
        boolean ok = (result != null) && result.getBoolean(ADD_VERIFIED_ORIGN);
        if (!ok) throw new RuntimeException("Cannot add verified origin");

        result = client.extraCommand(ENABLE_PARALLEL_REQUEST, params);
        ok = (result != null) && result.getBoolean(ENABLE_PARALLEL_REQUEST);
        if (!ok) throw new RuntimeException("Cannot enable Parallel Request");
        Log.w(TAG, "enabled Parallel Request");

        intent.intent.putExtra(PARALLEL_REQUEST_URL_KEY, parallelUri);
        intent.intent.putExtra(PARALLEL_REQUEST_REFERRER_KEY, referrerUri);
    }

    private void launchChromeIntent(
            String url, boolean warmup, CustomTabsClient client, String parallelUrl) {
        CustomTabsCallback callback = new CustomTabsCallback() {
            private long mNavigationStartOffsetMs;

            @Override
            public void onNavigationEvent(int navigationEvent, Bundle extras) {
                long offsetMs = now() - mIntentSentMs;
                switch (navigationEvent) {
                    case CustomTabsCallback.NAVIGATION_STARTED:
                        mNavigationStartOffsetMs = offsetMs;
                        Log.w(TAG, "navigationStarted = " + offsetMs);
                        break;
                    case CustomTabsCallback.NAVIGATION_FINISHED:
                        Log.w(TAG, "navigationFinished = " + offsetMs);
                        Log.w(TAG, "CHROME," + mNavigationStartOffsetMs + "," + offsetMs);
                        break;
                    default:
                        break;
                }
            }
        };
        CustomTabsSession session = client.newSession(callback);
        final CustomTabsIntent customTabsIntent = new CustomTabsIntent.Builder(session).build();
        final Uri uri = Uri.parse(url);

        IBinder sessionBinder = BundleCompat.getBinder(
                customTabsIntent.intent.getExtras(), CustomTabsIntent.EXTRA_SESSION);
        assert sessionBinder != null;
        maybePrepareParallelUrlRequest(parallelUrl, client, customTabsIntent, sessionBinder);

        if (warmup) {
            client.warmup(0);
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    mIntentSentMs = now();
                    customTabsIntent.launchUrl(MainActivity.this, uri);
                }
            }, 3000);
        } else {
            mIntentSentMs = now();
            customTabsIntent.launchUrl(MainActivity.this, uri);
        }
    }

    static long now() {
        return System.currentTimeMillis();
    }

    /** Start the second benchmark mode.
     *
     * NOTE: Methods below are for the second mode.
     */
    private void startCustomTabsBenchmark(Intent intent) {
        String url = intent.getStringExtra(URL_KEY);
        if (url == null) url = DEFAULT_URL;
        String parallelUrl = intent.getStringExtra(PARALLEL_URL_KEY);

        String speculatedUrl = intent.getStringExtra("speculated_url");
        if (speculatedUrl == null) speculatedUrl = url;
        String packageName = intent.getStringExtra("package_name");
        if (packageName == null) packageName = DEFAULT_PACKAGE;
        boolean warmup = intent.getBooleanExtra("warmup", false);

        boolean skipLauncherActivity = intent.getBooleanExtra("skip_launcher_activity", false);
        int delayToMayLaunchUrl = intent.getIntExtra("delay_to_may_launch_url", NONE);
        int delayToLaunchUrl = intent.getIntExtra("delay_to_launch_url", NONE);
        String speculationMode = intent.getStringExtra("speculation_mode");
        if (speculationMode == null) speculationMode = "prerender";
        int timeoutSeconds = intent.getIntExtra("timeout", NONE);

        if (parallelUrl != null && !warmup) {
            Log.w(TAG, "Parallel URL provided, forcing warmup");
            warmup = true;
            delayToLaunchUrl = Math.max(delayToLaunchUrl, PARALLEL_REQUEST_MIN_DELAY_AFTER_WARMUP);
            delayToMayLaunchUrl =
                    Math.max(delayToMayLaunchUrl, PARALLEL_REQUEST_MIN_DELAY_AFTER_WARMUP);
        }

        launchCustomTabs(packageName, speculatedUrl, url, warmup, skipLauncherActivity,
                speculationMode, delayToMayLaunchUrl, delayToLaunchUrl, timeoutSeconds,
                parallelUrl);
    }

    private final class CustomCallback extends CustomTabsCallback {
        private final String mPackageName;
        private final boolean mWarmup;
        private final boolean mSkipLauncherActivity;
        private final String mSpeculationMode;
        private final int mDelayToMayLaunchUrl;
        private final int mDelayToLaunchUrl;
        public boolean mWarmupCompleted = false;
        private long mIntentSentMs = NONE;
        private long mPageLoadStartedMs = NONE;
        private long mPageLoadFinishedMs = NONE;
        private long mFirstContentfulPaintMs = NONE;

        public CustomCallback(String packageName, boolean warmup, boolean skipLauncherActivity,
                String speculationMode, int delayToMayLaunchUrl, int delayToLaunchUrl) {
            mPackageName = packageName;
            mWarmup = warmup;
            mSkipLauncherActivity = skipLauncherActivity;
            mSpeculationMode = speculationMode;
            mDelayToMayLaunchUrl = delayToMayLaunchUrl;
            mDelayToLaunchUrl = delayToLaunchUrl;
        }

        public void recordIntentHasBeenSent() {
            mIntentSentMs = SystemClock.uptimeMillis();
        }

        @Override
        public void onNavigationEvent(int navigationEvent, Bundle extras) {
            switch (navigationEvent) {
                case CustomTabsCallback.NAVIGATION_STARTED:
                    mPageLoadStartedMs = SystemClock.uptimeMillis();
                    break;
                case CustomTabsCallback.NAVIGATION_FINISHED:
                    mPageLoadFinishedMs = SystemClock.uptimeMillis();
                    break;
                default:
                    break;
            }
            if (allSet()) logMetricsAndFinish();
        }

        @Override
        public void extraCallback(String callbackName, Bundle args) {
            if ("onWarmupCompleted".equals(callbackName)) {
                mWarmupCompleted = true;
                return;
            }

            if (!"NavigationMetrics".equals(callbackName)) {
                Log.w(TAG, "Unknown extra callback skipped: " + callbackName);
                return;
            }
            long firstPaintMs = args.getLong("firstContentfulPaint", NONE);
            long navigationStartMs = args.getLong("navigationStart", NONE);
            if (firstPaintMs == NONE || navigationStartMs == NONE) return;
            // Can be reported several times, only record the first one.
            if (mFirstContentfulPaintMs == NONE) {
                mFirstContentfulPaintMs = navigationStartMs + firstPaintMs;
            }
            if (allSet()) logMetricsAndFinish();
        }

        private boolean allSet() {
            return mIntentSentMs != NONE && mPageLoadStartedMs != NONE
                    && mFirstContentfulPaintMs != NONE && mPageLoadFinishedMs != NONE;
        }

        /** Outputs the available metrics, and die. Unavalaible metrics are set to -1. */
        private void logMetricsAndFinish() {
            String logLine = (mWarmup ? "1" : "0") + "," + (mSkipLauncherActivity ? "1" : "0") + ","
                    + mSpeculationMode + "," + mDelayToMayLaunchUrl + "," + mDelayToLaunchUrl + ","
                    + mIntentSentMs + "," + mPageLoadStartedMs + "," + mPageLoadFinishedMs + ","
                    + mFirstContentfulPaintMs;
            Log.w(TAG, logLine);
            logMemory(mPackageName, "AfterMetrics");
            MainActivity.this.finish();
        }

        /** Same as {@link logMetricsAndFinish()} with a set delay in ms. */
        public void logMetricsAndFinishDelayed(int delayMs) {
            mHandler.postDelayed(new Runnable() {
                @Override
                public void run() {
                    logMetricsAndFinish();
                }
            }, delayMs);
        }
    }

    /**
     * Sums all the memory usage of a package, and returns (PSS, Private Dirty).
     *
     * Only works for packages where a service is exported by each process, which is the case for
     * Chrome. Also, doesn't work on O and above, as {@link ActivityManager.getRunningServices} is
     * restricted.
     *
     * @param context Application context
     * @param packageName the package to query
     * @return {pss, privateDirty} in kB, or null.
     */
    private static int[] getPackagePssAndPrivateDirty(Context context, String packageName) {
        if (Build.VERSION.SDK_INT > Build.VERSION_CODES.N_MR1) return null;

        ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
        List<ActivityManager.RunningServiceInfo> services = am.getRunningServices(1000);
        if (services == null) return null;

        Set<Integer> pids = new HashSet<>();
        for (ActivityManager.RunningServiceInfo info : services) {
            if (packageName.equals(info.service.getPackageName())) pids.add(info.pid);
        }

        int[] pidsArray = new int[pids.size()];
        int i = 0;
        for (int pid : pids) pidsArray[i++] = pid;
        Debug.MemoryInfo infos[] = am.getProcessMemoryInfo(pidsArray);
        if (infos == null || infos.length == 0) return null;

        int pss = 0;
        int privateDirty = 0;
        for (Debug.MemoryInfo info : infos) {
            pss += info.getTotalPss();
            privateDirty += info.getTotalPrivateDirty();
        }

        return new int[] {pss, privateDirty};
    }

    private void logMemory(String packageName, String message) {
        int[] pssAndPrivateDirty = getPackagePssAndPrivateDirty(
                getApplicationContext(), packageName);
        if (pssAndPrivateDirty == null) return;
        Log.w(MEMORY_TAG, message + "," + pssAndPrivateDirty[0] + "," + pssAndPrivateDirty[1]);
    }

    private static void forceSpeculationMode(
            CustomTabsClient client, IBinder sessionBinder, String speculationMode) {
        // The same bundle can be used for all calls, as the commands only look for their own
        // arguments in it.
        Bundle params = new Bundle();
        BundleCompat.putBinder(params, "session", sessionBinder);
        params.putBoolean("ignoreFragments", true);
        params.putBoolean("prerender", true);

        int speculationModeValue = 0;
        switch (speculationMode) {
            case "disabled":
                speculationModeValue = NO_SPECULATION;
                break;
            case "prerender":
                speculationModeValue = PRERENDER;
                break;
            case "hidden_tab":
                speculationModeValue = HIDDEN_TAB;
                break;
            default:
                throw new RuntimeException("Invalid speculation mode");
        }
        params.putInt("speculationMode", speculationModeValue);

        boolean ok = client.extraCommand(SET_PRERENDER_ON_CELLULAR, params) != null;
        if (!ok) throw new RuntimeException("Cannot set cellular prerendering");
        ok = client.extraCommand(SET_IGNORE_URL_FRAGMENTS_FOR_SESSION, params) != null;
        if (!ok) throw new RuntimeException("Cannot set ignoreFragments");
        ok = client.extraCommand(SET_SPECULATION_MODE, params) != null;
        if (!ok) throw new RuntimeException("Cannot set the speculation mode");
    }

    private void onCustomTabsServiceConnected(CustomTabsClient client, final Uri speculatedUri,
            final Uri uri, final CustomCallback cb, boolean warmup, boolean skipLauncherActivity,
            String speculationMode, int delayToMayLaunchUrl, final int delayToLaunchUrl,
            final int timeoutSeconds, final String packageName, final String parallelUrl) {
        final CustomTabsSession session = client.newSession(cb);
        final CustomTabsIntent intent = (new CustomTabsIntent.Builder(session)).build();

        logMemory(packageName, "OnServiceConnected");

        IBinder sessionBinder =
                BundleCompat.getBinder(intent.intent.getExtras(), CustomTabsIntent.EXTRA_SESSION);
        assert sessionBinder != null;
        forceSpeculationMode(client, sessionBinder, speculationMode);

        final Runnable launchRunnable = new Runnable() {
            @Override
            public void run() {
                logMemory(packageName, "BeforeLaunch");

                if (cb.mWarmupCompleted) {
                    maybePrepareParallelUrlRequest(parallelUrl, client, intent, sessionBinder);
                } else {
                    Log.e(TAG, "not warmed up yet!");
                }

                intent.launchUrl(MainActivity.this, uri);
                cb.recordIntentHasBeenSent();
                if (timeoutSeconds != NONE) cb.logMetricsAndFinishDelayed(timeoutSeconds * 1000);
            }
        };
        Runnable mayLaunchRunnable = new Runnable() {
            @Override
            public void run() {
                logMemory(packageName, "BeforeMayLaunchUrl");
                session.mayLaunchUrl(speculatedUri, null, null);
                mHandler.postDelayed(launchRunnable, delayToLaunchUrl);
            }
        };

        if (warmup) client.warmup(0);
        if (delayToMayLaunchUrl != NONE) {
            mHandler.postDelayed(mayLaunchRunnable, delayToMayLaunchUrl);
        } else {
            mHandler.postDelayed(launchRunnable, delayToLaunchUrl);
        }
    }

    private void launchCustomTabs(final String packageName, String speculatedUrl, String url,
            final boolean warmup, final boolean skipLauncherActivity, final String speculationMode,
            final int delayToMayLaunchUrl, final int delayToLaunchUrl, final int timeoutSeconds,
            String parallelUrl) {
        final CustomCallback cb = new CustomCallback(packageName, warmup, skipLauncherActivity,
                speculationMode, delayToMayLaunchUrl, delayToLaunchUrl);
        final Uri speculatedUri = Uri.parse(speculatedUrl);
        final Uri uri = Uri.parse(url);
        CustomTabsClient.bindCustomTabsService(
                this, packageName, new CustomTabsServiceConnection() {
                    @Override
                    public void onCustomTabsServiceConnected(
                            ComponentName name, final CustomTabsClient client) {
                        MainActivity.this.onCustomTabsServiceConnected(client, speculatedUri, uri,
                                cb, warmup, skipLauncherActivity, speculationMode,
                                delayToMayLaunchUrl, delayToLaunchUrl, timeoutSeconds, packageName,
                                parallelUrl);
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName name) {}
                });
    }
}
