// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.url;

import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import com.google.errorprone.annotations.DoNotMock;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.BuildConfig;
import org.chromium.url.mojom.Url;
import org.chromium.url.mojom.UrlConstants;

import java.util.Random;

/**
 * An immutable Java wrapper for GURL, Chromium's URL parsing library.
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
@DoNotMock("Create a real instance instead.")
public class GURL {
    private static final String TAG = "GURL";
    /* package */ static final int SERIALIZER_VERSION = 1;
    /* package */ static final char SERIALIZER_DELIMITER = '\0';

    @FunctionalInterface
    public interface ReportDebugThrowableCallback {
        void run(Throwable throwable);
    }

    /**
     * Exception signalling that a GURL failed to parse due to an unexpected version marker in the
     * serialized input.
     */
    public static class BadSerializerVersionException extends RuntimeException {}

    // Right now this is only collecting reports on Canary which has a relatively small population.
    private static final int DEBUG_REPORT_PERCENTAGE = 10;
    private static ReportDebugThrowableCallback sReportCallback;

    // TODO(crbug.com/40113773): Right now we return a new String with each request for a
    //      GURL component other than the spec itself. Should we cache return Strings (as
    //      WeakReference?) so that callers can share String memory?
    private String mSpec;
    private boolean mIsValid;
    private Parsed mParsed;

    private static class Holder {
        private static GURL sEmptyGURL = new GURL("");
    }

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
        getNatives().init(uri, this);
    }

    @CalledByNative
    protected GURL() {}

    /** Enables debug stack trace gathering for GURL. */
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
            // "MainDex" in name of histogram is a dated reference to when we used to have 2
            // sections of the native library, main dex and non-main dex. Maintaining name for
            // consistency in metrics.
            RecordHistogram.recordTimesHistogram(
                    "Startup.Android.GURLEnsureMainDexInitialized",
                    SystemClock.elapsedRealtime() - time);
            if (sReportCallback != null && new Random().nextInt(100) < DEBUG_REPORT_PERCENTAGE) {
                final Throwable throwable =
                        new Throwable("This is not a crash, please ignore. See crbug.com/1065377.");
                // This isn't an assert, because by design this is possible, but we would prefer
                // this path does not get hit more than necessary and getting stack traces from the
                // wild will help find issues.
                PostTask.postTask(
                        TaskTraits.BEST_EFFORT_MAY_BLOCK,
                        () -> {
                            sReportCallback.run(throwable);
                        });
            }
        }
    }

    /** @return true if the GURL is null, empty, or invalid. */
    public static boolean isEmptyOrInvalid(@Nullable GURL gurl) {
        return gurl == null || gurl.isEmpty() || !gurl.isValid();
    }

    @CalledByNative
    private void init(@JniType("std::string") String spec, boolean isValid, Parsed parsed) {
        mSpec = spec;
        mIsValid = isValid;
        mParsed = parsed;
    }

    @CalledByNative
    private void toNativeGURL(long nativeGurl, long nativeParsed) {
        mParsed.initNative(nativeParsed);
        GURLJni.get().initNative(mSpec, mIsValid, nativeGurl, nativeParsed);
    }

    /** See native GURL::is_valid(). */
    public boolean isValid() {
        return mIsValid;
    }

    /** See native GURL::spec(). */
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

    /** See native GURL::possibly_invalid_spec(). */
    public String getPossiblyInvalidSpec() {
        return mSpec;
    }

    private String getComponent(int begin, int length) {
        if (length <= 0) return "";
        return mSpec.substring(begin, begin + length);
    }

    /** See native GURL::scheme(). */
    public String getScheme() {
        return getComponent(mParsed.mSchemeBegin, mParsed.mSchemeLength);
    }

    /** See native GURL::username(). */
    public String getUsername() {
        return getComponent(mParsed.mUsernameBegin, mParsed.mUsernameLength);
    }

    /** See native GURL::password(). */
    public String getPassword() {
        return getComponent(mParsed.mPasswordBegin, mParsed.mPasswordLength);
    }

    /** See native GURL::host(). */
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

    /** See native GURL::path(). */
    public String getPath() {
        return getComponent(mParsed.mPathBegin, mParsed.mPathLength);
    }

    /** See native GURL::query(). */
    public String getQuery() {
        return getComponent(mParsed.mQueryBegin, mParsed.mQueryLength);
    }

    /** See native GURL::ref(). */
    public String getRef() {
        return getComponent(mParsed.mRefBegin, mParsed.mRefLength);
    }

    /**
     * @return Whether the GURL is the empty String.
     */
    public boolean isEmpty() {
        return mSpec.isEmpty();
    }

    /** See native GURL::GetOrigin(). */
    public GURL getOrigin() {
        GURL target = new GURL();
        getOriginInternal(target);
        return target;
    }

    protected void getOriginInternal(GURL target) {
        getNatives().getOrigin(this, target);
    }

    /** See native GURL::DomainIs(). */
    public boolean domainIs(String domain) {
        return getNatives().domainIs(this, domain);
    }

    /**
     * Returns a copy of the URL with components replaced. See native GURL::ReplaceComponents().
     *
     * <p>Rules for replacement: 1. If a `clear*` boolean param is true, the component will be
     * removed from the result. 2. Otherwise if the corresponding string param is non-null, its
     * value will be used to replace the component. 3. If the string is null and the `clear*`
     * boolean is false, the component will not be modified.
     *
     * @param username Username replacement.
     * @param clearUsername True if the result should not contain a username.
     * @param password Password replacement.
     * @param clearPassword True if the result should not contain a password.
     * @return Copy of the URL with replacements applied.
     */
    public GURL replaceComponents(
            String username, boolean clearUsername, String password, boolean clearPassword) {
        GURL result = new GURL();
        getNatives()
                .replaceComponents(this, username, clearUsername, password, clearPassword, result);
        return result;
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
     * Prefer using {#link getSpec} instead. This method override exists only for pretty-printing a
     * GURL object during Java junit tests, such as when {#code Assert.assertEquals(gurl1, gurl2)}
     * throws an error.
     */
    @Override
    public String toString() {
        if (BuildConfig.ENABLE_ASSERTS) {
            // We prefix the spec with "GURL" in case this object is being compared to a non-GURL
            // object (such as a String). In such a case, the objects will not be equal but we want
            // the pretty-printed output to give a hint that this is because the types don't match.
            return "GURL(" + getSpec() + ")";
        }
        return super.toString();
    }

    /**
     * Deserialize a GURL serialized with {@link GURL#serialize()}. This will re-parse in case of
     * version mismatch, which may trigger undesired native loading. {@see
     * deserializeLatestVersionOnly} if you want to fail in case of version mismatch.
     *
     * <p>This function should *never* be used on a String coming from an untrusted source.
     *
     * @return The deserialized GURL (or null if the input is empty).
     */
    public static GURL deserialize(@Nullable String gurl) {
        try {
            return deserializeLatestVersionOnly(gurl);
        } catch (BadSerializerVersionException be) {
            // Just re-parse the GURL on version changes.
            String[] tokens = gurl.split(Character.toString(SERIALIZER_DELIMITER));
            return new GURL(getSpecFromTokens(gurl, tokens));
        } catch (Exception e) {
            // This is unexpected, maybe the storage got corrupted somehow?
            Log.w(TAG, "Exception while deserializing a GURL: " + gurl, e);
            return emptyGURL();
        }
    }

    /**
     * Deserialize a GURL serialized with {@link #serialize()}, throwing {@code
     * BadSerializerException} if the serialized input has a version other than the latest. This
     * function should never be used on a String coming from an untrusted source.
     */
    public static GURL deserializeLatestVersionOnly(@Nullable String gurl) {
        if (TextUtils.isEmpty(gurl)) return emptyGURL();
        String[] tokens = gurl.split(Character.toString(SERIALIZER_DELIMITER));

        // First token MUST always be the length of the serialized data.
        String length = tokens[0];
        if (gurl.length() != Integer.parseInt(length) + length.length() + 1) {
            throw new IllegalArgumentException("Serialized GURL had the wrong length.");
        }

        String spec = getSpecFromTokens(gurl, tokens);
        // Second token MUST always be the version number.
        int version = Integer.parseInt(tokens[1]);
        if (version != SERIALIZER_VERSION) {
            throw new BadSerializerVersionException();
        }

        boolean isValid = Boolean.parseBoolean(tokens[2]);
        Parsed parsed = Parsed.deserialize(tokens, 3);
        GURL result = new GURL();
        result.init(spec, isValid, parsed);
        return result;
    }

    private static String getSpecFromTokens(String gurl, String[] tokens) {
        // Last token MUST always be the original spec.
        // Special case for empty spec - it won't get its own token.
        return gurl.endsWith(Character.toString(SERIALIZER_DELIMITER))
                ? ""
                : tokens[tokens.length - 1];
    }

    /**
     * Returns the instance of {@link Natives}. The Robolectric Shadow intercepts invocations of
     * this method.
     *
     * <p>Unlike {@code GURLJni.TEST_HOOKS.setInstanceForTesting}, shadowing this method doesn't
     * rely on tests correctly cleaning up global state.
     */
    private static Natives getNatives() {
        return GURLJni.get();
    }

    /** Inits this GURL with the internal state of another GURL. */
    /* package */ void initForTesting(GURL gurl) {
        init(gurl.mSpec, gurl.mIsValid, gurl.mParsed);
    }

    /** @return A Mojom representation of this URL. */
    public Url toMojom() {
        Url url = new Url();
        // See url/mojom/url_gurl_mojom_traits.cc.
        url.url =
                TextUtils.isEmpty(getPossiblyInvalidSpec())
                                || getPossiblyInvalidSpec().length() > UrlConstants.MAX_URL_CHARS
                                || !isValid()
                        ? ""
                        : getPossiblyInvalidSpec();
        return url;
    }

    @NativeMethods
    interface Natives {
        /** Initializes the provided |target| by parsing the provided |uri|. */
        void init(@JniType("std::string") String uri, GURL target);

        /**
         * Reconstructs the native GURL for this Java GURL and initializes |target| with its Origin.
         */
        void getOrigin(@JniType("GURL") GURL self, GURL target);

        /** Reconstructs the native GURL for this Java GURL, and calls GURL.DomainIs. */
        boolean domainIs(@JniType("GURL") GURL self, @JniType("std::string") String domain);

        /** Reconstructs the native GURL for this Java GURL, assigning it to nativeGurl. */
        void initNative(
                @JniType("std::string") String spec,
                boolean isValid,
                long nativeGurl,
                long nativeParsed);

        /**
         * Reconstructs the native GURL for this Java GURL and initializes |result| with the result
         * of ReplaceComponents.
         */
        void replaceComponents(
                @JniType("GURL") GURL self,
                String username,
                boolean clearUsername,
                String password,
                boolean clearPassword,
                GURL result);
    }
}
