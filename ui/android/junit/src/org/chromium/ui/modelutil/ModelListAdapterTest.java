// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.text.TextUtils;
import android.view.View;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.ui.R;

import java.util.concurrent.TimeoutException;

/**
 * Tests to ensure/validate ModelListAdapter behavior.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ModelListAdapterTest {
    private static final Integer VIEW_TYPE_1 = 0;
    private static final Integer VIEW_TYPE_2 = 1;

    private static final PropertyModel.WritableBooleanPropertyKey BOOLEAN_PROPERTY =
            new PropertyModel.WritableBooleanPropertyKey();
    private static final PropertyModel.WritableFloatPropertyKey FLOAT_PROPERTY =
            new PropertyModel.WritableFloatPropertyKey();
    private static final PropertyModel.WritableIntPropertyKey INT_PROPERTY =
            new PropertyModel.WritableIntPropertyKey();
    private static final PropertyModel.WritableObjectPropertyKey OBJECT_PROPERTY =
            new PropertyModel.WritableObjectPropertyKey();
    private static final PropertyModel.ReadableBooleanPropertyKey READONLY_BOOLEAN_PROPERTY =
            new PropertyModel.ReadableBooleanPropertyKey();

    private class TestViewBinder implements PropertyModelChangeProcessor.ViewBinder {
        @Override
        public void bind(Object model, Object view, Object propertyKey) {
            if (propertyKey.equals(BOOLEAN_PROPERTY)) {
                mBindBooleanCallbackHelper.notifyCalled();
            } else if (propertyKey.equals(FLOAT_PROPERTY)) {
                mBindFloatCallbackHelper.notifyCalled();
            } else if (propertyKey.equals(INT_PROPERTY)) {
                mBindIntCallbackHelper.notifyCalled();
            } else if (propertyKey.equals(OBJECT_PROPERTY)) {
                mBindObjectCallbackHelper.notifyCalled();
            }
        }
    }

    private class TestViewBuilder implements ModelListAdapter.ViewBuilder<View> {
        @Override
        public View buildView() {
            return new View(RuntimeEnvironment.application);
        }
    }

    private class TestObject {
        private String mId;
        public TestObject(String id) {
            mId = id;
        }

        @Override
        public boolean equals(Object other) {
            return other == this
                    || (other != null && other instanceof TestObject
                            && TextUtils.equals(((TestObject) other).mId, mId));
        }
    }

    private PropertyModel mModel;
    private ModelListAdapter mModelListAdapter;
    private final CallbackHelper mBindBooleanCallbackHelper = new CallbackHelper();
    private final CallbackHelper mBindFloatCallbackHelper = new CallbackHelper();
    private final CallbackHelper mBindIntCallbackHelper = new CallbackHelper();
    private final CallbackHelper mBindObjectCallbackHelper = new CallbackHelper();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        ModelListAdapter.ModelList testData = new ModelListAdapter.ModelList();
        mModel = new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);
        testData.add(new ModelListAdapter.ListItem(VIEW_TYPE_1, mModel));

        mModelListAdapter = new ModelListAdapter(testData);
        mModelListAdapter.registerType(VIEW_TYPE_1, new TestViewBuilder(), new TestViewBinder());
        mModelListAdapter.registerType(VIEW_TYPE_2, new TestViewBuilder(), new TestViewBinder());
    }

    @Test
    public void testNullConvertView() throws TimeoutException {
        // Set a property to test that it gets bound.
        mModel.set(BOOLEAN_PROPERTY, true);

        View view = mModelListAdapter.getView(0, null, null);

        mBindBooleanCallbackHelper.waitForCallback(0);

        Assert.assertEquals("Incorrect view type", VIEW_TYPE_1, view.getTag(R.id.view_type));
        Assert.assertEquals("Incorrect model", mModel, view.getTag(R.id.view_model));
        Assert.assertNotNull("MCP not set", view.getTag(R.id.view_mcp));

        // Check that the MCP is hooked up.
        mModel.set(BOOLEAN_PROPERTY, false);
        mBindBooleanCallbackHelper.waitForCallback(1);
    }

    @Test
    public void testNullTypeConvertView() throws TimeoutException {
        // Set a property to test that it gets bound.
        mModel.set(BOOLEAN_PROPERTY, true);

        View nullTypeView = new View(RuntimeEnvironment.application);
        View view = mModelListAdapter.getView(0, nullTypeView, null);

        mBindBooleanCallbackHelper.waitForCallback(0);
        Assert.assertEquals("Incorrect view type", VIEW_TYPE_1, view.getTag(R.id.view_type));
        Assert.assertNotEquals("nullTypeView incorrectly reused", nullTypeView, view);
    }

    @Test
    public void testSameTypeConvertView_SameProperties() throws TimeoutException {
        // Construct a test model for the convertView.
        PropertyModel convertViewModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        // Set a property to the same value for both models.
        convertViewModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(BOOLEAN_PROPERTY, true);

        // Set up a convertView with the same type, the test model, and a test MCP.
        View convertView = new View(RuntimeEnvironment.application);
        PropertyModelChangeProcessor convertViewMcp =
                Mockito.spy(PropertyModelChangeProcessor.create(
                        convertViewModel, convertView, new TestViewBinder(), false));
        convertView.setTag(R.id.view_type, VIEW_TYPE_1);
        convertView.setTag(R.id.view_model, convertViewModel);
        convertView.setTag(R.id.view_mcp, convertViewMcp);

        View view = mModelListAdapter.getView(0, convertView, null);

        Assert.assertEquals("Incorrect callback count for boolean property", 0,
                mBindBooleanCallbackHelper.getCallCount());
        Assert.assertEquals("convertView not reused", convertView, view);
        Assert.assertEquals("Incorrect view type", VIEW_TYPE_1, view.getTag(R.id.view_type));
        Assert.assertEquals("Incorrect model", mModel, view.getTag(R.id.view_model));
        Assert.assertNotNull("MCP not set", view.getTag(R.id.view_mcp));
        Assert.assertNotEquals(
                "MCP not properly switched", convertViewMcp, view.getTag(R.id.view_mcp));
        Mockito.verify(convertViewMcp).destroy();

        // Set a property on the new view's model and assert the ViewBinder is invoked.
        mModel.set(BOOLEAN_PROPERTY, false);
        mBindBooleanCallbackHelper.waitForCallback(0);
        Assert.assertEquals("Incorrect callback count for boolean property", 1,
                mBindBooleanCallbackHelper.getCallCount());

        // Set a property on the convertView's model and assert the ViewBinder is not invoked.
        convertViewModel.set(BOOLEAN_PROPERTY, true);
        Assert.assertEquals("Incorrect callback count for boolean property", 1,
                mBindBooleanCallbackHelper.getCallCount());
    }

    @Test
    public void testSameTypeConvertView_DifferentProperties() throws TimeoutException {
        // Construct a test model for the convertView.
        PropertyModel convertViewModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        // Set a property to a different value for each model.
        convertViewModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(BOOLEAN_PROPERTY, false);

        View convertView = new View(RuntimeEnvironment.application);
        convertView.setTag(R.id.view_type, VIEW_TYPE_1);
        convertView.setTag(R.id.view_model, convertViewModel);

        View view = mModelListAdapter.getView(0, convertView, null);

        mBindBooleanCallbackHelper.waitForCallback(0);
        Assert.assertEquals("convertView not reused", convertView, view);
    }

    @Test
    public void testDifferentTypeConvertView() throws TimeoutException {
        // Construct a test model for the convertView.
        PropertyModel convertViewModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        // Set a property to the same value for each model.
        convertViewModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(BOOLEAN_PROPERTY, true);

        View convertView = new View(RuntimeEnvironment.application);
        convertView.setTag(R.id.view_type, VIEW_TYPE_2);
        convertView.setTag(R.id.view_model, convertViewModel);

        View view = mModelListAdapter.getView(0, convertView, null);

        mBindBooleanCallbackHelper.waitForCallback(0);
        Assert.assertEquals("Incorrect view type", VIEW_TYPE_1, view.getTag(R.id.view_type));
        Assert.assertNotEquals("convertView incorrectly reused", convertView, view);
    }

    @Test
    public void testBindNewModel_NullOldModel_SetPropertyValues() throws TimeoutException {
        mModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(FLOAT_PROPERTY, 1.2f);
        mModel.set(INT_PROPERTY, 3);
        mModel.set(OBJECT_PROPERTY, new TestObject("Test"));

        ModelListAdapter.bindNewModel(
                mModel, null, new View(RuntimeEnvironment.application), new TestViewBinder());

        mBindBooleanCallbackHelper.waitForCallback(0);
        mBindFloatCallbackHelper.waitForCallback(0);
        mBindIntCallbackHelper.waitForCallback(0);
        mBindObjectCallbackHelper.waitForCallback(0);
    }

    @Test
    public void testBindNewModel_NullOldModel_UnsetPropertyValues() {
        ModelListAdapter.bindNewModel(
                mModel, null, new View(RuntimeEnvironment.application), new TestViewBinder());

        Assert.assertEquals("Incorrect callback count for boolean property", 0,
                mBindBooleanCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for float property", 0,
                mBindFloatCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for int property", 0,
                mBindIntCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for object property", 0,
                mBindObjectCallbackHelper.getCallCount());
    }

    @Test
    public void testBindNewModel_NonNullOldModel_SamePropertyValues() {
        PropertyModel oldModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        oldModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(BOOLEAN_PROPERTY, true);
        oldModel.set(FLOAT_PROPERTY, 1.2f);
        mModel.set(FLOAT_PROPERTY, 1.2f);
        oldModel.set(INT_PROPERTY, 3);
        mModel.set(INT_PROPERTY, 3);
        oldModel.set(OBJECT_PROPERTY, new TestObject("Test"));
        mModel.set(OBJECT_PROPERTY, new TestObject("Test"));

        ModelListAdapter.bindNewModel(
                mModel, oldModel, new View(RuntimeEnvironment.application), new TestViewBinder());

        Assert.assertEquals("Incorrect callback count for boolean property", 0,
                mBindBooleanCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for float property", 0,
                mBindFloatCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for int property", 0,
                mBindIntCallbackHelper.getCallCount());
        Assert.assertEquals("Incorrect callback count for object property", 0,
                mBindObjectCallbackHelper.getCallCount());
    }

    @Test
    public void testBindNewModel_NonNullOldModel_DifferentPropertyValues() throws TimeoutException {
        PropertyModel oldModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        oldModel.set(BOOLEAN_PROPERTY, true);
        mModel.set(BOOLEAN_PROPERTY, false);
        oldModel.set(FLOAT_PROPERTY, 1.2f);
        mModel.set(FLOAT_PROPERTY, 1.21f);
        oldModel.set(INT_PROPERTY, 3);
        mModel.set(INT_PROPERTY, 4);
        oldModel.set(OBJECT_PROPERTY, new TestObject("Test"));
        mModel.set(OBJECT_PROPERTY, new TestObject("Test1"));

        ModelListAdapter.bindNewModel(
                mModel, oldModel, new View(RuntimeEnvironment.application), new TestViewBinder());

        mBindBooleanCallbackHelper.waitForCallback(0);
        mBindFloatCallbackHelper.waitForCallback(0);
        mBindIntCallbackHelper.waitForCallback(0);
        mBindObjectCallbackHelper.waitForCallback(0);
    }

    @Test
    public void testBindNewModel_NonNullOldModel_UnsetPropertyValues() throws TimeoutException {
        PropertyModel oldModel =
                new PropertyModel(BOOLEAN_PROPERTY, FLOAT_PROPERTY, INT_PROPERTY, OBJECT_PROPERTY);

        oldModel.set(BOOLEAN_PROPERTY, true);
        oldModel.set(FLOAT_PROPERTY, 1.2f);
        oldModel.set(INT_PROPERTY, 3);
        oldModel.set(OBJECT_PROPERTY, new TestObject("Test"));

        ModelListAdapter.bindNewModel(
                mModel, oldModel, new View(RuntimeEnvironment.application), new TestViewBinder());

        mBindBooleanCallbackHelper.waitForCallback(0);
        mBindFloatCallbackHelper.waitForCallback(0);
        mBindIntCallbackHelper.waitForCallback(0);
        mBindObjectCallbackHelper.waitForCallback(0);
    }

    @Test(expected = AssertionError.class)
    public void testBindNewModel_RewriteReadOnlyProperty() {
        PropertyModel oldModel = new PropertyModel.Builder(READONLY_BOOLEAN_PROPERTY)
                                         .with(READONLY_BOOLEAN_PROPERTY, false)
                                         .build();
        PropertyModel newModel = new PropertyModel.Builder(READONLY_BOOLEAN_PROPERTY)
                                         .with(READONLY_BOOLEAN_PROPERTY, true)
                                         .build();
        ModelListAdapter.bindNewModel(
                newModel, oldModel, new View(RuntimeEnvironment.application), new TestViewBinder());
    }
}
