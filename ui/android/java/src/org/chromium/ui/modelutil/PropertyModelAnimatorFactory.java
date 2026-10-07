// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.animation.ObjectAnimator;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.util.FloatProperty;
import android.util.IntProperty;

import androidx.annotation.ColorInt;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

/**
 * Static factory class that creates Animators for MVC properties by providing implementations of
 * android.util.Property that mutate a given property in a given model.
 */
@NullMarked
public class PropertyModelAnimatorFactory {
    /**
     * Builds an Animator for the given model, key, and target value.
     * @param model PropertyModel object to write changes to the given key to.
     * @param key Key of the property to change.
     * @param targetValue Target end value of the property.
     * @return An Animator that when run, will animate the property from its current value to the
     *         given target.
     */
    public static ObjectAnimator ofFloat(
            PropertyModel model, WritableFloatPropertyKey key, float targetValue) {
        PropertyModelFloatProp customProperty = new PropertyModelFloatProp(key);
        return ObjectAnimator.ofFloat(model, customProperty, targetValue);
    }

    /**
     * Builds an ARGB Animator for the given model, ColorStateList key, and color values.
     *
     * @param model PropertyModel object to write changes to the given key to.
     * @param key Key of the property to change.
     * @param values A set of color values that the animation will animate between over time. A
     *     single value implies that that value is the target value, and the start value is the
     *     default color of the current ColorStateList in the model (or Color.TRANSPARENT if unset).
     * @return An Animator that when run, will animate the property, setting a ColorStateList value
     *     on each update.
     */
    public static ObjectAnimator ofArgb(
            PropertyModel model,
            WritableObjectPropertyKey<ColorStateList> key,
            @ColorInt int... values) {
        PropertyModelColorStateListProp customProperty = new PropertyModelColorStateListProp(key);
        return ObjectAnimator.ofArgb(model, customProperty, values);
    }

    private static class PropertyModelFloatProp extends FloatProperty<PropertyModel> {
        final WritableFloatPropertyKey mKey;

        public PropertyModelFloatProp(WritableFloatPropertyKey key) {
            super(key.toString());
            mKey = key;
        }

        @Override
        public Float get(PropertyModel model) {
            return model.get(mKey);
        }

        @Override
        public void setValue(PropertyModel model, float value) {
            model.set(mKey, value);
        }
    }

    private static class PropertyModelColorStateListProp extends IntProperty<PropertyModel> {
        final WritableObjectPropertyKey<ColorStateList> mKey;

        public PropertyModelColorStateListProp(WritableObjectPropertyKey<ColorStateList> key) {
            super(key.toString());
            mKey = key;
        }

        @Override
        public Integer get(PropertyModel model) {
            ColorStateList csl = model.get(mKey);
            return csl == null ? Color.TRANSPARENT : csl.getDefaultColor();
        }

        @Override
        public void setValue(PropertyModel model, int value) {
            model.set(mKey, ColorStateList.valueOf(value));
        }
    }

    // TODO(https://crbug.com/1086676, pnoland): Implement factory methods for other types, e.g.
    // int.
}
