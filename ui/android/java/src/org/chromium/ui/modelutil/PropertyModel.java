// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.modelutil;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.drawable.Drawable;
import android.util.ArrayMap;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.util.ObjectsCompat;

import org.chromium.build.BuildConfig;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

/**
 * Generic property model that aims to provide an extensible and efficient model for ease of use.
 */
@NullMarked
public class PropertyModel extends PropertyObservable<PropertyKey> {
    /** A PropertyKey implementation that associates a name with the property for easy debugging. */
    static class NamedPropertyKey implements PropertyKey {
        private final @Nullable String mPropertyName;

        protected NamedPropertyKey(@Nullable String propertyName) {
            mPropertyName = propertyName;
        }

        @Override
        public String toString() {
            if (mPropertyName == null) return super.toString();
            return mPropertyName;
        }
    }

    /** The key type for read-only boolean model properties. */
    public static class ReadableBooleanPropertyKey extends NamedPropertyKey {
        /** Constructs a new unnamed read-only boolean property key. */
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
    public static final class WritableBooleanPropertyKey extends ReadableBooleanPropertyKey {
        /** Constructs a new unnamed writable boolean property key. */
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
        /** Constructs a new unnamed read-only float property key. */
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
    public static final class WritableFloatPropertyKey extends ReadableFloatPropertyKey {
        /** Constructs a new unnamed writable float property key. */
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
        /** Constructs a new unnamed read-only integer property key. */
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
    public static final class WritableIntPropertyKey extends ReadableIntPropertyKey {
        /** Constructs a new unnamed writable integer property key. */
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
     * The key type for read-only integer model properties with an IntDef type.
     *
     * @param <T> The IntDef annotation type being tracked by the key.
     */
    public static class ReadableIntDefPropertyKey<T> extends ReadableIntPropertyKey {
        /** The default value for this property when unset in the model. */
        public final int defaultValue;

        /**
         * Constructs a new unnamed read-only integer IntDef property key with a default value.
         *
         * @param defaultValue The default value when unset.
         */
        public ReadableIntDefPropertyKey(int defaultValue) {
            this(defaultValue, null);
        }

        /**
         * Constructs a new named read-only integer IntDef property key, e.g. for use in debugging.
         *
         * @param defaultValue The default value when unset.
         * @param name The optional name of the property.
         */
        public ReadableIntDefPropertyKey(int defaultValue, @Nullable String name) {
            super(name);
            this.defaultValue = defaultValue;
        }
    }

    /**
     * The key type for mutable integer model properties with an IntDef type.
     *
     * @param <T> The IntDef annotation type being tracked by the key.
     */
    public static final class WritableIntDefPropertyKey<T> extends ReadableIntDefPropertyKey<T> {
        /**
         * Constructs a new unnamed writable integer IntDef property key with a default value.
         *
         * @param defaultValue The default value when unset.
         */
        public WritableIntDefPropertyKey(int defaultValue) {
            super(defaultValue, null);
        }

        /**
         * Constructs a new named writable integer IntDef property key, e.g. for use in debugging.
         *
         * @param defaultValue The default value when unset.
         * @param name The optional name of the property.
         */
        public WritableIntDefPropertyKey(int defaultValue, @Nullable String name) {
            super(defaultValue, name);
        }
    }

    /** The key type for read-only long model properties. */
    public static class ReadableLongPropertyKey extends NamedPropertyKey {
        /** Constructs a new unnamed read-only long property key. */
        public ReadableLongPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named read-only long property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public ReadableLongPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /** The key type for mutable int model properties. */
    public static final class WritableLongPropertyKey extends ReadableLongPropertyKey {
        /** Constructs a new unnamed writable long property key. */
        public WritableLongPropertyKey() {
            this(null);
        }

        /**
         * Constructs a new named writable long property key, e.g. for use in debugging.
         * @param name The optional name of the property.
         */
        public WritableLongPropertyKey(@Nullable String name) {
            super(name);
        }
    }

    /**
     * The key type for read-only Object model properties.
     *
     * @param <T> The type of the Object being tracked by the key.
     */
    public static class ReadableObjectPropertyKey<T extends @Nullable Object>
            extends NamedPropertyKey {
        /** Constructs a new unnamed read-only object property key. */
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
    public static final class WritableObjectPropertyKey<T extends @Nullable Object>
            extends ReadableObjectPropertyKey<T> {
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

    /**
     * A key type that allows transforming the value type stored in the model to a different output
     * format. Some examples where this key type is useful:
     *
     * <ul>
     *   <li>In a RecyclerView where you want to defer expensive or memory intensive operations
     *       until it is needed to display on screen.
     *   <li>To avoid leaking implementation details about the conversion to View classes.
     * </ul>
     *
     * @param <T> The type value stored in the model.
     * @param <V> The type of transformed output.
     */
    public static class ReadableTransformingObjectPropertyKey<
                    T extends @Nullable Object, V extends @Nullable Object>
            extends NamedPropertyKey {
        /** Constructor for a named {@link ReadableTransformingObjectPropertyKey}. */
        public ReadableTransformingObjectPropertyKey(@Nullable String name) {
            super(name);
        }

        /** Constructor for an unnamed {@link ReadableTransformingObjectPropertyKey}. */
        public ReadableTransformingObjectPropertyKey() {
            this((String) null);
        }
    }

    /**
     * A version of {@link ReadableTransformingObjectPropertyKey} that supports the value being
     * mutated.
     *
     * @param <T> The type value stored in the model.
     * @param <V> The type of transformed output.
     */
    public static final class WritableTransformingObjectPropertyKey<
                    T extends @Nullable Object, V extends @Nullable Object>
            extends ReadableTransformingObjectPropertyKey<T, V> {
        /** Constructor for a named {@link WritableTransformingObjectPropertyKey}. */
        public WritableTransformingObjectPropertyKey(@Nullable String name) {
            super(name);
        }

        /** Constructor for an unnamed {@link WritableTransformingObjectPropertyKey}. */
        public WritableTransformingObjectPropertyKey() {
            this((String) null);
        }
    }

    private static final Object UNSET = new Object();

    private final Map<PropertyKey, @Nullable Object> mData;
    private final @Nullable Map<ReadableTransformingObjectPropertyKey<?, ?>, Function<?, ?>>
            mTransformers;

    /**
     * Constructs a model for the given list of keys.
     *
     * @param keys The key types supported by this model.
     */
    public PropertyModel(PropertyKey... keys) {
        this(buildData(Arrays.asList(keys)));
    }

    /**
     * Constructs a model with a generic collection of existing keys.
     *
     * @param keys The key types supported by this model.
     */
    public PropertyModel(Collection<PropertyKey> keys) {
        this(buildData(keys));
    }

    private PropertyModel(Map<PropertyKey, @Nullable Object> startingValues) {
        this(startingValues, null);
    }

    private PropertyModel(
            Map<PropertyKey, @Nullable Object> startingValues,
            @Nullable
                    Map<ReadableTransformingObjectPropertyKey<?, ?>, Function<?, ?>> transformers) {
        mData = startingValues;
        mTransformers = transformers;
    }

    /** Returns whether the given key is contained in the property model. */
    public boolean containsKey(PropertyKey key) {
        return mData.containsKey(key);
    }

    /** Returns whether the given key is contained and the value is equal to the passed arg. */
    public boolean containsKeyEqualTo(ReadableBooleanPropertyKey key, boolean value) {
        return mData.containsKey(key) && get(key) == value;
    }

    /** Returns whether the given key is contained and the value is equal to the passed arg. */
    public boolean containsKeyEqualTo(ReadableIntPropertyKey key, int value) {
        return mData.containsKey(key) && get(key) == value;
    }

    private void validateKey(PropertyKey key) {
        if (BuildConfig.ENABLE_ASSERTS && !mData.containsKey(key)) {
            throw new IllegalArgumentException(
                    "Invalid key passed in: " + key + ". Current data is: " + mData.toString());
        }
    }

    /** Get the current value from the float based key. */
    public float get(ReadableFloatPropertyKey key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? 0f : (float) assumeNonNull(value);
    }

    /** Set the value for the float based key. */
    public void set(WritableFloatPropertyKey key, float value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && (float) assumeNonNull(prev) == value) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    /** Get the current value from the int based key. */
    public int get(ReadableIntPropertyKey key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? 0 : (int) assumeNonNull(value);
    }

    /** Get the current value from the int-def based key. */
    public int get(ReadableIntDefPropertyKey<?> key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? key.defaultValue : (int) assumeNonNull(value);
    }

    /** Set the value for the int based key. */
    public void set(WritableIntPropertyKey key, int value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && (int) assumeNonNull(prev) == value) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    /** Set the value for the int-def based key. */
    public void set(WritableIntDefPropertyKey<?> key, int value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && (int) assumeNonNull(prev) == value) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    /** Get the current value from the long based key. */
    public long get(ReadableLongPropertyKey key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? 0L : (long) assumeNonNull(value);
    }

    /** Set the value for the long based key. */
    public void set(WritableLongPropertyKey key, long value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && (long) assumeNonNull(prev) == value) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    /** Get the current value from the boolean based key. */
    public boolean get(ReadableBooleanPropertyKey key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? false : (boolean) assumeNonNull(value);
    }

    /** Set the value for the boolean based key. */
    public void set(WritableBooleanPropertyKey key, boolean value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && (boolean) assumeNonNull(prev) == value) {
            return;
        }

        mData.put(key, value ? Boolean.TRUE : Boolean.FALSE);
        notifyPropertyChanged(key);
    }

    /** Get the current value from the object based key. */
    @SuppressWarnings("unchecked")
    @NullUnmarked // https://github.com/uber/NullAway/issues/1075
    public <T extends @Nullable Object> T get(ReadableObjectPropertyKey<T> key) {
        validateKey(key);
        Object value = mData.get(key);
        return value == UNSET ? null : (T) value;
    }

    /** Set the value for the Object based key. */
    @SuppressWarnings("unchecked")
    @NullUnmarked // https://github.com/uber/NullAway/issues/1075
    public <T extends @Nullable Object> void set(
            WritableObjectPropertyKey<T> key, @Nullable T value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && !key.mSkipEquality && ObjectsCompat.equals(prev, value)) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    /** Get the transformed value from the current value of an object based key. */
    @SuppressWarnings({"unchecked", "NullAway"}) // Can't check container != null when T is @NonNull
    @NullUnmarked // https://github.com/uber/NullAway/issues/1075
    public <T extends @Nullable Object, V extends @Nullable Object> V get(
            ReadableTransformingObjectPropertyKey<T, V> key) {
        assumeNonNull(mTransformers);
        validateKey(key);
        Object value = mData.get(key);
        if (value == UNSET) return null;
        var transformer = (Function<T, V>) mTransformers.get(key);
        assert transformer != null : "No transformer associated with: " + key;
        return transformer.apply((T) value);
    }

    /** Set the value for the transforming Object based key. */
    @SuppressWarnings("unchecked")
    @NullUnmarked // https://github.com/uber/NullAway/issues/1075
    public <T extends @Nullable Object, V extends @Nullable Object> void set(
            WritableTransformingObjectPropertyKey<T, V> key, T value) {
        validateKey(key);
        Object prev = mData.get(key);
        if (prev != UNSET && ObjectsCompat.equals(prev, value)) {
            return;
        }

        mData.put(key, value);
        notifyPropertyChanged(key);
    }

    @Override
    public Collection<PropertyKey> getAllSetProperties() {
        List<PropertyKey> properties = new ArrayList<>();
        for (Map.Entry<PropertyKey, @Nullable Object> entry : mData.entrySet()) {
            if (entry.getValue() != UNSET) properties.add(entry.getKey());
        }
        return properties;
    }

    @Override
    public Collection<PropertyKey> getAllProperties() {
        List<PropertyKey> properties = new ArrayList<>();
        for (Map.Entry<PropertyKey, @Nullable Object> entry : mData.entrySet()) {
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
     * Returns the int value from the item model based on the key. Otherwise returns the passed in
     * default value.
     *
     * @param model The model for the list menu item.
     * @param key The key of the property to retrieve.
     * @param defaultValue The default value if the the property is not found.
     * @return The value from the model or the default if the value is not found.
     */
    public static int getFromModelOrDefault(
            PropertyModel model, PropertyModel.ReadableIntPropertyKey key, int defaultValue) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (model.containsKey(key)) {
            return model.get(key);
        }
        return defaultValue;
    }

    /**
     * Returns the value from the item model based on the key. Otherwise returns the passed in
     * default value.
     *
     * @param model The model for the list menu item.
     * @param key The key of the property to retrieve.
     * @param defaultValue The default value if the the property is not found.
     * @return The value from the model or the default if the value is not found.
     */
    @SuppressWarnings("NullAway") // Cannot ensure model.get() is non-null when T is @NonNull.
    public static <T extends @Nullable Object> T getFromModelOrDefault(
            PropertyModel model, PropertyModel.ReadableObjectPropertyKey<T> key, T defaultValue) {
        // We need to check first because PropertyModel#get throws an exception if a key
        // is not present in the Map.
        if (model.containsKey(key)) {
            return model.get(key);
        }
        return defaultValue;
    }

    /** Allows constructing a new {@link PropertyModel} with read-only properties. */
    public static class Builder {
        private final Map<PropertyKey, @Nullable Object> mData;
        private @Nullable Map<ReadableTransformingObjectPropertyKey<?, ?>, Function<?, ?>>
                mTransformers;

        public Builder(PropertyKey... keys) {
            this(buildData(Arrays.asList(keys)));
        }

        public Builder(Collection<PropertyKey> keys) {
            this(buildData(keys));
        }

        private Builder(Map<PropertyKey, @Nullable Object> values) {
            mData = values;
        }

        private void validateKey(PropertyKey key) {
            if (BuildConfig.ENABLE_ASSERTS && !mData.containsKey(key)) {
                throw new IllegalArgumentException("Invalid key passed in: " + key);
            }
        }

        public Builder with(ReadableFloatPropertyKey key, float value) {
            validateKey(key);
            mData.put(key, value);
            return this;
        }

        public Builder with(ReadableIntPropertyKey key, int value) {
            validateKey(key);
            mData.put(key, value);
            return this;
        }

        public Builder with(ReadableLongPropertyKey key, long value) {
            validateKey(key);
            mData.put(key, value);
            return this;
        }

        public Builder with(ReadableBooleanPropertyKey key, boolean value) {
            validateKey(key);
            mData.put(key, value ? Boolean.TRUE : Boolean.FALSE);
            return this;
        }

        @NullUnmarked // https://github.com/uber/NullAway/issues/1075
        public <T extends @Nullable Object> Builder with(
                ReadableObjectPropertyKey<T> key, @Nullable T value) {
            validateKey(key);
            mData.put(key, value);
            return this;
        }

        /**
         * @param key The key of the specified {@link ReadableObjectPropertyKey<String>}.
         * @param resources The {@link Resources} for obtaining the specified string resource.
         * @param resId The specified string resource id.
         * @return The {@link Builder} with the specified key and string resource set.
         */
        @SuppressWarnings({"rawtypes", "unchecked"})
        public Builder with(
                ReadableObjectPropertyKey<? extends CharSequence> key,
                Resources resources,
                @StringRes int resId) {
            if (resId != 0) {
                with((ReadableObjectPropertyKey) key, resources.getString(resId));
            }
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

        /**
         * Adds a transforming key. The passed in {@link ReadableTransformingObjectPropertyKey} must
         * not already exist in the set of keys.
         *
         * @param key The key to be added to the {@link PropertyModel}.
         * @param transformer The function that will transform the value stored in the {@link
         *     PropertyModel} to the output format.
         * @return This {@link Builder} instance.
         * @param <T> The type value stored in the model.
         * @param <V> The type of transformed output.
         */
        @NullUnmarked // https://github.com/uber/NullAway/issues/1075
        public <T extends @Nullable Object, V extends @Nullable Object> Builder withTransformingKey(
                ReadableTransformingObjectPropertyKey<T, V> key, Function<T, V> transformer) {
            if (BuildConfig.ENABLE_ASSERTS && mData.containsKey(key)) {
                throw new IllegalArgumentException("Transforming key already exists.");
            }
            mData.put(key, UNSET);
            // Transforming keys are typically limited to 1-2 properties (e.g. text or icon
            // transformation in views), so capacity 2 minimizes memory footprint without rehashing.
            if (mTransformers == null) mTransformers = new ArrayMap<>(2);
            assert transformer != null : "Requires non-null transformer";
            mTransformers.put(key, transformer);
            return this;
        }

        /**
         * Adds a transforming key and initial value into the property model. The passed in {@link
         * ReadableTransformingObjectPropertyKey} must not already exist in the set of keys.
         *
         * @param key The key to be added to the {@link PropertyModel}.
         * @param transformer The function that will transform the value stored in the {@link
         *     PropertyModel} to the output format.
         * @param value The initial value to be stored in the {@link PropertyModel}.
         * @return This {@link Builder} instance.
         * @param <T> The type value stored in the model.
         * @param <V> The type of transformed output.
         */
        @NullUnmarked // https://github.com/uber/NullAway/issues/1075
        public <T extends @Nullable Object, V extends @Nullable Object> Builder withTransformingKey(
                ReadableTransformingObjectPropertyKey<T, V> key,
                Function<T, V> transformer,
                T value) {
            withTransformingKey(key, transformer);
            mData.put(key, value);
            return this;
        }

        public PropertyModel build() {
            return new PropertyModel(mData, mTransformers);
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

    // Threshold below which ArrayMap is used instead of HashMap to reduce memory overhead and
    // avoid entry object allocations for typical small key sets (<= 32).
    private static final int ARRAY_MAP_KEY_THRESHOLD = 32;

    private static Map<PropertyKey, @Nullable Object> buildData(Collection<PropertyKey> keys) {
        int size = keys.size();
        Map<PropertyKey, @Nullable Object> data =
                size <= ARRAY_MAP_KEY_THRESHOLD
                        ? new ArrayMap<PropertyKey, @Nullable Object>(size)
                        : new HashMap<PropertyKey, @Nullable Object>(size);
        for (PropertyKey key : keys) {
            if (data.containsKey(key)) {
                throw new IllegalArgumentException("Duplicate key: " + key);
            }
            data.put(key, UNSET);
        }
        return data;
    }
}
