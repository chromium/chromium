// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.base;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.PendingIntent;
import android.app.UiModeManager;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Build;
import android.os.IBinder;
import android.os.Process;
import android.util.TypedValue;
import android.view.Display;
import android.view.Surface;
import android.view.View;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.CalledByNativeForTesting;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.LifetimeAssert;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.UnownedUserDataHost;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;
import org.chromium.ui.gfx.OverlayTransform;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;

/** The window base class that has the minimum functionality. */
@JNINamespace("ui")
public class WindowAndroid implements AndroidPermissionDelegate, DisplayAndroidObserver {
    private static final String TAG = "WindowAndroid";
    private static final ImmutableWeakReference<Activity> NULL_ACTIVITY_WEAK_REF =
            new ImmutableWeakReference<>(null);

    // Arbitrary error margin to account for cases where the display's refresh rate might not
    // exactly match the target rate.
    private static final float MAX_REFRESH_RATE_DELTA = 2.f;

    private final LifetimeAssert mLifetimeAssert;
    private IntentRequestTrackerImpl mIntentRequestTracker;

    private KeyboardVisibilityDelegate mKeyboardVisibilityDelegate =
            KeyboardVisibilityDelegate.getInstance();

    private InsetObserver mInsetObserver;

    // Native pointer to the c++ WindowAndroid object.
    private long mNativeWindowAndroid;
    private final DisplayAndroid mDisplayAndroid;

    // A string used as a key to store intent errors in a bundle
    static final String WINDOW_CALLBACK_ERRORS = "window_callback_errors";

    // Error code returned when an Intent fails to start an Activity.
    public static final int START_INTENT_FAILURE = -1;

    private boolean mIsDestroyed;

    // We use a weak reference here to prevent this from leaking in WebView.
    private final ImmutableWeakReference<Context> mContextRef;

    // We track all animations over content and provide a drawing placeholder for them.
    private HashSet<Animator> mAnimationsOverContent = new HashSet<>();
    private View mAnimationPlaceholderView;

    /** A mechanism for observing and updating the application window's bottom inset. */
    private ApplicationViewportInsetSupplier mApplicationBottomInsetSupplier =
            new ApplicationViewportInsetSupplier();

    private AndroidPermissionDelegate mPermissionDelegate;

    // Note that this state lives in Java, rather than in the native BeginFrameSource because
    // clients may pause VSync before the native WindowAndroid is created.
    private boolean mVSyncPaused;

    // List of display modes with the same dimensions as the current mode but varying refresh rate.
    private List<Display.Mode> mSupportedRefreshRateModes;

    // A container for UnownedUserData objects that are not owned by, but can be accessed through
    // WindowAndroid.
    private final UnownedUserDataHost mUnownedUserDataHost = new UnownedUserDataHost();

    private float mRefreshRate;
    private boolean mHasFocus = true;
    private OverlayTransformApiHelper mOverlayTransformApiHelper;

    // The information required to draw a replica of the progress bar drawn in
    // java UI in composited UI.
    public static class ProgressBarConfig {
        // The background color of the progress bar.
        public int backgroundColor;
        // The height of both the progress indicator and the background.
        public int heightPhysical;
        // The color of the progress indicator.
        public int color;
        // The hairline drawn underneath the bottom edge of the progress bar.
        public int hairlineHeightPhysical;
        // The color of the hairline.
        public int hairlineColor;

        public static interface Provider {
            ProgressBarConfig getProgressBarConfig();
        }
    }

    private ProgressBarConfig.Provider mProgressBarConfigProvider;

    /** An interface to notify listeners that a context menu is closed. */
    public interface OnCloseContextMenuListener {
        /** Called when a context menu has been closed. */
        void onContextMenuClosed();
    }

    /** An interface to notify listeners of the changes in activity state. */
    public interface ActivityStateObserver {
        /** Called when the activity goes into paused state. */
        void onActivityPaused();

        /** Called when the activity goes into resumed state. */
        void onActivityResumed();

        /** Called when the activity goes into destroyed state. */
        void onActivityDestroyed();
    }

    private ObserverList<ActivityStateObserver> mActivityStateObservers = new ObserverList<>();

    /** An interface to notify listeners of the changes in selection handles state. */
    public interface SelectionHandlesObserver {
        /** Called when the selection handles state changes. */
        void onSelectionHandlesStateChanged(boolean active);
    }

    private boolean mSelectionHandlesActive;
    private ObserverList<SelectionHandlesObserver> mSelectionHandlesObservers =
            new ObserverList<>();

    private final boolean mAllowChangeRefreshRate;

    /** Gets the view for readback. */
    public View getReadbackView() {
        return null;
    }

    private final ObserverList<OnCloseContextMenuListener> mContextMenuCloseListeners =
            new ObserverList<>();

    private ModalDialogManager mModalDialogManagerForTesting;

    /**
     * @param context The application {@link Context}.
     */
    public WindowAndroid(Context context) {
        this(context, DisplayAndroid.getNonMultiDisplay(context));
    }

    protected WindowAndroid(Context context, IntentRequestTracker tracker) {
        this(context, DisplayAndroid.getNonMultiDisplay(context));
        mIntentRequestTracker = (IntentRequestTrackerImpl) tracker;
    }

    /**
     * @param context The application {@link Context}.
     * @param display The application {@link DisplayAndroid}.
     */
    @SuppressLint("UseSparseArrays")
    protected WindowAndroid(Context context, DisplayAndroid display) {
        mLifetimeAssert = LifetimeAssert.create(this);
        // context does not have the same lifetime guarantees as an application context so we can't
        // hold a strong reference to it.
        mContextRef = new ImmutableWeakReference<>(context);
        mDisplayAndroid = display;
        mDisplayAndroid.addObserver(this);

        // Using this setting is gated to Q due to bugs on Razer phones which can freeze the device
        // if the API is used. See crbug.com/990646.
        // Disable refresh rate change on TV platforms, as it may cause black screen flicker due to
        // display mode changes.
        mAllowChangeRefreshRate = Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && !isTv(context);

        // Multiple refresh rate support is only available on M+.
        recomputeSupportedRefreshRates();

        // Configuration.isDisplayServerWideColorGamut must be queried from the window's context.
        // Because of crbug.com/756180, many devices report true for isScreenWideColorGamut in
        // 8.0.0, even when they don't actually support wide color gamut.
        // TODO(boliu): Observe configuration changes to update the value of isScreenWideColorGamut.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && !Build.VERSION.RELEASE.equals("8.0.0")
                && ContextUtils.activityFromContext(context) != null) {
            Configuration configuration = context.getResources().getConfiguration();
            boolean isScreenWideColorGamut = configuration.isScreenWideColorGamut();
            display.updateIsDisplayServerWideColorGamut(isScreenWideColorGamut);
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2) {
            mOverlayTransformApiHelper = OverlayTransformApiHelper.create(this);
        }
    }

    private static boolean isTv(Context context) {
        UiModeManager uiModeManager =
                (UiModeManager) context.getSystemService(Context.UI_MODE_SERVICE);
        return uiModeManager != null
                && uiModeManager.getCurrentModeType() == Configuration.UI_MODE_TYPE_TELEVISION;
    }

    @CalledByNativeForTesting
    private static long createForTesting() {
        WindowAndroid windowAndroid = new WindowAndroid(ContextUtils.getApplicationContext());
        // |windowAndroid.getNativePointer()| creates native WindowAndroid object
        // which stores a global ref to |windowAndroid|. Therefore |windowAndroid|
        // is not immediately eligible for gc.
        return windowAndroid.getNativePointer();
    }

    @CalledByNative
    private void clearNativePointer() {
        mNativeWindowAndroid = 0;
    }

    /** Set the delegate that will handle android permissions requests. */
    public void setAndroidPermissionDelegate(AndroidPermissionDelegate delegate) {
        mPermissionDelegate = delegate;
    }

    /** Gets the {@link IntentRequestTracker} associated with the WindowAndroid's activity. */
    @Nullable
    public final IntentRequestTracker getIntentRequestTracker() {
        if (mIntentRequestTracker == null) {
            Log.w(
                    TAG,
                    "Cannot get IntentRequestTracker as the WindowAndroid is neither "
                            + "a ActivityWindowAndroid or a FragmentWindowAndroid.");
        }
        return mIntentRequestTracker;
    }

    /** Set the provider to pull the progress bar config from */
    public final void setProgressBarConfigProvider(ProgressBarConfig.Provider provider) {
        mProgressBarConfigProvider = provider;
    }

    /**
     * Shows an intent and returns the results to the callback object.
     *
     * @param intent The PendingIntent that needs to be shown.
     * @param callback The object that will receive the results for the intent.
     * @param errorId The ID of error string to be shown if activity is paused before intent
     *     results, or null if no message is required.
     * @return Whether the intent was shown.
     */
    public boolean showIntent(PendingIntent intent, IntentCallback callback, Integer errorId) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
            return false;
        }
        return mIntentRequestTracker.showCancelableIntent(intent, callback, errorId) >= 0;
    }

    /**
     * Shows an intent and returns the results to the callback object.
     * @param intent   The intent that needs to be shown.
     * @param callback The object that will receive the results for the intent.
     * @param errorId  The ID of error string to be shown if activity is paused before intent
     *                 results, or null if no message is required.
     * @return Whether the intent was shown.
     */
    public boolean showIntent(Intent intent, IntentCallback callback, Integer errorId) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
            return false;
        }
        return mIntentRequestTracker.showCancelableIntent(intent, callback, errorId) >= 0;
    }

    /**
     * Shows an intent that could be canceled and returns the results to the callback object.
     * @param  intent   The PendingIntent that needs to be shown.
     * @param  callback The object that will receive the results for the intent.
     * @param  errorId  The ID of error string to be shown if activity is paused before intent
     *                  results, or null if no message is required.
     * @return A non-negative request code that could be used for finishActivity, or
     *         START_INTENT_FAILURE if failed.
     */
    public int showCancelableIntent(
            PendingIntent intent, IntentCallback callback, Integer errorId) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
            return START_INTENT_FAILURE;
        }
        return mIntentRequestTracker.showCancelableIntent(intent, callback, errorId);
    }

    /**
     * Shows an intent that could be canceled and returns the results to the callback object.
     * @param  intent   The intent that needs to be shown.
     * @param  callback The object that will receive the results for the intent.
     * @param  errorId  The ID of error string to be shown if activity is paused before intent
     *                  results, or null if no message is required.
     * @return A non-negative request code that could be used for finishActivity, or
     *         START_INTENT_FAILURE if failed.
     */
    public int showCancelableIntent(Intent intent, IntentCallback callback, Integer errorId) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't show intent as context is not an Activity: " + intent);
            return START_INTENT_FAILURE;
        }
        return mIntentRequestTracker.showCancelableIntent(intent, callback, errorId);
    }

    public int showCancelableIntent(
            Callback<Integer> intentTrigger, IntentCallback callback, Integer errorId) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't show intent as context is not an Activity");
            return START_INTENT_FAILURE;
        }
        return mIntentRequestTracker.showCancelableIntent(intentTrigger, callback, errorId);
    }

    /**
     * Force finish another activity that you had previously started with showCancelableIntent.
     * @param requestCode The request code returned from showCancelableIntent.
     */
    public void cancelIntent(int requestCode) {
        if (mIntentRequestTracker == null) {
            Log.d(TAG, "Can't cancel intent as context is not an Activity: " + requestCode);
            return;
        }
        mIntentRequestTracker.cancelIntent(requestCode);
    }

    /**
     * Removes a callback from the list of pending intents, so that nothing happens if/when the
     * result for that intent is received.
     *
     * @param callback The object that should have received the results
     * @return True if the callback was removed, false if it was not found.
     */
    public boolean removeIntentCallback(IntentCallback callback) {
        if (mIntentRequestTracker == null) return false;
        return mIntentRequestTracker.removeIntentCallback(callback);
    }

    /**
     * Determine whether access to a particular permission is granted.
     * @param permission The permission whose access is to be checked.
     * @return Whether access to the permission is granted.
     */
    @CalledByNative
    @Override
    public boolean hasPermission(String permission) {
        if (mPermissionDelegate != null) return mPermissionDelegate.hasPermission(permission);

        return ApiCompatibilityUtils.checkPermission(
                        ContextUtils.getApplicationContext(),
                        permission,
                        Process.myPid(),
                        Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /**
     * Determine whether the specified permission can be requested.
     *
     * <p>
     * A permission can be requested in the following states:
     * 1.) Default un-granted state, permission can be requested
     * 2.) Permission previously requested but denied by the user, but the user did not select
     *     "Never ask again".
     *
     * @param permission The permission name.
     * @return Whether the requesting the permission is allowed.
     */
    @CalledByNative
    @Override
    public boolean canRequestPermission(String permission) {
        if (mPermissionDelegate != null) {
            return mPermissionDelegate.canRequestPermission(permission);
        }

        Log.w(
                TAG,
                "Cannot determine the request permission state as the context "
                        + "is not an Activity");
        assert false
                : "Failed to determine the request permission state using a WindowAndroid "
                        + "without an Activity";
        return false;
    }

    /**
     * Determine whether the specified permission is revoked by policy.
     *
     * @param permission The permission name.
     * @return Whether the permission is revoked by policy and the user has no ability to change it.
     */
    @Override
    public boolean isPermissionRevokedByPolicy(String permission) {
        if (mPermissionDelegate != null) {
            return mPermissionDelegate.isPermissionRevokedByPolicy(permission);
        }

        Log.w(
                TAG,
                "Cannot determine the policy permission state as the context "
                        + "is not an Activity");
        assert false
                : "Failed to determine the policy permission state using a WindowAndroid "
                        + "without an Activity";
        return false;
    }

    /**
     * Requests the specified permissions are granted for further use.
     * @param permissions The list of permissions to request access to.
     * @param callback The callback to be notified whether the permissions were granted.
     */
    @Override
    public void requestPermissions(String[] permissions, PermissionCallback callback) {
        if (mPermissionDelegate != null) {
            mPermissionDelegate.requestPermissions(permissions, callback);
            return;
        }

        Log.w(TAG, "Cannot request permissions as the context is not an Activity");
        assert false : "Failed to request permissions using a WindowAndroid without an Activity";
    }

    @Override
    public boolean handlePermissionResult(
            int requestCode, String[] permissions, int[] grantResults) {
        if (mPermissionDelegate != null) {
            return mPermissionDelegate.handlePermissionResult(
                    requestCode, permissions, grantResults);
        }
        return false;
    }

    @Override
    public boolean shouldShowRequestPermissionRationale(String permission) {
        return mPermissionDelegate != null
                && mPermissionDelegate.shouldShowRequestPermissionRationale(permission);
    }

    /**
     * Displays an error message with a provided error message string.
     * @param error The error message string to be displayed.
     */
    public static void showError(String error) {
        if (error != null) {
            Toast.makeText(ContextUtils.getApplicationContext(), error, Toast.LENGTH_SHORT).show();
        }
    }

    /**
     * Displays an error message from the given resource id.
     * @param resId The error message string's resource id.
     */
    public static void showError(int resId) {
        showError(ContextUtils.getApplicationContext().getString(resId));
    }

    /** Broadcasts the given intent to all interested BroadcastReceivers. */
    public void sendBroadcast(Intent intent) {
        ContextUtils.getApplicationContext().sendBroadcast(intent);
    }

    /**
     * @return DisplayAndroid instance belong to this window.
     */
    public DisplayAndroid getDisplay() {
        return mDisplayAndroid;
    }

    /**
     * @return A reference to owning Activity.  The returned WeakReference will never be null, but
     *         the contained Activity can be null (either if it has been garbage collected or if
     *         this is in the context of a WebView that was not created using an Activity).
     *         The returned WeakReference is immutable and calling clear will throw an exception.
     */
    public WeakReference<Activity> getActivity() {
        return NULL_ACTIVITY_WEAK_REF;
    }

    /**
     * @return The application context for this activity.
     */
    public Context getApplicationContext() {
        return ContextUtils.getApplicationContext();
    }

    /**
     * Notify any observers that the visibility of the Android Window associated
     * with this Window has changed.
     * @param visible whether the View is visible.
     */
    public void onVisibilityChanged(boolean visible) {
        if (mNativeWindowAndroid == 0) return;
        WindowAndroidJni.get()
                .onVisibilityChanged(mNativeWindowAndroid, WindowAndroid.this, visible);
    }

    /**
     * For window instances associated with an activity, notifies any listeners
     * that the activity has been stopped.
     */
    protected void onActivityStopped() {
        if (mNativeWindowAndroid == 0) return;
        WindowAndroidJni.get().onActivityStopped(mNativeWindowAndroid, WindowAndroid.this);
    }

    /**
     * For window instances associated with an activity, notifies any listeners
     * that the activity has been started.
     */
    protected void onActivityStarted() {
        if (mNativeWindowAndroid == 0) return;
        WindowAndroidJni.get().onActivityStarted(mNativeWindowAndroid, WindowAndroid.this);
    }

    protected void onActivityPaused() {
        for (ActivityStateObserver observer : mActivityStateObservers) observer.onActivityPaused();
    }

    protected void onActivityResumed() {
        for (ActivityStateObserver observer : mActivityStateObservers) observer.onActivityResumed();
    }

    protected void onActivityDestroyed() {
        for (ActivityStateObserver observer : mActivityStateObservers) {
            observer.onActivityDestroyed();
        }
    }

    /** Adds a new {@link ActivityStateObserver} instance. */
    public void addActivityStateObserver(ActivityStateObserver observer) {
        assert !mActivityStateObservers.hasObserver(observer);
        mActivityStateObservers.addObserver(observer);
    }

    /** Removes a new {@link ActivityStateObserver} instance. */
    public void removeActivityStateObserver(ActivityStateObserver observer) {
        assert mActivityStateObservers.hasObserver(observer);
        mActivityStateObservers.removeObserver(observer);
    }

    public void addSelectionHandlesObserver(SelectionHandlesObserver observer) {
        assert !mSelectionHandlesObservers.hasObserver(observer);
        mSelectionHandlesObservers.addObserver(observer);
        observer.onSelectionHandlesStateChanged(mSelectionHandlesActive);
    }

    public void removeSelectionHandlesObserver(SelectionHandlesObserver observer) {
        assert mSelectionHandlesObservers.hasObserver(observer);
        mSelectionHandlesObservers.removeObserver(observer);
    }

    /** Removes a new {@link ActivityStateObserver} instance. */
    @CalledByNative
    private void onSelectionHandlesStateChanged(boolean active) {
        mSelectionHandlesActive = active;
        for (SelectionHandlesObserver observer : mSelectionHandlesObservers) {
            observer.onSelectionHandlesStateChanged(active);
        }
    }

    @CalledByNative
    private int[] getProgressBarConfig() {
        int[] result = new int[5];

        if (mProgressBarConfigProvider == null) {
            Arrays.fill(result, 0);
            return result;
        }

        ProgressBarConfig config = mProgressBarConfigProvider.getProgressBarConfig();
        result[0] = config.backgroundColor;
        result[1] = config.heightPhysical;
        result[2] = config.color;
        result[3] = config.hairlineHeightPhysical;
        result[4] = config.hairlineColor;
        return result;
    }

    /**
     * @return Current state of the associated {@link Activity}. Can be overridden to return the
     *     correct state. {@code ActivityState.DESTROYED} by default.
     */
    @ActivityState
    public int getActivityState() {
        return ActivityState.DESTROYED;
    }

    /** An interface that intent callback objects have to implement. */
    public interface IntentCallback {
        /**
         * Handles the data returned by the requested intent.
         * @param resultCode Result code of the requested intent.
         * @param data The data returned by the intent.
         */
        void onIntentCompleted(int resultCode, Intent data);
    }

    /**
     * Tests that an activity is available to handle the passed in intent.
     * @param  intent The intent to check.
     * @return True if an activity is available to process this intent when started, meaning that
     *         Context.startActivity will not throw ActivityNotFoundException.
     */
    public boolean canResolveActivity(Intent intent) {
        return PackageManagerUtils.canResolveActivity(intent);
    }

    /**
     * Returns the ModalDialogManager to be used with this window.
     *
     * @return a {@link ModalDialogManager} for this window, or null if there is none.
     */
    public @Nullable ModalDialogManager getModalDialogManager() {
        // Will always be null except for some tests.
        return mModalDialogManagerForTesting;
    }

    /** Set the ModalDialogManager for testing. */
    @CalledByNativeForTesting
    private void setModalDialogManagerForTesting(ModalDialogManager modalDialogManager) {
        mModalDialogManagerForTesting = modalDialogManager;
    }

    @CalledByNative
    private long getNativeModalDialogManagerBridge() {
        ModalDialogManager manager = getModalDialogManager();
        if (manager == null) {
            return 0;
        }
        return manager.getOrCreateNativeBridge();
    }

    /**
     * @return Whether this instance is destroyed.
     */
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    /** Destroys the c++ WindowAndroid object if one has been created. */
    @CalledByNative
    public void destroy() {
        LifetimeAssert.setSafeToGc(mLifetimeAssert, true);
        mIsDestroyed = true;
        mDisplayAndroid.removeObserver(this);
        if (mNativeWindowAndroid != 0) {
            // Native code clears |mNativeWindowAndroid|.
            WindowAndroidJni.get().destroy(mNativeWindowAndroid, WindowAndroid.this);
        }

        mUnownedUserDataHost.destroy();
        mApplicationBottomInsetSupplier.destroy();

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2) {
            if (mOverlayTransformApiHelper != null) {
                mOverlayTransformApiHelper.destroy();
            }
        }
    }

    /**
     * Returns a pointer to the c++ AndroidWindow object and calls the initializer if
     * the object has not been previously initialized.
     * @return A pointer to the c++ AndroidWindow.
     */
    @CalledByNative
    private long getNativePointer() {
        if (mNativeWindowAndroid == 0) {
            mNativeWindowAndroid =
                    WindowAndroidJni.get()
                            .init(
                                    WindowAndroid.this,
                                    mDisplayAndroid.getDisplayId(),
                                    getMouseWheelScrollFactor(),
                                    getWindowIsWideColorGamut());
            WindowAndroidJni.get()
                    .setVSyncPaused(mNativeWindowAndroid, WindowAndroid.this, mVSyncPaused);
        }
        return mNativeWindowAndroid;
    }

    /**
     * Returns current wheel scroll factor (physical pixels per mouse scroll click).
     * @return wheel scroll factor or zero if attr retrieval fails.
     */
    private float getMouseWheelScrollFactor() {
        TypedValue outValue = new TypedValue();
        Context context = getContext().get();
        if (context != null
                && context.getTheme()
                        .resolveAttribute(android.R.attr.listPreferredItemHeight, outValue, true)) {
            // This is the same attribute used by Android Views to scale wheel
            // event motion into scroll deltas.
            return outValue.getDimension(context.getResources().getDisplayMetrics());
        }
        return 0;
    }

    // Helper to get the android Window. Always null for application context. Need to null check
    // result returning value.
    @Nullable
    public Window getWindow() {
        Activity activity = ContextUtils.activityFromContext(mContextRef.get());
        if (activity == null || activity.isFinishing()) return null;
        return activity.getWindow();
    }

    // This is android.view.Window.isWideColorGamut, which is only set if the passed in Context is
    // an Activity. This is normally not needed for apps which can decide whether to enable wide
    // gamut (on supported hardware and os). However it is important for embedders like WebView
    // which do not make the wide gamut decision to check this at run time.
    private boolean getWindowIsWideColorGamut() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return false;
        Window window = getWindow();
        if (window == null) return false;
        return window.isWideColorGamut();
    }

    /**
     * Set the animation placeholder view, which we set to 'draw' during animations, such that
     * animations aren't clipped by the SurfaceView 'hole'. This can be the SurfaceView itself or a
     * view directly on top of it. This could be extended to many views if we ever need it.
     */
    public void setAnimationPlaceholderView(View view) {
        mAnimationPlaceholderView = view;
    }

    protected void setKeyboardDelegate(KeyboardVisibilityDelegate keyboardDelegate) {
        mKeyboardVisibilityDelegate = keyboardDelegate;
        // TODO(crbug.com/343936788): Remove - callers should use the window to get the delegate.
        KeyboardVisibilityDelegate.setInstance(keyboardDelegate);
    }

    /**
     * The returned {@link KeyboardVisibilityDelegate} can read and influence the soft keyboard.
     *
     * @return a {@link KeyboardVisibilityDelegate} specific for this window.
     */
    public KeyboardVisibilityDelegate getKeyboardDelegate() {
        return mKeyboardVisibilityDelegate;
    }

    /** Returns the {@link InsetObserver} for the root view of the activity or null. */
    public InsetObserver getInsetObserver() {
        if (mInsetObserver == null) {
            Window window = getWindow();
            if (window == null) return null;

            mInsetObserver =
                    new InsetObserver(
                            new ImmutableWeakReference<>(window.getDecorView().getRootView()));
        }
        return mInsetObserver;
    }

    /**
     * @return A mechanism for updating and observing the bottom inset of the browser window.
     */
    public ApplicationViewportInsetSupplier getApplicationBottomInsetSupplier() {
        return mApplicationBottomInsetSupplier;
    }

    /** Adds a listener that will be notified whenever a ContextMenu is closed. */
    public void addContextMenuCloseListener(OnCloseContextMenuListener listener) {
        mContextMenuCloseListeners.addObserver(listener);
    }

    /**
     * Removes a listener from the list of listeners that will be notified when a
     * ContextMenu is closed.
     */
    public void removeContextMenuCloseListener(OnCloseContextMenuListener listener) {
        mContextMenuCloseListeners.removeObserver(listener);
    }

    /**
     * This hook is called whenever the context menu is being closed (either by
     * the user canceling the menu with the back/menu button, or when an item is
     * selected).
     */
    public void onContextMenuClosed() {
        for (OnCloseContextMenuListener listener : mContextMenuCloseListeners) {
            listener.onContextMenuClosed();
        }
    }

    /**
     * Start a post-layout animation on top of web content.
     *
     * By default, Android optimizes what it shows on top of SurfaceViews (saves power).
     * Effectively, layouts determine what gets drawn and post-layout animations outside
     * of this area may be 'clipped'. Using this method to start such animations will
     * ensure that nothing is clipped during the animation, and restore the optimal
     * state when the animation ends.
     */
    public void startAnimationOverContent(Animator animation) {
        // We may not need an animation placeholder (eg. Webview doesn't use SurfaceView)
        if (mAnimationPlaceholderView == null) return;
        if (animation.isStarted()) throw new IllegalArgumentException("Already started.");
        boolean added = mAnimationsOverContent.add(animation);
        if (!added) throw new IllegalArgumentException("Already Added.");

        // We start the animation in this method to help guarantee that we never get stuck in this
        // state or leak objects into the set. Starting the animation here should guarantee that we
        // get an onAnimationEnd callback, and remove this animation.
        animation.start();

        // When the first animation starts, make the placeholder 'draw' itself.
        if (mAnimationPlaceholderView.willNotDraw()) {
            mAnimationPlaceholderView.setWillNotDraw(false);
        }

        // When the last animation ends, remove the placeholder view,
        // returning to the default optimized state.
        animation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        animation.removeListener(this);
                        mAnimationsOverContent.remove(animation);
                        if (mAnimationsOverContent.isEmpty()) {
                            mAnimationPlaceholderView.setWillNotDraw(true);
                        }
                    }
                });
    }

    /**
     * Getter for the current context (not necessarily the application context).
     * Make no assumptions regarding what type of Context is returned here, it could be for example
     * an Activity or a Context created specifically to target an external display.
     * The returned WeakReference is immutable and calling clear will throw an exception.
     */
    public WeakReference<Context> getContext() {
        return mContextRef;
    }

    /** Return the current window token, or null. */
    public IBinder getWindowToken() {
        Window window = getWindow();
        if (window == null) return null;
        View decorView = window.peekDecorView();
        if (decorView == null) return null;
        return decorView.getWindowToken();
    }

    /**
     * As long as there are still animations which haven't ended, this will return false.
     * @return True if all known animations have ended.
     */
    @VisibleForTesting
    public boolean haveAnimationsEnded() {
        return mAnimationsOverContent.isEmpty();
    }

    /**
     * Pauses/Unpauses VSync. When VSync is paused the compositor for this window will idle, and
     * requestAnimationFrame callbacks won't fire, etc.
     */
    public void setVSyncPaused(boolean paused) {
        if (mVSyncPaused == paused) return;
        mVSyncPaused = paused;
        if (mNativeWindowAndroid != 0) {
            WindowAndroidJni.get().setVSyncPaused(mNativeWindowAndroid, WindowAndroid.this, paused);
        }
    }

    @Override
    public void onRefreshRateChanged(float refreshRate) {
        if (mNativeWindowAndroid != 0) {
            WindowAndroidJni.get()
                    .onUpdateRefreshRate(mNativeWindowAndroid, WindowAndroid.this, refreshRate);
        }
    }

    protected void onWindowFocusChanged(boolean hasFocus) {
        mHasFocus = hasFocus;
        if (!mHasFocus) {
            // `preferredDisplayModeId` affects other windows even when this window is not in focus,
            // so reset to no preference when not in focus.
            doSetPreferredRefreshRate(0);
        } else {
            doSetPreferredRefreshRate(mRefreshRate);
        }
    }

    @Override
    public void onCurrentModeChanged(Display.Mode currentMode) {
        recomputeSupportedRefreshRates();
    }

    @Override
    public void onDisplayModesChanged(List<Display.Mode> supportedModes) {
        recomputeSupportedRefreshRates();
    }

    @RequiresApi(Build.VERSION_CODES.O)
    @CalledByNative
    public void setWideColorEnabled(boolean enabled) {
        // Although this API was added in Android O, it was buggy.
        // Restrict to Android Q, where it was fixed.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            assert !enabled;
            return;
        }
        Window window = getWindow();
        if (window == null) return;

        int colorMode =
                enabled
                        ? ActivityInfo.COLOR_MODE_WIDE_COLOR_GAMUT
                        : ActivityInfo.COLOR_MODE_DEFAULT;
        window.setColorMode(colorMode);
    }

    private void recomputeSupportedRefreshRates() {
        Display.Mode currentMode = mDisplayAndroid.getCurrentMode();
        List<Display.Mode> supportedModes = mDisplayAndroid.getSupportedModes();
        // Note: getCurrentMode() and getSupportedModes() can return null in some situations - see
        // crbug.com/1401514.
        if (currentMode == null || supportedModes == null || supportedModes.size() == 0) {
            return;
        }

        List<Display.Mode> supportedRefreshRateModes = new ArrayList<Display.Mode>();
        for (int i = 0; i < supportedModes.size(); ++i) {
            if (currentMode.equals(supportedModes.get(i))) {
                supportedRefreshRateModes.add(supportedModes.get(i));
                continue;
            }

            // Only advertise refresh rates which wouldn't change other configurations on the
            // Display.
            if (currentMode.getPhysicalWidth() == supportedModes.get(i).getPhysicalWidth()
                    && currentMode.getPhysicalHeight() == supportedModes.get(i).getPhysicalHeight()
                    && currentMode.getRefreshRate() != supportedModes.get(i).getRefreshRate()) {
                supportedRefreshRateModes.add(supportedModes.get(i));
                continue;
            }
        }

        boolean changed = !supportedRefreshRateModes.equals(mSupportedRefreshRateModes);
        if (changed) {
            mSupportedRefreshRateModes = supportedRefreshRateModes;
            if (mNativeWindowAndroid != 0) {
                WindowAndroidJni.get()
                        .onSupportedRefreshRatesUpdated(
                                mNativeWindowAndroid,
                                WindowAndroid.this,
                                getSupportedRefreshRates());
            }
        }
    }

    @CalledByNative
    private float getRefreshRate() {
        return mDisplayAndroid.getRefreshRate();
    }

    @SuppressLint("NewApi")
    // mSupportedRefreshRateModes should only be set if Display.Mode is available.
    @CalledByNative
    private float[] getSupportedRefreshRates() {
        if (mSupportedRefreshRateModes == null || !mAllowChangeRefreshRate) return null;

        float[] supportedRefreshRates = new float[mSupportedRefreshRateModes.size()];
        for (int i = 0; i < mSupportedRefreshRateModes.size(); ++i) {
            supportedRefreshRates[i] = mSupportedRefreshRateModes.get(i).getRefreshRate();
        }
        return supportedRefreshRates;
    }

    @SuppressLint("NewApi")
    @CalledByNative
    private void setPreferredRefreshRate(float preferredRefreshRate) {
        mRefreshRate = preferredRefreshRate;
        if (mHasFocus) doSetPreferredRefreshRate(preferredRefreshRate);
    }

    private void doSetPreferredRefreshRate(float preferredRefreshRate) {
        if (mSupportedRefreshRateModes == null || !mAllowChangeRefreshRate) return;

        int preferredModeId = getPreferredModeId(preferredRefreshRate);
        Window window = getWindow();
        if (window == null) return;
        WindowManager.LayoutParams params = window.getAttributes();
        if (params.preferredDisplayModeId == preferredModeId) return;

        params.preferredDisplayModeId = preferredModeId;
        window.setAttributes(params);
    }

    @SuppressLint("NewApi")
    // mSupportedRefreshRateModes should only be set if Display.Mode is available.
    private int getPreferredModeId(float preferredRefreshRate) {
        if (preferredRefreshRate == 0) return 0;

        Display.Mode preferredMode = null;
        float preferredModeDelta = Float.MAX_VALUE;

        for (int i = 0; i < mSupportedRefreshRateModes.size(); ++i) {
            Display.Mode mode = mSupportedRefreshRateModes.get(i);
            float delta = Math.abs(preferredRefreshRate - mode.getRefreshRate());
            if (delta < preferredModeDelta) {
                preferredModeDelta = delta;
                preferredMode = mode;
            }
        }

        if (preferredModeDelta > MAX_REFRESH_RATE_DELTA) {
            Log.e(TAG, "Refresh rate not supported : " + preferredRefreshRate);
            return 0;
        }

        return preferredMode.getModeId();
    }

    /**
     * @return The {@link UnownedUserDataHost} attached to the current {@link WindowAndroid}.
     */
    public UnownedUserDataHost getUnownedUserDataHost() {
        return mUnownedUserDataHost;
    }

    void onOverlayTransformUpdated() {
        if (mNativeWindowAndroid != 0) {
            WindowAndroidJni.get().onOverlayTransformUpdated(mNativeWindowAndroid, this);
        }
    }

    @CalledByNative
    private @OverlayTransform int getOverlayTransform() {
        int overlayTransform = OverlayTransform.INVALID;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S_V2
                && mOverlayTransformApiHelper != null) {
            overlayTransform = mOverlayTransformApiHelper.getOverlayTransform();
        }
        // Fallback to display rotation
        if (overlayTransform == OverlayTransform.INVALID) {
            switch (mDisplayAndroid.getRotation()) {
                case Surface.ROTATION_0:
                    return OverlayTransform.NONE;
                case Surface.ROTATION_90:
                    return OverlayTransform.ROTATE_CLOCKWISE_90;
                case Surface.ROTATION_180:
                    return OverlayTransform.ROTATE_CLOCKWISE_180;
                case Surface.ROTATION_270:
                    return OverlayTransform.ROTATE_CLOCKWISE_270;
                default:
                    return OverlayTransform.NONE;
            }
        }
        return overlayTransform;
    }

    public void setUnfoldLatencyBeginTime(long beginTimestampMs) {
        try (TraceEvent e = TraceEvent.scoped("setUnfoldLatencyBeginTime")) {
            if (mNativeWindowAndroid != 0) {
                WindowAndroidJni.get()
                        .sendUnfoldLatencyBeginTimestamp(mNativeWindowAndroid, beginTimestampMs);
            }
        }
    }

    @NativeMethods
    interface Natives {
        long init(
                WindowAndroid caller,
                int displayId,
                float scrollFactor,
                boolean windowIsWideColorGamut);

        void onVisibilityChanged(long nativeWindowAndroid, WindowAndroid caller, boolean visible);

        void onActivityStopped(long nativeWindowAndroid, WindowAndroid caller);

        void onActivityStarted(long nativeWindowAndroid, WindowAndroid caller);

        void setVSyncPaused(long nativeWindowAndroid, WindowAndroid caller, boolean paused);

        void onUpdateRefreshRate(long nativeWindowAndroid, WindowAndroid caller, float refreshRate);

        void destroy(long nativeWindowAndroid, WindowAndroid caller);

        void onSupportedRefreshRatesUpdated(
                long nativeWindowAndroid, WindowAndroid caller, float[] supportedRefreshRates);

        void onOverlayTransformUpdated(long nativeWindowAndroid, WindowAndroid caller);

        void sendUnfoldLatencyBeginTimestamp(long nativeWindowAndroid, long beginTimestampMs);
    }
}
