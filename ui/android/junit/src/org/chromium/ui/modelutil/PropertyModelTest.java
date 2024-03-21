// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.hamcrest.Matchers.equalTo;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.ExpectedException;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.BuildConfig;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableTransformingObjectPropertyKey;
import org.chromium.ui.modelutil.PropertyObservable.PropertyObserver;

import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.function.Function;

/** Tests to ensure/validate the interactions with the PropertyModel. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PropertyModelTest {
    @Rule public ExpectedException thrown = ExpectedException.none();

    public static WritableBooleanPropertyKey BOOLEAN_PROPERTY_A = new WritableBooleanPropertyKey();
    public static WritableBooleanPropertyKey BOOLEAN_PROPERTY_B = new WritableBooleanPropertyKey();
    public static WritableBooleanPropertyKey BOOLEAN_PROPERTY_C = new WritableBooleanPropertyKey();

    public static WritableFloatPropertyKey FLOAT_PROPERTY_A = new WritableFloatPropertyKey();
    public static WritableFloatPropertyKey FLOAT_PROPERTY_B = new WritableFloatPropertyKey();
    public static WritableFloatPropertyKey FLOAT_PROPERTY_C = new WritableFloatPropertyKey();

    public static WritableIntPropertyKey INT_PROPERTY_A = new WritableIntPropertyKey();
    public static WritableIntPropertyKey INT_PROPERTY_B = new WritableIntPropertyKey();
    public static WritableIntPropertyKey INT_PROPERTY_C = new WritableIntPropertyKey();

    public static WritableObjectPropertyKey<Object> OBJECT_PROPERTY_A =
            new WritableObjectPropertyKey<>();
    public static WritableObjectPropertyKey<String> OBJECT_PROPERTY_B =
            new WritableObjectPropertyKey<>();
    public static WritableObjectPropertyKey<List<Integer>> OBJECT_PROPERTY_C =
            new WritableObjectPropertyKey<>();
    public static WritableObjectPropertyKey<Object> OBJECT_PROPERTY_SKIP_EQUALITY =
            new WritableObjectPropertyKey<>(true);

    public static WritableTransformingObjectPropertyKey<Object, Object>
            TRANSFORMING_OBJECT_PROPERTY_A = new WritableTransformingObjectPropertyKey<>();
    public static WritableTransformingObjectPropertyKey<String, Object>
            TRANSFORMING_OBJECT_PROPERTY_B = new WritableTransformingObjectPropertyKey<>();
    public static WritableTransformingObjectPropertyKey<List<Integer>, Object>
            TRANSFORMING_OBJECT_PROPERTY_C = new WritableTransformingObjectPropertyKey<>();

    @Test
    public void getAllSetProperties() {
        PropertyModel model =
                new PropertyModel(
                        BOOLEAN_PROPERTY_A, FLOAT_PROPERTY_A, INT_PROPERTY_A, OBJECT_PROPERTY_A);
        model.set(BOOLEAN_PROPERTY_A, true);
        model.set(INT_PROPERTY_A, 42);
        Collection<PropertyKey> setProperties = model.getAllSetProperties();
        assertThat(setProperties, containsInAnyOrder(BOOLEAN_PROPERTY_A, INT_PROPERTY_A));
        assertThat(setProperties.size(), equalTo(2));
    }

    @Test
    public void booleanUpdates() {
        PropertyModel model = new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B);

        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_A, false);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_A, true);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_B, true);
        verifyBooleanUpdate(model, BOOLEAN_PROPERTY_B, false);
    }

    private void verifyBooleanUpdate(
            PropertyModel model, WritableBooleanPropertyKey key, boolean value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.get(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void floatUpdates() {
        PropertyModel model =
                new PropertyModel(FLOAT_PROPERTY_A, FLOAT_PROPERTY_B, FLOAT_PROPERTY_C);

        verifyFloatUpdate(model, FLOAT_PROPERTY_A, 0f);
        verifyFloatUpdate(model, FLOAT_PROPERTY_B, 1f);
        verifyFloatUpdate(model, FLOAT_PROPERTY_C, -1f);

        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.NaN);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.NEGATIVE_INFINITY);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.POSITIVE_INFINITY);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.MIN_VALUE);
        verifyFloatUpdate(model, FLOAT_PROPERTY_A, Float.MAX_VALUE);
    }

    private void verifyFloatUpdate(PropertyModel model, WritableFloatPropertyKey key, float value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.get(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void intUpdates() {
        PropertyModel model = new PropertyModel(INT_PROPERTY_A, INT_PROPERTY_B, INT_PROPERTY_C);

        verifyIntUpdate(model, INT_PROPERTY_A, 0);
        verifyIntUpdate(model, INT_PROPERTY_B, -1);
        verifyIntUpdate(model, INT_PROPERTY_C, 1);

        verifyIntUpdate(model, INT_PROPERTY_A, Integer.MAX_VALUE);
        verifyIntUpdate(model, INT_PROPERTY_A, Integer.MIN_VALUE);
    }

    private void verifyIntUpdate(PropertyModel model, WritableIntPropertyKey key, int value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.get(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void objectUpdates() {
        PropertyModel model =
                new PropertyModel(OBJECT_PROPERTY_A, OBJECT_PROPERTY_B, OBJECT_PROPERTY_C);

        verifyObjectUpdate(model, OBJECT_PROPERTY_A, new Object());
        verifyObjectUpdate(model, OBJECT_PROPERTY_A, null);

        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test");
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test1");
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, null);
        verifyObjectUpdate(model, OBJECT_PROPERTY_B, "Test");

        List<Integer> list = new ArrayList<>();
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
        list = new ArrayList<>(list);
        list.add(1);
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
        list = new ArrayList<>(list);
        list.add(2);
        verifyObjectUpdate(model, OBJECT_PROPERTY_C, list);
    }

    private <T> void verifyObjectUpdate(
            PropertyModel model, WritableObjectPropertyKey<T> key, T value) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.get(key), equalTo(value));

        model.removeObserver(observer);
    }

    @Test
    public void transformingObjectUpdates() {
        Function identityFunction = o -> o;
        Function duplicateStringFunction =
                o -> {
                    if (o == null) return "really_null";
                    return o.toString() + o.toString();
                };
        Function listLengthFunction = o -> ((List) o).size();
        PropertyModel model =
                new PropertyModel.Builder()
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_A, identityFunction)
                        .withTransformingKey(
                                TRANSFORMING_OBJECT_PROPERTY_B, duplicateStringFunction)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_C, listLengthFunction)
                        .build();

        Object obj1 = new Object();
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_A, obj1, obj1);
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_A, null, null);

        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_B, "Test", "TestTest");
        verifyTransformingObjectUpdate(
                model, TRANSFORMING_OBJECT_PROPERTY_B, "Test1", "Test1Test1");
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_B, null, "really_null");
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_B, "Test", "TestTest");

        List<Integer> list = new ArrayList<>();
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_C, list, 0);
        list = new ArrayList<>(list);
        list.add(1);
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_C, list, 1);
        list = new ArrayList<>(list);
        list.add(2);
        verifyTransformingObjectUpdate(model, TRANSFORMING_OBJECT_PROPERTY_C, list, 2);
    }

    private <T, V> void verifyTransformingObjectUpdate(
            PropertyModel model,
            WritableTransformingObjectPropertyKey<T, V> key,
            T value,
            V transformedValue) {
        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(key, value);
        verify(observer).onPropertyChanged(model, key);
        assertThat(model.get(key), equalTo(transformedValue));

        model.removeObserver(observer);
    }

    @Test
    public void duplicateSetChangeSuppression() {
        PropertyModel model =
                new PropertyModel.Builder(
                                BOOLEAN_PROPERTY_A,
                                FLOAT_PROPERTY_A,
                                INT_PROPERTY_A,
                                OBJECT_PROPERTY_A)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_A, o -> o)
                        .build();
        model.set(BOOLEAN_PROPERTY_A, true);
        model.set(FLOAT_PROPERTY_A, 1f);
        model.set(INT_PROPERTY_A, -1);

        Object obj1 = new Object();
        model.set(OBJECT_PROPERTY_A, obj1);

        Object obj2 = new Object();
        model.set(TRANSFORMING_OBJECT_PROPERTY_A, obj2);

        @SuppressWarnings("unchecked")
        PropertyObserver<PropertyKey> observer = Mockito.mock(PropertyObserver.class);
        model.addObserver(observer);
        Mockito.<PropertyObserver>reset(observer);

        model.set(BOOLEAN_PROPERTY_A, true);
        model.set(FLOAT_PROPERTY_A, 1f);
        model.set(INT_PROPERTY_A, -1);
        model.set(OBJECT_PROPERTY_A, obj1);
        model.set(TRANSFORMING_OBJECT_PROPERTY_A, obj2);

        Mockito.verifyNoMoreInteractions(observer);
    }

    @Test
    public void ensureValidKey() {
        if (!BuildConfig.ENABLE_ASSERTS) return;

        PropertyModel model = new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B);
        thrown.expect(IllegalArgumentException.class);
        model.set(BOOLEAN_PROPERTY_C, true);
    }

    @Test(expected = IllegalArgumentException.class)
    public void preventsDuplicateKeys() {
        new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_A);
    }

    @Test
    public void testCompareValue_Boolean() {
        PropertyModel model1 =
                new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B, BOOLEAN_PROPERTY_C);
        model1.set(BOOLEAN_PROPERTY_A, true);
        model1.set(BOOLEAN_PROPERTY_B, true);
        model1.set(BOOLEAN_PROPERTY_C, false);

        PropertyModel model2 =
                new PropertyModel(BOOLEAN_PROPERTY_A, BOOLEAN_PROPERTY_B, BOOLEAN_PROPERTY_C);
        model2.set(BOOLEAN_PROPERTY_A, true);
        model2.set(BOOLEAN_PROPERTY_B, false);

        Assert.assertTrue(
                "BOOLEAN_PROPERTY_A should be equal",
                model1.compareValue(model2, BOOLEAN_PROPERTY_A));
        Assert.assertFalse(
                "BOOLEAN_PROPERTY_B should not be equal",
                model1.compareValue(model2, BOOLEAN_PROPERTY_B));
        Assert.assertFalse(
                "BOOLEAN_PROPERTY_C should not be equal",
                model1.compareValue(model2, BOOLEAN_PROPERTY_C));
    }

    @Test
    public void testCompareValue_Integer() {
        PropertyModel model1 = new PropertyModel(INT_PROPERTY_A, INT_PROPERTY_B, INT_PROPERTY_C);
        model1.set(INT_PROPERTY_A, 1);
        model1.set(INT_PROPERTY_B, 2);
        model1.set(INT_PROPERTY_C, 3);

        PropertyModel model2 = new PropertyModel(INT_PROPERTY_A, INT_PROPERTY_B, INT_PROPERTY_C);
        model2.set(INT_PROPERTY_A, 1);
        model2.set(INT_PROPERTY_B, 3);

        Assert.assertTrue(
                "INT_PROPERTY_A should be equal", model1.compareValue(model2, INT_PROPERTY_A));
        Assert.assertFalse(
                "INT_PROPERTY_B should not be equal", model1.compareValue(model2, INT_PROPERTY_B));
        Assert.assertFalse(
                "INT_PROPERTY_C should not be equal", model1.compareValue(model2, INT_PROPERTY_C));
    }

    @Test
    public void testCompareValue_Float() {
        PropertyModel model1 =
                new PropertyModel(FLOAT_PROPERTY_A, FLOAT_PROPERTY_B, FLOAT_PROPERTY_C);
        model1.set(FLOAT_PROPERTY_A, 1.2f);
        model1.set(FLOAT_PROPERTY_B, 2.2f);
        model1.set(FLOAT_PROPERTY_C, 3.2f);

        PropertyModel model2 =
                new PropertyModel(FLOAT_PROPERTY_A, FLOAT_PROPERTY_B, FLOAT_PROPERTY_C);
        model2.set(FLOAT_PROPERTY_A, 1.2f);
        model2.set(FLOAT_PROPERTY_B, 3.2f);

        Assert.assertTrue(
                "FLOAT_PROPERTY_A should be equal", model1.compareValue(model2, FLOAT_PROPERTY_A));
        Assert.assertFalse(
                "FLOAT_PROPERTY_B should not be equal",
                model1.compareValue(model2, FLOAT_PROPERTY_B));
        Assert.assertFalse(
                "FLOAT_PROPERTY_C should not be equal",
                model1.compareValue(model2, FLOAT_PROPERTY_C));
    }

    @Test
    public void testCompareValue_Object() {
        Object sharedObject = new Object();

        PropertyModel model1 =
                new PropertyModel(OBJECT_PROPERTY_A, OBJECT_PROPERTY_B, OBJECT_PROPERTY_C);
        model1.set(OBJECT_PROPERTY_A, sharedObject);
        model1.set(OBJECT_PROPERTY_B, "Test");
        model1.set(OBJECT_PROPERTY_C, new ArrayList<>());

        PropertyModel model2 =
                new PropertyModel(OBJECT_PROPERTY_A, OBJECT_PROPERTY_B, OBJECT_PROPERTY_C);
        model2.set(OBJECT_PROPERTY_A, sharedObject);
        model2.set(OBJECT_PROPERTY_B, "Test");

        Assert.assertTrue(
                "OBJECT_PROPERTY_A should be equal",
                model1.compareValue(model2, OBJECT_PROPERTY_A));
        Assert.assertTrue(
                "OBJECT_PROPERTY_B should be equal",
                model1.compareValue(model2, OBJECT_PROPERTY_B));
        Assert.assertFalse(
                "OBJECT_PROPERTY_C should not be equal",
                model1.compareValue(model2, OBJECT_PROPERTY_C));

        model2.set(OBJECT_PROPERTY_B, "Test2");
        Assert.assertFalse(
                "OBJECT_PROPERTY_B should not be equal",
                model1.compareValue(model2, OBJECT_PROPERTY_B));
    }

    @Test
    public void testCompareValue_Object_SkipEquality() {
        Object sharedObject = new Object();

        PropertyModel model1 = new PropertyModel(OBJECT_PROPERTY_SKIP_EQUALITY);
        model1.set(OBJECT_PROPERTY_SKIP_EQUALITY, sharedObject);

        PropertyModel model2 = new PropertyModel(OBJECT_PROPERTY_SKIP_EQUALITY);
        model2.set(OBJECT_PROPERTY_SKIP_EQUALITY, sharedObject);

        Assert.assertFalse(
                "OBJECT_PROPERTY_A should not be equal",
                model1.compareValue(model2, OBJECT_PROPERTY_SKIP_EQUALITY));
    }

    @Test
    public void testCompareValue_TransformingObject() {
        Object sharedObject = new Object();

        Function toStringFunction = o -> o.toString();
        PropertyModel model1 =
                new PropertyModel.Builder()
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_A, toStringFunction)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_B, toStringFunction)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_C, toStringFunction)
                        .build();
        model1.set(TRANSFORMING_OBJECT_PROPERTY_A, sharedObject);
        model1.set(TRANSFORMING_OBJECT_PROPERTY_B, "Test");
        model1.set(TRANSFORMING_OBJECT_PROPERTY_C, new ArrayList<>());

        PropertyModel model2 =
                new PropertyModel.Builder()
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_A, toStringFunction)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_B, toStringFunction)
                        .withTransformingKey(TRANSFORMING_OBJECT_PROPERTY_C, toStringFunction)
                        .build();
        model2.set(TRANSFORMING_OBJECT_PROPERTY_A, sharedObject);
        model2.set(TRANSFORMING_OBJECT_PROPERTY_B, "Test");

        Assert.assertTrue(
                "TRANSFORMING_OBJECT_PROPERTY_A should be equal",
                model1.compareValue(model2, TRANSFORMING_OBJECT_PROPERTY_A));
        Assert.assertTrue(
                "TRANSFORMING_OBJECT_PROPERTY_B should be equal",
                model1.compareValue(model2, TRANSFORMING_OBJECT_PROPERTY_B));
        Assert.assertFalse(
                "TRANSFORMING_OBJECT_PROPERTY_C should not be equal",
                model1.compareValue(model2, TRANSFORMING_OBJECT_PROPERTY_C));

        model2.set(TRANSFORMING_OBJECT_PROPERTY_B, "Test2");
        Assert.assertFalse(
                "TRANSFORMING_OBJECT_PROPERTY_B should not be equal",
                model1.compareValue(model2, TRANSFORMING_OBJECT_PROPERTY_B));
    }
}
