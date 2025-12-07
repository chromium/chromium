// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.theme;

import android.content.Context;
import android.content.ContextWrapper;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.theme.ThemeResourceWrapper.ThemeObserver;

/** Unit tests for {@link ThemeResourceWrapperProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ThemeResourceWrapperProviderUnitTest {

    private Context mBaseContext;

    @Before
    public void setup() {
        mBaseContext = ApplicationProvider.getApplicationContext();
    }

    @Test
    public void testGetFromContext_direct() {
        TestThemeResourceWrapperProvider context =
                new TestThemeResourceWrapperProvider(mBaseContext, true);
        Assert.assertEquals(
                "Should return the context itself.",
                context,
                ThemeResourceWrapperProvider.getFromContext(context));
    }

    @Test
    public void testGetFromContext_wrapped() {
        TestThemeResourceWrapperProvider innerContext =
                new TestThemeResourceWrapperProvider(mBaseContext, true);
        ContextWrapper outerContext = new ContextWrapper(innerContext);
        Assert.assertEquals(
                "Should return the inner context.",
                innerContext,
                ThemeResourceWrapperProvider.getFromContext(outerContext));
    }

    @Test
    public void testGetFromContext_doubleWrapped() {
        TestThemeResourceWrapperProvider innerContext =
                new TestThemeResourceWrapperProvider(mBaseContext, true);
        ContextWrapper middleContext = new ContextWrapper(innerContext);
        ContextWrapper outerContext = new ContextWrapper(middleContext);
        Assert.assertEquals(
                "Should return the inner context.",
                innerContext,
                ThemeResourceWrapperProvider.getFromContext(outerContext));
    }

    @Test
    public void testGetFromContext_notFound() {
        ContextWrapper context = new ContextWrapper(mBaseContext);
        Assert.assertNull(
                "Should return null when not found.",
                ThemeResourceWrapperProvider.getFromContext(context));
    }

    @Test
    public void testGetFromContext_baseContextOnly() {
        Assert.assertNull(
                "Should return null for a base context.",
                ThemeResourceWrapperProvider.getFromContext(mBaseContext));
    }

    @Test
    public void testAttachAndDetach() {
        TestThemeResourceWrapperProvider context =
                new TestThemeResourceWrapperProvider(mBaseContext, true);
        ThemeObserver observer = Mockito.mock(ThemeObserver.class);

        Assert.assertNull(context.mAttachedObserver);
        context.attachThemeObserver(observer);
        Assert.assertEquals(observer, context.mAttachedObserver);

        context.detachThemeObserver(observer);
        Assert.assertNull(context.mAttachedObserver);
    }

    @Test
    public void testHasThemeResourceWrapper() {
        TestThemeResourceWrapperProvider contextWithChangeable =
                new TestThemeResourceWrapperProvider(mBaseContext, true);
        Assert.assertTrue("Should be changeable.", contextWithChangeable.hasThemeResourceWrapper());

        TestThemeResourceWrapperProvider contextWithoutChangeable =
                new TestThemeResourceWrapperProvider(mBaseContext, false);
        Assert.assertFalse(
                "Should not be changeable.", contextWithoutChangeable.hasThemeResourceWrapper());
    }

    // Test only ThemeResourceWrapperProvider to demonstrate the usage.
    private static class TestThemeResourceWrapperProvider extends ContextWrapper
            implements ThemeResourceWrapperProvider {
        private final boolean mIsChangeable;
        public ThemeObserver mAttachedObserver;

        public TestThemeResourceWrapperProvider(Context base, boolean isChangeable) {
            super(base);
            mIsChangeable = isChangeable;
        }

        @Override
        public boolean hasThemeResourceWrapper() {
            return mIsChangeable;
        }

        @Override
        public void attachThemeObserver(ThemeObserver observer) {
            mAttachedObserver = observer;
        }

        @Override
        public void detachThemeObserver(ThemeObserver observer) {
            if (mAttachedObserver == observer) {
                mAttachedObserver = null;
            }
        }
    }
}
