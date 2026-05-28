// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui;

import android.content.Context;
import android.content.res.TypedArray;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.LayoutRes;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import androidx.appcompat.app.AppCompatViewInflater;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;

/**
 * A utility class for inflating layouts asynchronously using Chromium's TaskRunner.
 *
 * <p>This is a replacement for AndroidX's {@code AsyncLayoutInflater} that uses PostTask instead of
 * a dedicated Thread. It also removes the logic that retries inflation on the UI thread should any
 * exception be thrown.
 *
 * <p>Note: This class automatically supports AppCompat and Material Design view rewrites by
 * dynamically loading the theme's configured {@link AppCompatViewInflater}. See {@link
 * AsyncViewStub} Javadoc for more details on thread safety risks and configuration change risks.
 */
@NullMarked
public final class AsyncLayoutInflater {
    private static final TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.USER_VISIBLE);

    private final Context mContext;

    /**
     * Creates a new AsyncLayoutInflater. Must be called on the UI thread.
     *
     * @param context The context to use for inflation.
     */
    public AsyncLayoutInflater(Context context) {
        mContext = context;
    }

    /**
     * Inflates a layout resource asynchronously on a background thread.
     *
     * @param resId The layout resource ID to inflate.
     * @param parent Optional parent view.
     * @param callback Callback to be invoked on the UI thread when inflation finishes. Provides the
     *     inflated View.
     */
    @UiThread
    public void inflate(@LayoutRes int resId, @Nullable ViewGroup parent, Callback<View> callback) {
        new AsyncTask<View>() {
            @Override
            protected View doInBackground() {
                return new AppCompatInflater(mContext)
                        .inflate(resId, parent, /* attachToRoot= */ false);
            }

            @Override
            protected void onPostExecute(View view) {
                callback.onResult(view);
            }
        }.executeOnTaskRunner(sSequencedTaskRunner);
    }

    /**
     * Inflates a layout resource synchronously on the current thread using the same {@link
     * AppCompatInflater} semantics.
     *
     * @param resId The layout resource ID to inflate.
     * @param parent Optional parent view.
     * @return The inflated view.
     */
    public View inflateSync(@LayoutRes int resId, @Nullable ViewGroup parent) {
        return inflateSync(resId, parent, /* attachToRoot= */ false);
    }

    public View inflateSync(
            @LayoutRes int resId, @Nullable ViewGroup parent, boolean attachToRoot) {
        return LayoutInflaterUtils.inflate(
                new AppCompatInflater(mContext), resId, parent, attachToRoot);
    }

    /**
     * Same as androidx's AsyncLayoutInflater.BasicInflater, except uses AppCompatViewInflater so
     * that clients do not need to use a AsyncAppCompatFactory with it.
     */
    private static class AppCompatInflater extends LayoutInflater {
        private static final String TAG = "AppCompatInflater";
        private static final String[] sClassPrefixList = {
            "android.widget.", "android.webkit.", "android.app."
        };

        private @Nullable AppCompatViewInflater mAppCompatViewInflater;

        AppCompatInflater(Context context) {
            super(context);
        }

        @Override
        public LayoutInflater cloneInContext(Context newContext) {
            return new AppCompatInflater(newContext);
        }

        @Override
        protected View onCreateView(String name, AttributeSet attrs) throws ClassNotFoundException {
            if (mAppCompatViewInflater == null) {
                mAppCompatViewInflater = createAppCompatViewInflater(getContext());
            }

            View view =
                    mAppCompatViewInflater.createView(
                            /* parent= */ null,
                            name,
                            getContext(),
                            attrs,
                            /* inheritContext= */ false, // No parent to inherit from
                            /* readAndroidTheme= */ false, // Obsoleted in API 21
                            /* readAppTheme= */ true, // Support both app:theme and android:theme
                            /* wrapContext= */ false); // Obsoleted in API 21
            if (view != null) {
                return view;
            }

            for (String prefix : sClassPrefixList) {
                try {
                    view = createView(name, prefix, attrs);
                    if (view != null) {
                        return view;
                    }
                } catch (ClassNotFoundException e) {
                    // Let the base class handle it.
                }
            }

            return super.onCreateView(name, attrs);
        }

        private AppCompatViewInflater createAppCompatViewInflater(Context context) {
            TypedArray a = context.obtainStyledAttributes(R.styleable.AppCompatTheme);
            String viewInflaterClassName =
                    a.getString(R.styleable.AppCompatTheme_viewInflaterClass);
            a.recycle();

            if (viewInflaterClassName == null) {
                return new AppCompatViewInflater();
            }

            try {
                Class<?> viewInflaterClass =
                        context.getClassLoader().loadClass(viewInflaterClassName);
                return (AppCompatViewInflater)
                        viewInflaterClass.getDeclaredConstructor().newInstance();
            } catch (Throwable t) {
                Log.w(
                        TAG,
                        "Failed to instantiate custom view inflater "
                                + viewInflaterClassName
                                + ". Falling back to default.",
                        t);
                return new AppCompatViewInflater();
            }
        }
    }
}
