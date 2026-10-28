// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero

@JNINamespace("jni_zero::tests")
class SampleSafePointersKotlin {
  @JniType("::jni_zero::tests::KotlinNativeObj") interface NativeObj : JniTypeToken

  @CalledByNative fun onPtr(ptr: JniPtr<NativeObj>, nullablePtr: JniPtr<NativeObj>?) {}

  fun exercise() {
    val raw: JniRawPtr<NativeObj> = SampleSafePointersKotlinJni.get().borrowRaw()
    val maybeRaw: JniRawPtr<NativeObj>? = SampleSafePointersKotlinJni.get().maybeBorrowRaw()
    SampleSafePointersKotlinJni.get().usePtr(raw)
    SampleSafePointersKotlinJni.get().useNullablePtr(maybeRaw)
    raw.release()
    maybeRaw?.release()

    val owned: JniUniquePtr<NativeObj> = SampleSafePointersKotlinJni.get().createOwned()
    SampleSafePointersKotlinJni.get().usePtr(owned)
    owned.destroy()
  }

  @NativeMethods
  interface Natives {
    fun borrowRaw(): JniRawPtr<NativeObj>

    fun maybeBorrowRaw(): JniRawPtr<NativeObj>?

    fun createOwned(): JniUniquePtr<NativeObj>

    fun usePtr(self: JniPtr<NativeObj>)

    fun useNullablePtr(ptr: JniPtr<NativeObj>?)
  }
}
