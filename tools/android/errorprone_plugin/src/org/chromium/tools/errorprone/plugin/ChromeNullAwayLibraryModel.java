// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.tools.errorprone.plugin;

import static com.uber.nullaway.LibraryModels.MethodRef.methodRef;

import com.google.common.collect.ImmutableSet;
import com.google.common.collect.ImmutableSetMultimap;
import com.uber.nullaway.LibraryModels;

import org.chromium.build.annotations.ServiceImpl;

/** Checks for calls to .stream(). See //styleguide/java/java.md for rationale. */
@ServiceImpl(LibraryModels.class)
public class ChromeNullAwayLibraryModel implements LibraryModels {

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> failIfNullParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> explicitlyNullableParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nonNullParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesTrueParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesFalseParameters() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> nullImpliesNullParameters() {
        String sharedPrefs = "android.content.SharedPreferences";
        return new ImmutableSetMultimap.Builder<MethodRef, Integer>()
                .put(methodRef(sharedPrefs, "getString(java.lang.String,java.lang.String)"), 1)
                .put(
                        methodRef(
                                sharedPrefs,
                                "getStringSet(java.lang.String,java.util.Set<java.lang.String>)"),
                        1)
                .put(methodRef("android.content.Intent", "normalizeMimeType(java.lang.String)"), 0)
                .put(methodRef("android.util.LongSparseArray", "get(long,E)"), 1)
                .put(methodRef("android.util.SparseArray", "get(int,E)"), 1)
                .build();
    }

    @Override
    public ImmutableSet<MethodRef> nullableReturns() {
        return ImmutableSet.of(
                methodRef("android.util.SparseArray", "get(int)"),
                methodRef("android.util.LongSparseArray", "get(long)"),
                methodRef("java.util.Map", "put(K, V)"),
                methodRef("java.util.Map", "remove(java.lang.Object)"));
    }

    @Override
    public ImmutableSet<MethodRef> nonNullReturns() {
        return ImmutableSet.of(
                // Null only during onCreate().
                methodRef("android.content.ContentProvider", "getContext()"),
                // While findViewById() can return null, call sites are generally pretty confident
                // that they don't since we use generated constants that map to XML layouts.
                methodRef("androidx.appcompat.app.AppCompatDelegate", "findViewById(int)"),
                methodRef("androidx.appcompat.app.AppCompatDialog", "findViewById(int)"),
                methodRef("androidx.preference.PreferenceViewHolder", "findViewById(int)"),
                // For correct inputs, findPreference() basically always returns non-null.
                methodRef(
                        "androidx.preference.PreferenceGroup",
                        "<T>findPreference(java.lang.CharSequence)"),
                methodRef(
                        "androidx.preference.PreferenceManager",
                        "<T>findPreference(java.lang.CharSequence)"),
                methodRef(
                        "androidx.preference.PreferenceFragment",
                        "<T>findPreference(java.lang.CharSequence)"),
                methodRef(
                        "androidx.preference.PreferenceFragmentCompat",
                        "<T>findPreference(java.lang.CharSequence)"));
    }

    @Override
    public ImmutableSetMultimap<MethodRef, Integer> castToNonNullMethods() {
        return ImmutableSetMultimap.of();
    }

    @Override
    public ImmutableSetMultimap<String, Integer> typeVariablesWithNullableUpperBounds() {
        // TODO(https://github.com/uber/NullAway/issues/1212): Add FutureTask:
        //      .put("java.util.concurrent.FutureTask", 0)
        return new ImmutableSetMultimap.Builder<String, Integer>()
                .put("java.util.concurrent.Callable", 0)
                .put("java.util.concurrent.CompletableFuture", 0)
                .put("java.util.concurrent.CompletionStage", 0)
                .put("java.util.concurrent.Future", 0)
                .put("java.util.concurrent.RunnableFuture", 0)
                .put("java.util.function.BiConsumer", 0)
                .put("java.util.function.BiConsumer", 1)
                .put("java.util.function.BiFunction", 0)
                .put("java.util.function.BiFunction", 1)
                .put("java.util.function.BiFunction", 2)
                .put("java.util.function.Consumer", 0)
                .put("java.util.function.DoubleFunction", 0)
                .put("java.util.function.IntFunction", 0)
                .put("java.util.function.Function", 0)
                .put("java.util.function.Function", 1)
                .put("java.util.function.LongFunction", 0)
                .put("java.util.function.Predicate", 0)
                .put("java.util.function.Supplier", 0)
                .build();
    }

    @Override
    public ImmutableSet<String> nullMarkedClasses() {
        return typeVariablesWithNullableUpperBounds().keySet();
    }
}
