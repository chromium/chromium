// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.support.v4.util.ObjectsCompat;
import android.support.v7.content.res.AppCompatResources;

import androidx.annotation.DrawableRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;

import org.chromium.base.annotations.RemovableInRelease;

import java.util.ArrayList;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * Generic property model that aims to provide an extensible and efficient model for ease of use.
 */
public class PropertyModel extends PropertyObservable<PropertyKey> {
    /**
     * A PropertyKey implementation that associates a name with the property for easy debugging.
     */
    private static class NamedPropertyKey implements PropertyKey {
        private final String mPropertyName;

        public NamedPropertyKey(@Nullable String propertyName) {
            mPropertyName = propertyName;
        }

        @Override
        public String toString() {
            if (mPropertyName == null) return super.toString();
            return mPropertyName;
        }
    }

    /** The key type for read-ony boolean model properties. */
    public static class ReadableBooleanPropertyKey extends NamedPropertyKey {
        /**
         * Constructs a new unnamed read-only boolean property key.
         */
        public ReadableBooleanPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named read-only boolean property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public ReadableBooleanPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for mutable boolean model properties. */
    public final static class WritableBooleanPropertyKey extends ReadableBooleanPropertyKey {
        /**
         * Constructs a new unnamed writable boolean property key.
         */
        public WritableBooleanPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named writable boolean property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public WritableBooleanPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for read-only float model properties. */
    public static class ReadableFloatPropertyKey extends NamedPropertyKey {
        /**
         * Constructs a new unnamed read-only float property key.
         */
        public ReadableFloatPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named read-only float property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public ReadableFloatPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for mutable float model properties. */
    public final static class WritableFloatPropertyKey extends ReadableFloatPropertyKey {
        /**
         * Constructs a new unnamed writable float property key.
         */
        public WritableFloatPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named writable float property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public WritableFloatPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for read-only int model properties. */
    public static class ReadableIntPropertyKey extends NamedPropertyKey {
        /**
         * Constructs a new unnamed read-only integer property key.
         */
        public ReadableIntPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named read-only integer property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public ReadableIntPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for mutable int model properties. */
    public final static class WritableIntPropertyKey extends ReadableIntPropertyKey {
        /**
         * Constructs a new unnamed writable integer property key.
         */
        public WritableIntPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named writable integer property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public WritableIntPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /**
     * The key type for read-only Object model properties.
     *
     * @param <T> The type of the Object being tracked by the key.
     */
    public static class ReadableObjectPropertyKey<T> extends NamedPropertyKey {
        /**
         * Constructs a new unnamed read-only object property key.
         */
        public ReadableObjectPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named read-only object property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public ReadableObjectPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /**
     * The key type for mutable Object model properties.
     *
     * @param <T> The type of the Object being tracked by the key.
     */
    public final static class WritableObjectPropertyKey<T> extends ReadableObjectPropertyKey<T> {
        private final boolean mSkipEquality;

        /** Default constructor for an unnamed writable object property. */
        public WritableObjectPropertyKey() {
            this(false);
        }

        /**
         * Constructs a new unnamed writable object property.
         * @param skipEquality Whether the equality check should be bypassed for this key.
         */
        public WritableObjectPropertyKey(boolean skipEquality) {
            this(skipEquality, null);
        }

        /**
         * Constructs a new named writable object property key bypassing equality checks.
         * @param name The optional name of the property.
         */
        public WritableObjectPropertyKey(@Nullable String name) {
            this(false, name);
        }

        /**
         * Constructs a new writable, named object property.
         * @param skipEquality Whether the equality check should be bypassed for this key.
         * @param name Name of the property -- used while debugging.
         */
        public WritableObjectPropertyKey(boolean skipEquality, @Nullable String name) {
            super(name);
            mSkipEquality = skipEquality;
        }
    }

    private final Map<PropertyKey, ValueContainer> mData;

    /**
     * Constructs a model for the given list of keys.
     *
     * @param keys The key types supported by this model.
     */
    public PropertyModel(PropertyKey... keys) {
        this(buildData(keys));
    }

    /**
     * Constructs a model with a generic collection of existing keys.
     *
     * @param keys The key types supported by this model.
     */
    public PropertyModel(Collection<PropertyKey> keys) {
        this(buildData(keys.toArray(new PropertyKey[keys.size()])));
    }

    private PropertyModel(Map<PropertyKey, ValueContainer> startingValues) {
        mData = startingValues;
    }

    @RemovableInRelease
    private void validateKey(PropertyKey key) {
        if (!mData.containsKey(key)) {
            throw new IllegalArgumentException("Invalid key passed in: " + key);
        }
    }

    /**
     * Get the current value from the float based key.
     */
    public float get(ReadableFloatPropertyKey key) {
        validateKey(key);
        FloatContainer container = (FloatContainer) mData.get(key);
        return container == null ? 0f : container.value;
    }

    /**
     * Set the value for the float based key.
     */
    public void set(WritableFloatPropertyKey key, float value) {
        validateKey(key);
        FloatContainer container = (FloatContainer) mData.get(key);
        if (container == null) {
            container = new FloatContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }

        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the int based key.
     */
    public int get(ReadableIntPropertyKey key) {
        validateKey(key);
        IntContainer container = (IntContainer) mData.get(key);
        return container == null ? 0 : container.value;
    }

    /**
     * Set the value for the int based key.
     */
    public void set(WritableIntPropertyKey key, int value) {
        validateKey(key);
        IntContainer container = (IntContainer) mData.get(key);
        if (container == null) {
            container = new IntContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }

        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the boolean based key.
     */
    public boolean get(ReadableBooleanPropertyKey key) {
        validateKey(key);
        BooleanContainer container = (BooleanContainer) mData.get(key);
        return container == null ? false : container.value;
    }

    /**
     * Set the value for the boolean based key.
     */
    public void set(WritableBooleanPropertyKey key, boolean value) {
        validateKey(key);
        BooleanContainer container = (BooleanContainer) mData.get(key);
        if (container == null) {
            container = new BooleanContainer();
            mData.put(key, container);
        } else if (container.value == value) {
            return;
        }

        container.value = value;
        notifyPropertyChanged(key);
    }

    /**
     * Get the current value from the object based key.
     */
    @SuppressWarnings("unchecked")
    public <T> T get(ReadableObjectPropertyKey<T> key) {
        validateKey(key);
        ObjectContainer<T> container = (ObjectContainer<T>) mData.get(key);
        return container == null ? null : container.value;
    }

    /**
     * Set the value for the Object based key.
     */
    @SuppressWarnings("unchecked")
    public <T> void set(WritableObjectPropertyKey<T> key, T value) {
        validateKey(key);
        ObjectContainer<T> container = (ObjectContainer<T>) mData.get(key);
        if (container == null) {
            container = new ObjectContainer<T>();
            mData.put(key, container);
        } else if (!key.mSkipEquality && ObjectsCompat.equals(container.value, value)) {
            return;
        }

        container.value = value;
        notifyPropertyChanged(key);
    }

    @Override
    public Collection<PropertyKey> getAllSetProperties() {
        List<PropertyKey> properties = new ArrayList<>();
        for (Map.Entry<PropertyKey, ValueContainer> entry : mData.entrySet()) {
            if (entry.getValue() != null) properties.add(entry.getKey());
        }
        return properties;
    }

    @Override
    public Collection<PropertyKey> getAllProperties() {
        List<PropertyKey> properties = new ArrayList<>();
        for (Map.Entry<PropertyKey, ValueContainer> entry : mData.entrySet()) {
            properties.add(entry.getKey());
        }
        return properties;
    }

    /**
     * Determines whether the value for the provided key is the same in this model and a different
     * model.
     * @param otherModel The other {@link PropertyModel} to check.
     * @param key The {@link PropertyKey} to check.
     * @return Whether this model and {@code otherModel} have the same value set for {@code key}.
     */
    public boolean compareValue(PropertyModel otherModel, PropertyKey key) {
        validateKey(key);
        otherModel.validateKey(key);
        if (!mData.containsKey(key) || !otherModel.mData.containsKey(key)) return false;

        if (key instanceof WritableObjectPropertyKey
                && ((WritableObjectPropertyKey) key).mSkipEquality) {
            return false;
        }

        return ObjectsCompat.equals(mData.get(key), otherModel.mData.get(key));
    }

    /**
     * Allows constructing a new {@link PropertyModel} with read-only properties.
     */
    public static class Builder {
        private final Map<PropertyKey, ValueContainer> mData;

        public Builder(PropertyKey... keys) {
            this(buildData(keys));
        }

        private Builder(Map<PropertyKey, ValueContainer> values) {
            mData = values;
        }

        @RemovableInRelease
        private void validateKey(PropertyKey key) {
            if (!mData.containsKey(key)) {
                throw new IllegalArgumentException("Invalid key passed in: " + key);
            }
        }

        public Builder with(ReadableFloatPropertyKey key, float value) {
            validateKey(key);
            FloatContainer container = new FloatContainer();
            container.value = value;
            mData.put(key, container);
            return this;
        }

        public Builder with(ReadableIntPropertyKey key, int value) {
            validateKey(key);
            IntContainer container = new IntContainer();
            container.value = value;
            mData.put(key, container);
            return this;
        }

        public Builder with(ReadableBooleanPropertyKey key, boolean value) {
            validateKey(key);
            BooleanContainer container = new BooleanContainer();
            container.value = value;
            mData.put(key, container);
            return this;
        }

        public <T> Builder with(ReadableObjectPropertyKey<T> key, T value) {
            validateKey(key);
            ObjectContainer<T> container = new ObjectContainer<>();
            container.value = value;
            mData.put(key, container);
            return this;
        }

        /**
         * @param key The key of the specified {@link ReadableObjectPropertyKey<String>}.
         * @param resources The {@link Resources} for obtaining the specified string resource.
         * @param resId The specified string resource id.
         * @return The {@link Builder} with the specified key and string resource set.
         */
        public Builder with(
                ReadableObjectPropertyKey<String> key, Resources resources, @StringRes int resId) {
            if (resId != 0) with(key, resources.getString(resId));
            return this;
        }

        /**
         * @param key The key of the specified {@link ReadableObjectPropertyKey<Drawable>}.
         * @param context The {@link Context} for obtaining the specified drawable resource.
         * @param resId The specified drawable resource id.
         * @return The {@link Builder} with the specified key and drawable resource set.
         */
        public Builder with(
                ReadableObjectPropertyKey<Drawable> key, Context context, @DrawableRes int resId) {
            if (resId != 0) with(key, AppCompatResources.getDrawable(context, resId));
            return this;
        }

        public PropertyModel build() {
            return new PropertyModel(mData);
        }
    }

    /**
     * Merge lists of property keys.
     * @param k1 The first list of keys.
     * @param k2 The second list of keys.
     * @return A concatenated list of property keys.
     */
    public static PropertyKey[] concatKeys(PropertyKey[] k1, PropertyKey[] k2) {
        PropertyKey[] outList = new PropertyKey[k1.length + k2.length];
        System.arraycopy(k1, 0, outList, 0, k1.length);
        System.arraycopy(k2, 0, outList, k1.length, k2.length);
        return outList;
    }

    private static Map<PropertyKey, ValueContainer> buildData(PropertyKey[] keys) {
        Map<PropertyKey, ValueContainer> data = new HashMap<>();
        for (PropertyKey key : keys) {
            if (data.containsKey(key)) {
                throw new IllegalArgumentException("Duplicate key: " + key);
            }
            data.put(key, null);
        }
        return data;
    }

    private static class ValueContainer {}
    private static class FloatContainer extends ValueContainer {
        public float value;

        @Override
        public String toString() {
            return value + " in " + super.toString();
        }

        @Override
        public boolean equals(Object other) {
            return other != null && other instanceof FloatContainer
                    && ((FloatContainer) other).value == value;
        }
    }

    private static class IntContainer extends ValueContainer {
        public int value;

        @Override
        public String toString() {
            return value + " in " + super.toString();
        }

        @Override
        public boolean equals(Object other) {
            return other != null && other instanceof IntContainer
                    && ((IntContainer) other).value == value;
        }
    }

    private static class BooleanContainer extends ValueContainer {
        public boolean value;

        @Override
        public String toString() {
            return value + " in " + super.toString();
        }

        @Override
        public boolean equals(Object other) {
            return other != null && other instanceof BooleanContainer
                    && ((BooleanContainer) other).value == value;
        }
    }

    private static class ObjectContainer<T> extends ValueContainer {
        public T value;

        @Override
        public String toString() {
            return value + " in " + super.toString();
        }

        @Override
        public boolean equals(Object other) {
            return other != null && other instanceof ObjectContainer
                    && ObjectsCompat.equals(((ObjectContainer) other).value, value);
        }
    }
}
