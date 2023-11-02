// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.ui.modelutil;

/**
 * Base class for a {@link ListObservable} containing a {@link SimpleList} of items.
 * It allows models to compose different ListObservables.
 * Under the hood this class is just a shorthand for {@link ListModelBase} with a
 * {@link Void} partial change notification payload type, for list types {@code T} that don't
 * support partial change notification.
 * @param <T> The object type that this class manages in a list.
 */
public class ListModel<T> extends ListModelBase<T, Void> {}
