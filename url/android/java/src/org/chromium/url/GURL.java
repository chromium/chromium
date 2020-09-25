// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;

import java.util.Random;

/**
 * A Java wrapper for GURL, Chromium's URL parsing library.
 *
 * This class is safe to use during startup, but will block on the native library being sufficiently
 * loaded to use native GURL (and will not wait for content initialization). In practice it's very
 * unlikely that this will actually block startup unless used extremely early, in which case you
 * should probably seek an alternative solution to using GURL.
 *
 * The design of this class avoids destruction/finalization by caching all values necessary to
 * reconstruct a GURL in Java, allowing it to be much faster in the common case and easier to use.
 */
@JNINamespace("url")
@MainDex
public class GURL {
    private static final String TAG = "GURL";
    /* package */ static final int SERIALIZER_VERSION = 1;
    /* package */ static final char SERIALIZER_DELIMITER = '\0';

    @FunctionalInterface
    public interface ReportDebugThrowableCallback {
        void run(Throwable throwable);
    }

    // Right now this is only collecting reports on Canary which has a relatively small population.
    private static final int DEBUG_REPORT_PERCENTAGE = 10;
    private static ReportDebugThrowableCallback sReportCallback;

    // TODO(https://crbug.com/1039841): Right now we return a new String with each request for a
    //      GURL component other than the spec itself. Should we cache return Strings (as
    //      WeakReference?) so that callers can share String memory?
    private String mSpec;
    private boolean mIsValid;
    private Parsed mParsed;

    private static class Holder { private static GURL sEmptyGURL = new GURL(""); }

    @CalledByNative
    public static GURL emptyGURL() {
        return Holder.sEmptyGURL;
    }

    /**
     * Create a new GURL.
     *
     * @param uri The string URI representation to parse into a GURL.
     */
    public GURL(String uri) {
        // Avoid a jni hop (and initializing the native library) for empty GURLs.
        if (TextUtils.isEmpty(uri)) {
            mSpec = "";
            mParsed = Parsed.createEmpty();
            return;
        }
        ensureNativeInitializedForGURL();
        GURLJni.get().init(uri, this);
    }

    @CalledByNative
    protected GURL() {}

    /**
     * Enables debug stack trace gathering for GURL.
     */
    public static void setReportDebugThrowableCallback(ReportDebugThrowableCallback callback) {
        sReportCallback = callback;
    }

    /**
     * Ensures that the native library is sufficiently loaded for GURL usage.
     *
     * This function is public so that GURL-related usage like the UrlFormatter also counts towards
     * the "Startup.Android.GURLEnsureMainDexInitialized" histogram.
     */
    public static void ensureNativeInitializedForGURL() {
        if (LibraryLoader.getInstance().isInitialized()) return;
        long time = SystemClock.elapsedRealtime();
        LibraryLoader.getInstance().ensureMainDexInitialized();
        // Record metrics only for the UI thread where the delay in loading the library is relevant.
        if (ThreadUtils.runningOnUiThread()) {
            RecordHistogram.recordTimesHistogram("Startup.Android.GURLEnsureMainDexInitialized",
                    SystemClock.elapsedRealtime() - time);
            if (sReportCallback != null && new Random().nextInt(100) < DEBUG_REPORT_PERCENTAGE) {
                final Throwable throwable =
                        new Throwable("This is not a crash, please ignore. See crbug.com/1065377.");
                // This isn't an assert, because by design this is possible, but we would prefer
                // this path does not get hit more than necessary and getting stack traces from the
                // wild will help find issues.
                PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK,
                        () -> { sReportCallback.run(throwable); });
            }
        }
    }

    /** @return true if the GURL is null, empty, or invalid. */
    public static boolean isEmptyOrInvalid(@Nullable GURL gurl) {
        return gurl == null || gurl.isEmpty() || !gurl.isValid();
    }

    @CalledByNative
    private void init(String spec, boolean isValid, Parsed parsed) {
        mSpec = spec;
        // Ensure that the spec only contains US-ASCII or the parsed indices will be wrong.
        assert mSpec.matches("\\A\\p{ASCII}*\\z");
        mIsValid = isValid;
        mParsed = parsed;
    }

    @CalledByNative
    private long toNativeGURL() {
        return GURLJni.get().createNative(mSpec, mIsValid, mParsed.toNativeParsed());
    }

    /**
     * See native GURL::is_valid().
     */
    public boolean isValid() {
        return mIsValid;
    }

    /**
     * See native GURL::spec().
     */
    public String getSpec() {
        if (isValid() || mSpec.isEmpty()) return mSpec;
        assert false : "Trying to get the spec of an invalid URL!";
        return "";
    }

    /**
     * @return Either a valid Spec (see {@link #getSpec}), or an empty string.
     */
    public String getValidSpecOrEmpty() {
        if (isValid()) return mSpec;
        return "";
    }

    /**
     * See native GURL::possibly_invalid_spec().
     */
    public String getPossiblyInvalidSpec() {
        return mSpec;
    }

    private String getComponent(int begin, int length) {
        if (length <= 0) return "";
        return mSpec.substring(begin, begin + length);
    }

    /**
     * See native GURL::scheme().
     */
    public String getScheme() {
        return getComponent(mParsed.mSchemeBegin, mParsed.mSchemeLength);
    }

    /**
     * See native GURL::username().
     */
    public String getUsername() {
        return getComponent(mParsed.mUsernameBegin, mParsed.mUsernameLength);
    }

    /**
     * See native GURL::password().
     */
    public String getPassword() {
        return getComponent(mParsed.mPasswordBegin, mParsed.mPasswordLength);
    }

    /**
     * See native GURL::host().
     */
    public String getHost() {
        return getComponent(mParsed.mHostBegin, mParsed.mHostLength);
    }

    /**
     * See native GURL::port().
     *
     * Note: Do not convert this to an integer yourself. See native GURL::IntPort().
     */
    public String getPort() {
        return getComponent(mParsed.mPortBegin, mParsed.mPortLength);
    }

    /**
     * See native GURL::path().
     */
    public String getPath() {
        return getComponent(mParsed.mPathBegin, mParsed.mPathLength);
    }

    /**
     * See native GURL::query().
     */
    public String getQuery() {
        return getComponent(mParsed.mQueryBegin, mParsed.mQueryLength);
    }

    /**
     * See native GURL::ref().
     */
    public String getRef() {
        return getComponent(mParsed.mRefBegin, mParsed.mRefLength);
    }

    /**
     * @return Whether the GURL is the empty String.
     */
    public boolean isEmpty() {
        return mSpec.isEmpty();
    }

    /**
     * See native GURL::GetOrigin().
     */
    public GURL getOrigin() {
        GURL target = new GURL();
        getOriginInternal(target);
        return target;
    }

    protected void getOriginInternal(GURL target) {
        GURLJni.get().getOrigin(mSpec, mIsValid, mParsed.toNativeParsed(), target);
    }

    @Override
    public final int hashCode() {
        return mSpec.hashCode();
    }

    @Override
    public final boolean equals(Object other) {
        if (other == this) return true;
        if (!(other instanceof GURL)) return false;
        return mSpec.equals(((GURL) other).mSpec);
    }

    /**
     * Serialize a GURL to a String, to be used with {@link GURL#deserialize(String)}.
     *
     * Note that a serialized GURL should only be used internally to Chrome, and should *never* be
     * used if coming from an untrusted source.
     *
     * @return A serialzed GURL.
     */
    public final String serialize() {
        StringBuilder builder = new StringBuilder();
        builder.append(SERIALIZER_VERSION).append(SERIALIZER_DELIMITER);
        builder.append(mIsValid).append(SERIALIZER_DELIMITER);
        builder.append(mParsed.serialize()).append(SERIALIZER_DELIMITER);
        builder.append(mSpec);
        String serialization = builder.toString();
        return Integer.toString(serialization.length()) + SERIALIZER_DELIMITER + serialization;
    }

    /**
     * Deserialize a GURL serialized with {@link GURL#serialize()}.
     *
     * This function should *never* be used on a String coming from an untrusted source.
     *
     * @return The deserialized GURL (or null if the input is empty).
     */
    public static GURL deserialize(@Nullable String gurl) {
        try {
            if (TextUtils.isEmpty(gurl)) return emptyGURL();
            String[] tokens = gurl.split(Character.toString(SERIALIZER_DELIMITER));

            // First token MUST always be the length of the serialized data.
            String length = tokens[0];
            if (gurl.length() != Integer.parseInt(length) + length.length() + 1) {
                throw new IllegalArgumentException("Serialized GURL had the wrong length.");
            }

            // Last token MUST always be the original spec - just re-parse the GURL on version
            // changes.
            String spec = tokens[tokens.length - 1];
            // Special case for empty spec - it won't get its own token.
            if (gurl.endsWith(Character.toString(SERIALIZER_DELIMITER))) spec = "";

            // Second token MUST always be the version number.
            int version = Integer.parseInt(tokens[1]);
            if (version != SERIALIZER_VERSION) return new GURL(spec);

            boolean isValid = Boolean.parseBoolean(tokens[2]);
            Parsed parsed = Parsed.deserialize(tokens, 3);
            GURL result = new GURL();
            result.init(spec, isValid, parsed);
            return result;
        } catch (Exception e) {
            // This is unexpected, maybe the storage got corrupted somehow?
            Log.w(TAG, "Exception while deserializing a GURL: " + gurl, e);
            return emptyGURL();
        }
    }

    @NativeMethods
    interface Natives {
        /**
         * Initializes the provided |target| by parsing the provided |uri|.
         */
        void init(String uri, GURL target);

        /**
         * Reconstructs the native GURL for this Java GURL and initializes |target| with its Origin.
         */
        void getOrigin(String spec, boolean isValid, long nativeParsed, GURL target);

        /**
         * Reconstructs the native GURL for this Java GURL, returning its native pointer.
         */
        long createNative(String spec, boolean isValid, long nativeParsed);
    }
}
