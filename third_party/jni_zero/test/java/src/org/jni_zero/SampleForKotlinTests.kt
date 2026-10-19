// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.jni_zero

@JNINamespace("jni_zero::tests")
class SampleForKotlinTests @CalledByNative constructor(val id: Long) {
  @CalledByNative
  fun onEvent(code: Int, message: String): Boolean {
    return SampleForKotlinTestsJni.get().calculate(code, 1) > 0
  }

  @get:CalledByNative val count: Int = 42

  companion object {
    @JvmStatic
    @CalledByNative
    fun create(id: Long): SampleForKotlinTests {
      return SampleForKotlinTests(id)
    }
  }

  @NativeMethods
  interface Natives {
    fun init(nativeSampleForKotlinTests: Long)

    fun calculate(a: Int, b: Int): Int

    fun toInts(bytes: ByteArray): IntArray

    fun echoCppString(str: @JniType("std::string") String): @JniType("std::string") String

    fun maybe(s: String?): String?

    fun strings(
      list: @JniType("std::vector<std::string>") List<String>,
    ): @JniType("std::vector<std::string>") Array<String>

    fun clazz(c: Class<*>): Class<*>

    fun any(o: Any?): Any?
  }
}
