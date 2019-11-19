// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.weblayer_private.interfaces;

import android.os.IBinder;

import java.lang.reflect.Field;

/**
 * This wraps an object to be transferred across sibling classloaders in the same process via the
 * IObjectWrapper AIDL interface. By using reflection to retrieve the object, no serialization needs
 * to occur.
 *
 * @param <T> The type of the wrapped object.
 */
public final class ObjectWrapper<T> extends IObjectWrapper.Stub {
    /**
     * The wrapped object. You must not add another member in this class because the check for
     * retrieving this member variable is that this is the ONLY member variable declared in this
     * class and it is private. This is because ObjectWrapper can be obfuscated, so that this member
     * variable can have an obfuscated name.
     */
    private final T mWrappedObject;

    /* DO NOT ADD NEW CLASS MEMBERS (see above) */

    /** Disable creating an object wrapper. Instead, use {@link #wrap(Object)}. */
    private ObjectWrapper(T object) {
        mWrappedObject = object;
    }

    /**
     * Create the wrapped object.
     *
     * @param object The object instance to wrap.
     * @return The wrapped object.
     */
    public static <T> IObjectWrapper wrap(T object) {
        return new ObjectWrapper<T>(object);
    }

    /**
     * Unwrap the object within the {@link IObjectWrapper} using reflection.
     *
     * @param remote The {@link IObjectWrapper} instance to unwrap.
     * @param clazz The {@link Class} of the unwrapped object type.
     * @return The unwrapped object.
     */
    public static <T> T unwrap(IObjectWrapper remote, Class<T> clazz) {
        if (remote == null) return null;

        // Handle the case when not getting an IObjectWrapper from a sibling classloader
        if (remote instanceof ObjectWrapper) {
            @SuppressWarnings("unchecked")
            ObjectWrapper<T> typedRemote = ((ObjectWrapper<T>) remote);
            return typedRemote.mWrappedObject;
        }

        IBinder remoteBinder = remote.asBinder();

        // It is possible that ObjectWrapper was obfuscated in which case wrappedObject
        // would have a different name.  The following checks that there is a single
        // declared field that is private.
        Class<?> remoteClazz = remoteBinder.getClass();

        Field validField = null;
        for (Field field : remoteClazz.getDeclaredFields()) {
            if (field.isSynthetic()) continue;

            // Only one valid, non-synthetic field is allowed on the class.
            if (validField != null) {
                validField = null;
                break;
            }
            validField = field;
        }

        if (validField == null || validField.isAccessible()) {
            throw new IllegalArgumentException("The concrete class implementing IObjectWrapper"
                    + " must have exactly *one* declared *private* field for the wrapped object. "
                    + " Preferably, this is an instance of the ObjectWrapper<T> class.");
        }

        validField.setAccessible(true);
        try {
            Object wrappedObject = validField.get(remoteBinder);
            if (wrappedObject == null) return null;
            if (!clazz.isInstance(wrappedObject)) {
                throw new IllegalArgumentException("remoteBinder is the wrong class.");
            }
            return clazz.cast(wrappedObject);
        } catch (NullPointerException e) {
            throw new IllegalArgumentException("Binder object is null.", e);
        } catch (IllegalArgumentException e) {
            throw new IllegalArgumentException("remoteBinder is the wrong class.", e);
        } catch (IllegalAccessException e) {
            throw new IllegalArgumentException("Could not access the field in remoteBinder.", e);
        }
    }
}
