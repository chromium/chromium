// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.test.util;

import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.ImageView;

import androidx.annotation.StringDef;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

import com.google.android.material.textfield.TextInputLayout;

import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.rules.TestWatcher;
import org.junit.runner.Description;

import org.chromium.base.CommandLine;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.TestThreadUtils;
import org.chromium.ui.UiUtils;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;
import java.util.concurrent.Callable;

/**
 * A TestRule for creating Render Tests. The comparison is performed using the Skia Gold image
 * diffing service on the host.
 *
 * General usage:
 *
 * <pre>
 * {@code
 *
 * @RunWith(BaseJUnit4ClassRunner.class)
 * public class MyTest extends BlankUiTestActivityTestCase {
 *     @Rule
 *     public RenderTestRule mRenderTestRule = new RenderTestRule.Builder()
 *             // Required. If using ANDROID_RENDER_TESTS_PUBLIC, the Builder can be created with
 *             // the shorthand RenderTestRule.Builder.withPublicCorpus().
 *             .setCorpus(RenderTestRule.Corpus.ANDROID_RENDER_TESTS_PUBLIC)
 *             // Required. If adding a test for the first time for a component, add the string
 *             // value to the Component @StringDef and @interface.
 *             .setBugComponent(RenderTestRule.Component.BLINK_FORMS_COLOR)
 *             // Optional, only necessary once a CL lands that should invalidate previous golden
 *             // images, e.g. a UI rework.
 *             .setRevision(2)
 *             // Optional, only necessary if you want a message to be associated with these
 *             // golden images and shown in the Gold web UI, e.g. the reason why the revision was
 *             // incremented.
 *             .setDescription("Material design rework")
 *             .build();
 *
 *     @Test
 *     // "RenderTest" feature required.
 *     @Feature({"RenderTest"})
 *     public void testViewAppearance() {
 *         // Setup the UI.
 *         ...
 *
 *         // Render the UI Elements.
 *         mRenderTestRule.render(bigWidgetView, "big_widget");
 *         mRenderTestRule.render(smallWidgetView, "small_widget");
 *     }
 * }
 *
 * }
 * </pre>
 *
 */
public class RenderTestRule extends TestWatcher {
    private static final String TAG = "RenderTest";

    private static final String SKIA_GOLD_FOLDER_RELATIVE = "/skia_gold";

    // State for a test class.
    private final String mOutputFolder;

    // State for a test method.
    private String mTestClassName;
    private String mFullTestName;
    private boolean mHasRenderTestFeature;

    /** Parameterized tests have a prefix inserted at the front of the test description. */
    private String mVariantPrefix;

    /** Prefix on the render test images that describes light/dark mode. */
    private String mNightModePrefix;

    private String mSkiaGoldCorpus;
    private int mSkiaGoldRevision;
    private String mSkiaGoldRevisionDescription;
    private boolean mFailOnUnsupportedConfigs;
    private String mBugComponent;

    @StringDef({
        Corpus.ANDROID_RENDER_TESTS_PUBLIC,
        Corpus.ANDROID_RENDER_TESTS_INTERNAL,
        Corpus.ANDROID_VR_RENDER_TESTS
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Corpus {
        // Corpus for general use and public results.
        String ANDROID_RENDER_TESTS_PUBLIC = "android-render-tests";
        // Corpus for general use and internal results.
        String ANDROID_RENDER_TESTS_INTERNAL = "android-render-tests-internal";
        // Corpus for VR (virtual reality) features.
        String ANDROID_VR_RENDER_TESTS = "android-vr-render-tests";
    }

    @StringDef({
        Component.BLINK_CONTACTS,
        Component.BLINK_FORMS_COLOR,
        Component.BLINK_PAYMENTS,
        Component.FREEZE_DRIED_TABS,
        Component.PRIVACY,
        Component.PRIVACY_INCOGNITO,
        Component.SERVICES_SIGN_IN,
        Component.SERVICES_SYNC,
        Component.UI_BROWSER_AUTOFILL,
        Component.UI_BROWSER_BOOKMARKS,
        Component.UI_BROWSER_BUBBLES_PAGE_INFO,
        Component.UI_BROWSER_CONTENT_SUGGESTIONS,
        Component.UI_BROWSER_CONTENT_SUGGESTIONS_FEED,
        Component.UI_BROWSER_CONTENT_SUGGESTIONS_HISTORY,
        Component.UI_BROWSER_FIRST_RUN,
        Component.UI_BROWSER_INCOGNITO,
        Component.UI_BROWSER_INFOBARS,
        Component.UI_BROWSER_MEDIA_PICKER,
        Component.UI_BROWSER_MOBILE,
        Component.UI_BROWSER_MOBILE_APP_MENU,
        Component.UI_BROWSER_MOBILE_CONTEXT_MENU,
        Component.UI_BROWSER_MOBILE_CUSTOM_TABS,
        Component.UI_BROWSER_MOBILE_HUB,
        Component.UI_BROWSER_MOBILE_MESSAGES,
        Component.UI_BROWSER_MOBILE_RECENT_TABS,
        Component.UI_BROWSER_MOBILE_SETTINGS,
        Component.UI_BROWSER_MOBILE_START,
        Component.UI_BROWSER_MOBILE_TAB_GROUPS,
        Component.UI_BROWSER_MOBILE_TAB_SWITCHER,
        Component.UI_BROWSER_MOBILE_TAB_SWITCHER_GRID,
        Component.UI_BROWSER_NAVIGATION_GESTURENAV,
        Component.UI_BROWSER_NEW_TAB_PAGE,
        Component.UI_BROWSER_OMNIBOX,
        Component.UI_BROWSER_PASSWORDS,
        Component.UI_BROWSER_SEARCH_VOICE,
        Component.UI_BROWSER_SHARING,
        Component.UI_BROWSER_SHOPPING,
        Component.UI_BROWSER_SHOPPING_DEALS,
        Component.UI_BROWSER_SHOPPING_MERCHANT_TRUST,
        Component.UI_BROWSER_SHOPPING_PRICE_TRACKING,
        Component.UI_BROWSER_TOOLBAR,
        Component.UI_BROWSER_THUMBNAIL,
        Component.UI_BROWSER_WEB_APP_INSTALLS,
        Component.UI_SETTINGS_PRIVACY
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface Component {
        String BLINK_CONTACTS = "Blink>Contacts";
        String BLINK_FORMS_COLOR = "Blink>Forms>Color";
        String BLINK_PAYMENTS = "Blink>Payments";
        String BLINK_VIEW_TRANSITIONS = "Blink>ViewTransitions";
        String FREEZE_DRIED_TABS = "Internals>FreezeDriedTabs";
        String PRIVACY = "Privacy";
        String PRIVACY_INCOGNITO = "Privacy>Incognito";
        String SERVICES_SIGN_IN = "Services>SignIn";
        String SERVICES_SYNC = "Services>Sync";
        String UI_BROWSER_AUTOFILL = "UI>Browser>Autofill";
        String UI_BROWSER_BOOKMARKS = "UI>Browser>Bookmarks";
        String UI_BROWSER_BUBBLES_PAGE_INFO = "UI>Browser>Bubbles>PageInfo";
        String UI_BROWSER_CONTENT_SUGGESTIONS = "UI>Browser>ContentSuggestions";
        String UI_BROWSER_CONTENT_SUGGESTIONS_FEED = "UI>Browser>ContentSuggestions>Feed";
        String UI_BROWSER_CONTENT_SUGGESTIONS_HISTORY = "UI>Browser>ContentSuggestions>History";
        String UI_BROWSER_FIRST_RUN = "UI>Browser>FirstRun";
        String UI_BROWSER_INCOGNITO = "UI>Browser>Incognito";
        String UI_BROWSER_INFOBARS = "UI>Browser>Infobars";
        String UI_BROWSER_MEDIA_PICKER = "UI>Browser>MediaPicker";
        String UI_BROWSER_MOBILE = "UI>Browser>Mobile";
        String UI_BROWSER_MOBILE_APP_MENU = "UI>Browser>Mobile>AppMenu";
        String UI_BROWSER_MOBILE_CONTEXT_MENU = "UI>Browser>Mobile>ContextMenu";
        String UI_BROWSER_MOBILE_CUSTOM_TABS = "UI>Browser>Mobile>CustomTabs";
        String UI_BROWSER_MOBILE_HUB = "UI>Browser>Mobile>Hub";
        String UI_BROWSER_MOBILE_MESSAGES = "UI>Browser>Mobile>Messages";
        String UI_BROWSER_MOBILE_RECENT_TABS = "UI>Browser>Mobile>RecentTabs";
        String UI_BROWSER_MOBILE_SETTINGS = "UI>Browser>Mobile>Settings";
        String UI_BROWSER_MOBILE_START = "UI>Browser>Mobile>Start";
        String UI_BROWSER_MOBILE_TAB_GROUPS = "UI>Browser>Mobile>TabGroups";
        String UI_BROWSER_MOBILE_TAB_SWITCHER = "UI>Browser>Mobile>TabSwitcher";
        String UI_BROWSER_MOBILE_TAB_SWITCHER_GRID = "UI>Browser>Mobile>TabSwitcher>Grid";
        String UI_BROWSER_NAVIGATION_GESTURENAV = "UI>Browser>Navigation>GestureNav";
        String UI_BROWSER_NEW_TAB_PAGE = "UI>Browser>NewTabPage";
        String UI_BROWSER_OMNIBOX = "UI>Browser>Omnibox";
        String UI_BROWSER_PASSWORDS = "UI>Browser>Passwords";
        String UI_BROWSER_SEARCH_VOICE = "UI>Browser>Search>Voice";
        String UI_BROWSER_SHARING = "UI>Browser>Sharing";
        String UI_BROWSER_SHOPPING = "UI>Browser>Shopping";
        String UI_BROWSER_SHOPPING_DEALS = "UI>Browser>Shopping>Deals";
        String UI_BROWSER_SHOPPING_MERCHANT_TRUST = "UI>Browser>Shopping>MerchantTrust";
        String UI_BROWSER_SHOPPING_PRICE_TRACKING = "UI>Browser>Shopping>PriceTracking";
        String UI_BROWSER_THUMBNAIL = "UI>Browser>Thumbnail";
        String UI_BROWSER_TOOLBAR = "UI>Browser>Toolbar";
        String UI_BROWSER_WEB_APP_INSTALLS = "UI>Browser>WebAppInstalls";
        String UI_SETTINGS_PRIVACY = "UI>Settings>Privacy";
    }

    // Skia Gold-specific constructor used by the builder.
    // Note that each corpus/description combination results in some additional initialization
    // on the host (~250 ms), so consider whether adding unique descriptions is necessary before
    // adding them to a bunch of test classes.
    protected RenderTestRule(
            int revision,
            @Corpus String corpus,
            String description,
            boolean failOnUnsupportedConfigs,
            @Component String component) {
        assert revision >= 0;
        // Don't have a default corpus so that users explicitly specify whether
        // they want their test results to be public or not.
        assert corpus != null;
        assert component != null;

        mSkiaGoldCorpus = corpus;
        mSkiaGoldRevisionDescription = description;
        mSkiaGoldRevision = revision;
        mFailOnUnsupportedConfigs = failOnUnsupportedConfigs;
        mBugComponent = component;

        // The output folder can be overridden with the --render-test-output-dir command.
        mOutputFolder = CommandLine.getInstance().getSwitchValue("render-test-output-dir");
    }

    @Override
    protected void starting(Description desc) {
        // desc.getClassName() gets the fully qualified name.
        mTestClassName = desc.getTestClass().getSimpleName();
        mFullTestName = desc.getClassName() + "#" + desc.getMethodName();

        Feature feature = desc.getAnnotation(Feature.class);
        mHasRenderTestFeature =
                (feature != null && Arrays.asList(feature.value()).contains("RenderTest"));
    }

    /**
     * Renders the |view| and compares it to the golden view with the |id|. Image comparison is
     * performed on the host after the test has finished running. Comparison will fail if the given
     * image does not exactly match one of the images in Gold and the image came from a device that
     * should have baselines maintained (see the RENDER_TEST_MODEL_SDK_CONFIGS constant in the
     * Python test runner code at
     * //build/android/pylib/local/device/local_device_instrumentation_test_run.py).
     *
     * @throws IOException if the rendered image cannot be saved to the device.
     */
    public void render(final View view, String id) throws IOException {
        Assert.assertNotNull(view);
        Assert.assertTrue("Render Tests must have the RenderTest feature.", mHasRenderTestFeature);

        // De-flake by flushing the tasks that are already queued on the Looper's Handler.
        // TODO(crbug.com/40260566): Remove this and properly fix flaky tests.
        TestThreadUtils.flushNonDelayedLooperTasks();
        Bitmap testBitmap =
                ThreadUtils.runOnUiThreadBlocking(
                        new Callable<Bitmap>() {
                            @Override
                            public Bitmap call() {
                                int height = view.getMeasuredHeight();
                                int width = view.getMeasuredWidth();
                                if (height <= 0 || width <= 0) {
                                    throw new IllegalStateException(
                                            "Invalid view dimensions: " + width + "x" + height);
                                }

                                return UiUtils.generateScaledScreenshot(
                                        view, 0, Bitmap.Config.ARGB_8888);
                            }
                        });

        compareForResult(testBitmap, id);
    }

    /**
     * Compares the given |testBitmap| to the images in Gold for |id|. Image comparison is performed
     * on the host after the test has finished running. Comparison will fail if the given image
     * does not exactly match one of the images in Gold and the image came from a device that should
     * have baselines maintained (see the RENDER_TEST_MODEL_SDK_CONFIGS constant in the Python test
     * runner code at //build/android/pylib/local/device/local_device_instrumentation_test_run.py).
     *
     * Tests should prefer {@link RenderTestRule#render(View, String) render} to this if possible.
     *
     * @throws IOException if the rendered image cannot be saved to the device.
     */
    public void compareForResult(Bitmap testBitmap, String id) throws IOException {
        Assert.assertTrue("Render Tests must have the RenderTest feature.", mHasRenderTestFeature);

        // Save the image and its metadata to a location where it can be pulled by the test runner
        // for comparison after the test finishes.
        String imageName = getImageName(mTestClassName, mVariantPrefix, id);
        String jsonName = getJsonName(mTestClassName, mVariantPrefix, id);

        saveBitmap(testBitmap, createOutputPath(SKIA_GOLD_FOLDER_RELATIVE, imageName));
        JSONObject goldKeys = new JSONObject();
        JSONObject optionalKeys = new JSONObject();
        try {
            goldKeys.put("source_type", mSkiaGoldCorpus);
            goldKeys.put("model", Build.MODEL);
            goldKeys.put("sdk_version", String.valueOf(Build.VERSION.SDK_INT));
            if (!TextUtils.isEmpty(mSkiaGoldRevisionDescription)) {
                optionalKeys.put("revision_description", mSkiaGoldRevisionDescription);
            }
            optionalKeys.put(
                    "fail_on_unsupported_configs", String.valueOf(mFailOnUnsupportedConfigs));
            optionalKeys.put("bug_component", mBugComponent);
            // This key will be deleted by the test runner before uploading to Gold. It is used to
            // differentiate results from different tests if the test runner has batched multiple
            // tests together in a single run.
            goldKeys.put("full_test_name", mFullTestName);
            // This key will be deleted by the test runner and its contents passed into Gold as
            // optional key/value pairs. These are purely informational as opposed to indicating
            // something that might have an effect on test output such as device model.
            goldKeys.put("optional_keys", optionalKeys);
        } catch (JSONException e) {
            Assert.fail("Failed to create Skia Gold JSON keys: " + e.toString());
        }
        saveString(goldKeys.toString(), createOutputPath(SKIA_GOLD_FOLDER_RELATIVE, jsonName));
    }

    /**
     * Searches the View hierarchy and modifies the Views to provide better stability in tests. For
     * example it will disable the blinking cursor in EditTexts.
     */
    public static void sanitize(View view) {
        // Add more sanitizations as we discover more flaky attributes.
        if (view instanceof ViewGroup) {
            ViewGroup viewGroup = (ViewGroup) view;
            for (int i = 0; i < viewGroup.getChildCount(); i++) {
                sanitize(viewGroup.getChildAt(i));
            }
        } else if (view instanceof EditText) {
            EditText editText = (EditText) view;
            editText.setCursorVisible(false);
        } else if (view instanceof ImageView) {
            Drawable drawable = ((ImageView) view).getDrawable();
            if (drawable instanceof AnimatedVectorDrawableCompat) {
                ((AnimatedVectorDrawableCompat) drawable).stop();
            }
        }
        if (view instanceof TextInputLayout) {
            TextInputLayout textInputLayout = (TextInputLayout) view;
            textInputLayout.setHintAnimationEnabled(false);
        }
        // Scrollbars fade slowly, making tests flaky due to differences in rendered images.
        view.setVerticalScrollBarEnabled(false);
    }

    /**
     * Sets a string that will be inserted at the start of the description in the golden image name.
     * This is used to create goldens for multiple different variants of the UI.
     */
    public void setVariantPrefix(String variantPrefix) {
        mVariantPrefix = variantPrefix;
    }

    /** Sets a string prefix that describes the light/dark mode in the golden image name. */
    public void setNightModeEnabled(boolean nightModeEnabled) {
        mNightModePrefix = nightModeEnabled ? "NightModeEnabled" : "NightModeDisabled";
    }

    /**
     * Creates an image name combining the image description with details about the device
     * (e.g. current orientation).
     */
    private String getImageName(String testClass, String variantPrefix, String desc) {
        return String.format("%s.png", getFileName(testClass, variantPrefix, desc));
    }

    /**
     * Creates a JSON name combining the description with details about the device (e.g. current
     * orientation).
     */
    private String getJsonName(String testClass, String variantPrefix, String desc) {
        return String.format("%s.json", getFileName(testClass, variantPrefix, desc));
    }

    /**
     * Creates a generic filename (without a file extension) combining the description with details
     * about the device (e.g. current orientation).
     */
    private String getFileName(String testClass, String variantPrefix, String desc) {
        if (!TextUtils.isEmpty(mNightModePrefix)) {
            desc = mNightModePrefix + "-" + desc;
        }

        if (!TextUtils.isEmpty(variantPrefix)) {
            desc = variantPrefix + "-" + desc;
        }

        return String.format("%s.%s.rev_%s", testClass, desc, mSkiaGoldRevision);
    }

    /**
     * Returns a string encoding the device model and sdk. It is used to identify device goldens.
     */
    private static String modelSdkIdentifier() {
        return Build.MODEL.replace(' ', '_') + "-" + Build.VERSION.SDK_INT;
    }

    /** Saves a the given |bitmap| to the |file|. */
    private static void saveBitmap(Bitmap bitmap, File file) throws IOException {
        FileOutputStream out = new FileOutputStream(file);
        try {
            bitmap.compress(Bitmap.CompressFormat.PNG, 100, out);
        } finally {
            out.close();
        }
    }

    /** Saves the given |string| to the |file|. */
    private static void saveString(String string, File file) throws IOException {
        try (PrintWriter out = new PrintWriter(file)) {
            out.println(string);
        }
    }

    /**
     * Convenience method to create a File pointing to |filename| in the |subfolder| in
     * |mOutputFolder|.
     */
    private File createOutputPath(String subfolder, String filename) throws IOException {
        return createPath(mOutputFolder + subfolder, filename);
    }

    private static File createPath(String folder, String filename) throws IOException {
        File path = new File(folder);
        if (!path.exists()) {
            if (!path.mkdirs()) {
                throw new IOException("Could not create " + path.getAbsolutePath());
            }
        }
        return new File(path + "/" + filename);
    }

    /** Base Builder class for creating RenderTestRules and its derivatives. */
    protected abstract static class BaseBuilder<B extends BaseBuilder<B>> {
        protected int mRevision;
        protected @Corpus String mCorpus;
        protected String mDescription;
        protected boolean mFailOnUnsupportedConfigs;
        protected @Component String mBugComponent;

        /**
         * Sets the revision that will be appended to the test name reported to Gold. This should
         * be incremented anytime output changes significantly enough that previous baselines
         * should be considered invalid.
         */
        public B setRevision(int revision) {
            mRevision = revision;
            return self();
        }

        /** Sets the corpus in the Gold instance that images belong to. */
        public B setCorpus(@Corpus String corpus) {
            mCorpus = corpus;
            return self();
        }

        /**
         * Sets the optional description that will be shown alongside the image in the Gold web UI.
         */
        public B setDescription(String description) {
            mDescription = description;
            return self();
        }

        /**
         * Sets whether failures should still be reported on unsupported hardware/software configs.
         * Supported configurations are listed in the Python test runner code in
         * //build/android/pylib/local/device/local_device_instrumentation_test_run.py under the
         * RENDER_TEST_MODEL_SDK_CONFIGS constant.
         */
        public B setFailOnUnsupportedConfigs(boolean fail) {
            mFailOnUnsupportedConfigs = fail;
            return self();
        }

        /** Sets the bug component that will be shown alongside the image in the Gold web UI. */
        public B setBugComponent(@Component String component) {
            mBugComponent = component;
            return self();
        }

        protected B self() {
            return (B) this;
        }

        public abstract RenderTestRule build();
    }

    /** Builder to create a RenderTestRule. */
    public static class Builder extends BaseBuilder<Builder> {
        @Override
        public RenderTestRule build() {
            return new RenderTestRule(
                    mRevision, mCorpus, mDescription, mFailOnUnsupportedConfigs, mBugComponent);
        }

        /** Creates a Builder with the default public corpus. */
        public static Builder withPublicCorpus() {
            return new Builder().setCorpus(Corpus.ANDROID_RENDER_TESTS_PUBLIC);
        }
    }
}
