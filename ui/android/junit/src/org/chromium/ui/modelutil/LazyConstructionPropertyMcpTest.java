// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.hamcrest.Matchers.is;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.view.View;

import androidx.annotation.Nullable;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;
import org.chromium.ui.test.util.modelutil.FakeViewProvider;

/** Unit tests for LazyConstructionPropertyMcp. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class LazyConstructionPropertyMcpTest {
    private static final WritableBooleanPropertyKey VISIBILITY = new WritableBooleanPropertyKey();
    private static final WritableObjectPropertyKey<String> STRING_PROPERTY =
            new WritableObjectPropertyKey<>();
    private static final WritableIntPropertyKey INT_PROPERTY = new WritableIntPropertyKey();
    private static final PropertyKey[] ALL_PROPERTIES =
            new PropertyKey[] {VISIBILITY, STRING_PROPERTY, INT_PROPERTY};
    private PropertyModel mModel;
    private FakeViewProvider<View> mViewProvider;
    private @Nullable PropertyObservable.PropertyObserver<PropertyKey> mModelObserver;

    @Mock private View mView;
    @Mock private ViewBinder<PropertyModel, View, PropertyKey> mViewBinder;

    @Before
    public void setup() {
        MockitoAnnotations.initMocks(this);
        mModel = new PropertyModel(ALL_PROPERTIES);
        mModel.set(VISIBILITY, false);
        mViewProvider = new FakeViewProvider<>();
        mModel.addObserver(
                (source, propertyKey) -> {
                    // Forward model changes to the model observer if it exists. It's important for
                    // the test that the observer is notified before the
                    // LazyConstructionPropertyMcp.
                    if (mModelObserver != null) {
                        mModelObserver.onPropertyChanged(source, propertyKey);
                    }
                });
    }

    @Test
    public void testInitialConstruction() {
        LazyConstructionPropertyMcp.create(mModel, VISIBILITY, mViewProvider, mViewBinder);
        assertFalse(mViewProvider.inflationHasStarted());

        mModel.set(VISIBILITY, true);

        assertTrue(mViewProvider.inflationHasStarted());
        mViewProvider.finishInflation(mView);
        ShadowLooper.idleMainLooper();

        verifyBind(VISIBILITY);
    }

    @Test
    public void testUpdatesBeforeInflation() {
        LazyConstructionPropertyMcp.create(mModel, VISIBILITY, mViewProvider, mViewBinder);
        mModel.set(STRING_PROPERTY, "foo");
        mModel.set(VISIBILITY, true);
        assertTrue(mViewProvider.inflationHasStarted());
        mViewProvider.finishInflation(mView);
        ShadowLooper.idleMainLooper();
        verifyBind(STRING_PROPERTY, VISIBILITY);
    }

    @Test
    public void testUpdatesWhileVisible() {
        LazyConstructionPropertyMcp.create(mModel, VISIBILITY, mViewProvider, mViewBinder);

        // Show the view and pump the looper to do the initial bind.
        mModel.set(VISIBILITY, true);
        assertTrue(mViewProvider.inflationHasStarted());
        mViewProvider.finishInflation(mView);
        ShadowLooper.idleMainLooper();
        verifyBind(VISIBILITY);
        Mockito.<ViewBinder>reset(mViewBinder);

        mModel.set(INT_PROPERTY, 42);
        verifyBind(INT_PROPERTY);

        mModel.set(VISIBILITY, false);
        verifyBind(VISIBILITY);
    }

    @Test
    public void testUpdatesWhileHidden() {
        LazyConstructionPropertyMcp.create(mModel, VISIBILITY, mViewProvider, mViewBinder);

        // Show the view and pump the looper to do the initial bind, then hide the view again.
        mModel.set(VISIBILITY, true);
        assertTrue(mViewProvider.inflationHasStarted());
        mViewProvider.finishInflation(mView);
        ShadowLooper.idleMainLooper();
        verifyBind(VISIBILITY);

        mModel.set(VISIBILITY, false);
        verify(mViewBinder, times(2)).bind(eq(mModel), eq(mView), eq(VISIBILITY));
        Mockito.<ViewBinder>reset(mViewBinder);

        // While the view is hidden, the binder should not be invoked.
        mModel.set(STRING_PROPERTY, "foo");
        mModel.set(STRING_PROPERTY, "bar");
        verify(mViewBinder, never())
                .bind(any(PropertyModel.class), any(View.class), any(PropertyKey.class));

        // When the view is shown, all pending updates should be dispatched, coalescing updates to
        // the same property.
        mModel.set(VISIBILITY, true);
        verifyBind(VISIBILITY, STRING_PROPERTY);
    }

    @Test
    public void testReentrantUpdates() {
        mModel.set(INT_PROPERTY, 0);
        LazyConstructionPropertyMcp.create(mModel, VISIBILITY, mViewProvider, mViewBinder);

        // Increase INT_PROPERTY any time visibility changes.
        mModelObserver =
                (source, propertyKey) -> {
                    if (propertyKey != VISIBILITY) return;
                    mModel.set(INT_PROPERTY, mModel.get(INT_PROPERTY) + 1);
                };

        mModel.set(VISIBILITY, true);
        mViewProvider.finishInflation(mView);
        ShadowLooper.idleMainLooper();
        verifyBind(VISIBILITY, INT_PROPERTY);
        assertThat(mModel.get(INT_PROPERTY), is(1));
        Mockito.<ViewBinder>reset(mViewBinder);

        mModel.set(VISIBILITY, false);
        verifyBind(INT_PROPERTY, VISIBILITY);
        assertThat(mModel.get(INT_PROPERTY), is(2));
    }

    private void verifyBind(PropertyKey... properties) {
        for (PropertyKey key : properties) {
            verify(mViewBinder).bind(mModel, mView, key);
        }
    }
}
