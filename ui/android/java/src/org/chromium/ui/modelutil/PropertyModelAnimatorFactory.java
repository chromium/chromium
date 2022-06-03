// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.animation.ObjectAnimator;
import android.util.Property;

import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;

/**
 * Static factory class that creates Animators for MVC properties by providing implementations of
 * android.util.Property that mutate a given property in a given model.
 */
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

    private static class PropertyModelFloatProp extends Property<PropertyModel, Float> {
        final WritableFloatPropertyKey mKey;

        public PropertyModelFloatProp(WritableFloatPropertyKey key) {
            super(Float.class, key.toString());
            mKey = key;
        }

        @Override
        public Float get(PropertyModel model) {
            return model.get(mKey);
        }

        @Override
        public void set(PropertyModel model, Float value) {
            model.set(mKey, value);
        }
    }

    // TODO(https://crbug.com/1086676, pnoland): Implement factory methods for other types, e.g. int
    // and aRGB.
}
