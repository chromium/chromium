// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.build.BuildConfig;

import java.lang.ref.WeakReference;

/**
 * An {@link OptimizedFrameLayout} that increases the speed of frequent view lookup by ID by caching
 * the result of the lookup. Adding or removing a view with the same ID as a cached version will
 * cause the cache to be invalidated for that view and cause a re-lookup the next time it is
 * queried. The goal of this view type is to be used in cases where child views are frequently
 * accessed or reused, for example as part of a {@link androidx.recyclerview.widget.RecyclerView}.
 * The logic in the {@link #fastFindViewById(int)} method would be in {@link #findViewById(int)} if
 * it weren't final on the {@link View} class.
 *
 * {@link android.view.ViewGroup.OnHierarchyChangeListener}s cannot be used on ViewGroups that are
 * children of this group since they would overwrite the listeners that are critical to this class'
 * functionality.
 *
 * Usage:
 *     Use the same way that you would use a normal {@link android.widget.FrameLayout}, but instead
 *     of using {@link #findViewById(int)} to access views, use {@link #fastFindViewById(int)}.
 */
public class ViewLookupCachingFrameLayout extends OptimizedFrameLayout {
    /** A map containing views that have had lookup performed on them for quicker access. */
    private final SparseArray<WeakReference<View>> mCachedViews = new SparseArray<>();

    /** The hierarchy listener responsible for notifying the cache that the tree has changed. */
    @VisibleForTesting
    final OnHierarchyChangeListener mListener =
            new OnHierarchyChangeListener() {
                @Override
                public void onChildViewAdded(View parent, View child) {
                    mCachedViews.remove(child.getId());
                    setHierarchyListenerOnTree(child, this);
                }

                @Override
                public void onChildViewRemoved(View parent, View child) {
                    mCachedViews.remove(child.getId());
                    setHierarchyListenerOnTree(child, null);
                }
            };

    /** Default constructor for use in XML. */
    public ViewLookupCachingFrameLayout(Context context, AttributeSet atts) {
        super(context, atts);
        setOnHierarchyChangeListener(mListener);
    }

    @Override
    public void setOnHierarchyChangeListener(OnHierarchyChangeListener listener) {
        assert listener == mListener : "Hierarchy change listeners cannot be set for this group!";
        super.setOnHierarchyChangeListener(listener);
    }

    /**
     * Set the hierarchy listener that invalidates relevant parts of the cache when subtrees change.
     * @param view The root of the tree to attach listeners to.
     * @param listener The listener to attach (null to unset).
     */
    private void setHierarchyListenerOnTree(View view, OnHierarchyChangeListener listener) {
        if (!(view instanceof ViewGroup)) return;

        ViewGroup group = (ViewGroup) view;
        group.setOnHierarchyChangeListener(listener);
        for (int i = 0; i < group.getChildCount(); i++) {
            setHierarchyListenerOnTree(group.getChildAt(i), listener);
        }
    }

    /**
     * Does the same thing as {@link #findViewById(int)} but caches the result if not null.
     * Subsequent lookups are cheaper as a result. Adding or removing a child view invalidates
     * the cache for the ID of the view removed and causes a re-lookup.
     * @param id The ID of the view to lookup.
     * @return The view if it exists.
     */
    @Nullable
    public View fastFindViewById(@IdRes int id) {
        WeakReference<View> ref = mCachedViews.get(id);
        View view = null;
        if (ref != null) view = ref.get();
        if (view == null) view = findViewById(id);
        if (BuildConfig.ENABLE_ASSERTS) {
            assert view == findViewById(id) : "View caching logic is broken!";
            assert ref == null || ref.get() != null
                    : "Cache held reference to garbage collected view!";
        }

        if (view != null) mCachedViews.put(id, new WeakReference<>(view));

        return view;
    }

    @VisibleForTesting
    SparseArray<WeakReference<View>> getCache() {
        return mCachedViews;
    }
}
