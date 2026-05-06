---
name: jni-zero
description: Guidance for working with JNI Zero to call between C++ and Java.
---

# JNI Zero Guidance

JNI Zero is a code generator that lives at `//third_party/jni_zero`. It's
written in Python and has both C++ and Java APIs. The main reference is in
`//third_party/jni_zero/README.md`.

## Guidance for GN

- Use `_jni` as a suffix for all `generate_jni()` and `generate_jar_jni()`
  targets.
- `generate_jni()` targets should be in the same `BUILD.gn` as the
  `android_library()` that owns the sources.
- Ensure that all `android_library()` targets that have a `.java` file that is
  listed in a `generate_jni()` target also contain a `deps` entry onto the
  `foo_jni_java` target.

## Guidance for C++

Generated headers:

- Are generated at
  `{OUTPUT_DIR}/gen/jni_headers/{BUILD_GN_SUBDIR}/{TARGET_NAME}/{JavaClass}_jni.h`
- Omit the `gen/jni_headers` prefix when #include'ing them.
- #includes of `_jni.h` headers from `.cc` files should come after all other
  #includes and be preceded with the comment:
  - `// Must come after headers that provide symbols used by @JniType.`
- #includes of `_jni.h` headers from `.h` files should always use the
  `_shared_jni.h` variants.

API Usage:

- When returning or passing a null `JavaRef`, use `nullptr` rather than calling
  a constructor.
- Prefer `JArray<jobject>` over `jobjectArray`, as well as for other array types
  (`JArray<*>` over `j*Array`)
- Use the `jni_zero::` namespace instead of the `base::android::` aliases (e.g.
  for `*JavaRef`, and `AttachBaseContext()`)
- To call static Java methods, do not call the `Java_Clazz_method()` functions
  directly. Call them through their typed wrappers: `ClazzJni::method()`
- To call constructors methods, use `ClazzJni::New()`
- To call member functions given a `JavaRef<jobject>`, use `Java_Clazz_method()`
- To call member functions given a `JavaRef<JFoo>`, use `foo->method()`

Defining native entry point methods:

- Use the signatures from the examples within the generated `_jni.h` files
- Omit the `JNIEnv*` parameter if it will be unused.

## Guidance for Java

- Name all @NativeMethods interfaces "Natives", and make them package-private.
- Use `@JniType()` on as many parameters and return types as possible.
- Parameters and return types that may be `null` must be annotated as
  `@Nullable` to avoid a runtime null-check.
- A Java class owns a native object if it creates it via JNI.
- A native object owns a Java class if it creates it via JNI.
- If a Java class owns a native object, ensure that there is an `onDestroy()`
  method that calls `delete` via JNI, and sets the field `0`.
- If a native object owns a Java object, ensure that its destructor sets the
  Java object's native pointer field to 0 from its destructor.

## Guidance for @JniType

- Search all .h files for `FromJniType` or `ToJniType` to discover which types
  are currently supported.
- Strings are either `std::string` or `std::u16string`.
- Use `std::optional` for `@Nullable` types except when the type itself contains
  a null-state (e.g. GURL, Callback, Strings where "" is not a valid value).
- Prefer to use `List<>` over typed object arrays.

## Converting From jobject to Correctly-Typed jobject Subclasses

### Core Steps

#### 1. Type Conversion

- Change `JavaRef<jobject>` and `ScopedJavaLocalRef<jobject>` to use generated
  classes that extend `jobject`.
- E.g., `JavaRef<jobject>` for a `FooBar` class becomes `JavaRef<JFooBar>`.
- **Special Cases**:
  - `java.lang.Object` maps to `jobject`
  - `java.lang.String` maps to `jstring`
  - `java.lang.Throwable` maps to `jthrowable`
  - `java.lang.Class` maps to `jclass`
- Use `.As<JFoo>()` to convert from `JavaRef<jobject>` to a specific generated
  type.
- Convert `ScopedJavaLocalRef<jobject>()` (empty constructor) to `nullptr`.

#### 2. Method Calls

- **Instance Methods**: Replace `Java_MyClass_method(env, ref, ...)` with
  `ref->method(env, ...)`.
- **Static Methods**: Replace `Java_MyClass_method(env, ...)` with
  `JMyClassClass::method(env, ...)`.
- Methods taking `java.lang.Object` will take `jobject`. If you have a
  `JavaRef<jobject>`, you can pass it directly.

#### 3. Namespaces and Aliases

- JNI types often have long namespaces (e.g., `::org::chromium::base::JFoo`).
- If a `using` statement isn't already in the `_jni.h` file, add an alias at the
  top of the `.cc` file.
- E.g., `using ::org::chromium::base::JLogo;`.
- Avoid fully-qualified names in method signatures within `.cc` files when an
  alias is available.

#### 4. Headers and Dependencies

- If a type `JFoo` is used in a `.h` file:
  - Include `foo_shared_jni.h`.
  - In the `BUILD.gn`, add a `public_dep` onto the `generate_jni()` target that
    produces it.
- Do NOT include `_shared_jni.h` in `.cc` files; use regular `_jni.h` headers
  there.
- For system classes (`List`, `Set`, `Map`):
  - Add a dependency on `//third_party/jni_zero:system_jni`.
  - Use generated headers from that target.

### Examples

See [examples.md](references/jobject_subclass_examples.md) for concrete
before/after snippets.

### Validation

Build all affected `.cc` and `.java` files using `autoninja`:

```bash
autoninja -C {OUTPUT_DIR} ../../path/to/foo.cc^ ../../path/to/Foo.java^
```

*Note: Use the specified `OUTPUT_DIR` (typically `out/Default` or `out/Debug`).*
