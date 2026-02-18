# JNI in Chromium

This document describes how to use JNI Zero in Chromium.

[TOC]

## GN Build Rules

Templates are in [//third_party/jni_zero/jni_zero.gni](https://source.chromium.org/chromium/chromium/src/+/main:third_party/jni_zero/jni_zero.gni).

### generate_jni

Given a set of Java files, generates a header file to call into Java for all
`@CalledByNative` functions. If `@NativeMethods` is present, also generates a
`.srcjar` containing `<ClassName>Jni.java`, which should be depended on via the
generated GN target `<generate_jni's target name>_java`.

** Example:**
```python
generate_jni("abcd_jni") {
  sources = [ "org/chromium/foo/Bar.java" ]
}

android_library("abcd_java") {
  ...
  # For the generated `${Bar}Jni` classes.
  deps = [ ":abcd_jni_java" ]
}

source_set("abcd") {
 ...
 # Allows the cc files to include the generated `${OriginalClassName}_jni.h`
 # headers.
 deps = [ ":abcd_jni" ]
}
```

### generate_jar_jni

Given a `.jar` file (defaults to `android.jar`), generates a header file
similar to `generate_jni`, if every method and public field were annotated by
`@CalledByNative`.

### generate_jni_registration

Generates a whole-program Java and native link - required for all Java that
calls into native via `@NativeMethods`.

### shared_library_with_jni

A wrapper around a `shared_library` that bundles a `generate_jni_registration`.

### component_with_jni

Same as `shared_library` but for a `component`.

## Chromium-Specific Type Conversions {#jnitype}

JNI Zero supports automatic type conversions via the `@JniType` annotation. See
the main [README.md](README.md#jnitype) for details and a list of conversions
provided by JNI Zero.

### Strings

Use `@JniType("std::string")` for UTF-8 or `@JniType("std::u16string")` for UTF-16.

**Java:**
```java
void onNameChanged(@JniType("std::string") String name);
@JniType("std::string") String getName();
```

**C++:**
```c++
#include "base/android/jni_string.h"
#include "path/to/generated_jni/MyClass_jni.h"

void JNI_MyClass_OnNameChanged(JNIEnv* env, const std::string& name) { ... }
std::string JNI_MyClass_GetName(JNIEnv* env) { return "name"; }
```

**Header:** `#include "base/android/jni_string.h"`
**Null Handling:** `null` Java strings are converted to empty `std::string` or `std::u16string`.

### Callbacks (Java -> Native)

To pass a Java `Runnable` or `Callback` to C++ as a `base::OnceClosure` or `base::OnceCallback`.

**Java:**
```java
void doSomething(@JniType("base::OnceClosure") Runnable callback);
void fetchSuccess(@JniType("base::OnceCallback<void(std::string)>") Callback<String> callback);
```

**C++:**
```c++
#include "base/android/callback_android.h"
#include "path/to/generated_jni/MyClass_jni.h"

void JNI_MyClass_DoSomething(JNIEnv* env, base::OnceClosure callback) {
    std::move(callback).Run();
}
```

**Header:** `#include "base/android/callback_android.h"`

### Callbacks (Native -> Java)

To pass a C++ callback to Java, use `ToJniCallback()`. The Java side should
receive a `JniOnceCallback<T>`, `JniRepeatedCallback<T>`, `JniOnceRunnable`, or
`JniRepeatingRunnable` (which for the Once* variants, can be typed as
`Runnable` or `Callback` if you don't need to call `destroy()`.

**Java:**
```java
@CalledByNative
void onResult(JniOnceCallback<String> callback) {
    callback.onResult("success");
}
```

**C++:**
```c++
#include "base/android/jni_callback.h"
#include "path/to/generated_jni/MyClass_jni.h"

void Finish(JNIEnv* env, base::OnceCallback<void(std::string)> callback) {
    Java_MyClass_onResult(env, ToJniCallback(env, std::move(callback)));
}
```

**Header:** `#include "base/android/jni_callback.h"`
**Important:** For the `Once*` variants, you must call `destroy()` on the Java
callback if it is never run to avoid leaking the native object.
`JniOnceCallback` and `JniOnceRunnable` will automatically destroy themselves
after one call. For the `Repeated*` variants, you must call `destroy()` when
done with them.

### Nullable Parameters (std::optional)

Use `std::optional<T>` to handle nullable parameters.

**Java:**
```java
void maybeDoSomething(@JniType("std::optional<std::string>") String maybeName);
```

**C++:**
```c++
#include "third_party/jni_zero/default_conversions.h"
#include "base/android/jni_string.h"
#include "path/to/generated_jni/MyClass_jni.h"

void JNI_MyClass_MaybeDoSomething(JNIEnv* env, std::optional<std::string> maybeName) {
    if (maybeName) { ... }
}
```

**Header:** `#include "third_party/jni_zero/default_conversions.h"`
**Behavior:** `null` is converted to `std::nullopt`.

### Collections and Spans

JNI Zero can convert between Java arrays/Lists and C++ containers or `base::span`.

#### base::span (Native -> Java)

You can pass a `base::span` from Native to Java where Java expects an array or a `List`.

**Java:**
```java
@CalledByNative
void showItems(@JniType("base::span<const GURL>") GURL[] items);
```

**C++:**
```c++
#include "url/android/gurl_android.h"
#include "path/to/generated_jni/MyClass_jni.h"

void SendItems(JNIEnv* env, base::span<const GURL> items) {
    Java_MyClass_showItems(env, items);
}
```

**Header:** `#include "third_party/jni_zero/default_conversions.h"` (and the
header for the element type, e.g., `url/android/gurl_android.h`)

## List of Available Conversions

Searching for `FromJniType` or `ToJniType` specializations will reveal more. Common ones include:

| C++ Type | Java Type | Header |
| :--- | :--- | :--- |
| `std::string` | `String` | `base/android/jni_string.h` |
| `std::u16string` | `String` | `base/android/jni_string.h` |
| `base::OnceClosure` | `Runnable` | `base/android/callback_android.h` |
| `base::OnceCallback<void(T)>`| `Callback<T>` | `base/android/callback_android.h` |
| `GURL` | `GURL` | `url/android/gurl_android.h` |
| `base::Token` | `Token` | `base/android/token_android.h` |
| `base::UnguessableToken` | `UnguessableToken` | `base/android/unguessable_token_android.h` |
