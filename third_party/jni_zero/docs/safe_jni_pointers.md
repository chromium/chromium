# Safe JNI Pointers

**Bug**:
[Mitigate security risks due to C++ pointers in Java represented by a \`long\`.](https://crbug.com/40794873)

Building upon the exploration and discussions in
*[Protecting JNI C++ Pointers](https://docs.google.com/document/u/0/d/1Vr-3yHVpEoYqOgGvBAyLO3MzL3JfNqorpW_1relfvGg/edit?resourcekey=0-cZ3jb2zumEzkyEUVulGXrA)*,
this document distills the proposed ideas into a concrete solution. The design
presented here was selected for offering the most robust safety guarantees with
the lightest possible performance footprint. We also value ergonomics for
fail-safe and developer productivity.

# Introduction & Problem Statement

Currently, C++ pointers are passed across the JNI boundary as raw long values.
While efficient, this practice bypasses the security protections offered by
smart pointers like raw_ptr. If a Java object holds onto a long representing a
native pointer, and that native pointer is later freed, the Java object now
holds a dangling pointer. If this long is passed back to C++ and
reinterpret_casted, it can be used to exploit a Use-after-Free (UaF)
vulnerability.

This document explores solutions to ensure that C++ pointers exposed to or
stored in Java are properly managed.

# Background

**raw_ptr (MiraclePtr)**

raw_ptr (often referred to as MiraclePtr in Chromium context) is a UaF
mitigation technology. It replaces raw C++ pointers with a raw_ptr template
class. When the underlying object T is freed, its memory is not immediately
returned to the allocator (quarantine). This is managed by a reference count.

There are low-level APIs to manage reference counts directly.

**The JNI Problem**

The core problem is that a long in Java is not a smart pointer. When a native
pointer is converted to a long, nothing tracks this reference.

# Goals and Non-Goals

**Goals**

- **Prevent UaF vulnerabilities.**
- The solution should be as automated and error-proof as possible for
  developers.
- Provide clear guidance for different pointer ownership patterns across the JNI
  boundary.
- **Minimal Performance overhead:** Minimize JNI boundary overhead by avoiding
  Java object allocation on the native side.

**Non-Goals**

- This proposal does not aim to refactor all existing JNI bindings at once.
- Support shared references between threads (Thread confinement is assumed).

# Analysis of Ownership Models

We have three categories of ownership patterns regarding C++ pointers passed to
Java.

1. **Owned pointer**. Java owns a C++ object. When done with the object, Java
   must destruct the object.
2. **Long-borrowed pointer**. C++ owns a C++ object, and Java borrows its
   pointer. Java is responsible for forgetting the pointer before C++ destructs
   the referenced object.
3. **Short-borrowed pointer**. C++ owns a C++ object, and Java borrows its
   pointer in a JNI function call. The lifetime of the pointer is bound to the
   JNI function call.

We observe all of these categories in the wild.

# Proposed Solution: Ownership Models & Pointer Classes

We propose a robust pointer management system where **all pointer exchanges
happen as primitive jlong values**, preventing overhead from JNI object
construction on the native side. The safety and ownership semantics are enforced
by distinct wrapper types and automated code generation logic.

## Overview of Ownership Models

To ensure safety without sacrificing performance, we explicitly categorize JNI
pointer usage into three **Ownership Models** and assign a specific Java wrapper
class to each.

- **Owned Pointer (UniquePtr)**: Represents a case where Java takes full
  ownership of the native object. Java is responsible for explicitly destroying
  the object when it is no longer needed.
- **Long-borrowed Pointer (RawPtr)**: Represents a case where Java holds a
  long-lived reference to a native object, but does *not* own it. The native
  object's lifecycle is managed elsewhere (e.g., by C++), but Java holds a safe
  handle (raw_ptr) to it.
- **Short-borrowed Pointer (Ptr)**: Represents a temporary "borrow" of a pointer
  for the duration of a single JNI method call. This is used for standard
  pass-through arguments.

## Summary Table

**From C++ to Java**

| Ownership Model       | Java Class   | C++ Type     | Description                                                                                                         | Responsibility                                                     | Count |
| :-------------------- | :----------- | :----------- | :------------------------------------------------------------------------------------------------------------------ | :----------------------------------------------------------------- | :---- |
| **1. Owned Pointer**  | JniUniquePtr | JniUniquePtr | Java takes full ownership of the native object T.                                                                   | Java **must** explicitly destroy the object.                       | 135   |
| **2. Long-borrowed**  | JniRawPtr    | JniRawPtr    | Java holds a reference to a native object owned by C++ (or elsewhere). The reference persists across JNI calls.     | Java manages the *reference* lifecycle, C++ manages T's lifecycle. | 278   |
| **3. Short-borrowed** | JniPtr       | T\*          | Java borrows a pointer only for the duration of a single JNI call. **Cannot be used as a return type from Native.** | Automatically invalidated after the function returns.              | 18    |

**From Java to C++**

| Ownership Model  | Java Class | C++ Type | Description                      | Responsibility |
| :--------------- | :--------- | :------- | :------------------------------- | :------------- |
| **Short-borrow** | JniPtr     | T\*      | C++ borrows a pointer from Java. | N/A            |

- Both UniquePtr and RawPtr inherit (implement) Ptr.
- We *could* support passing ownership from Java to C++, but no such pattern was
  found in the wild.

## Model 1: Owned Pointer (UniquePtr)

This model applies when Java is responsible for the lifecycle of the native
object. This is analogous to std::unique_ptr in C++.

- **Behavior:** C++ passes a heap pointer to Java. Java takes ownership of the
  underlying object T.
- **Example:** Java instantiates a native object and destroys it later.

**C++ Implementation:**

MakeUnique (helper defined in Detailed Implementation) creates T, and holds its
pointer. (It doesn't create a std::unique_ptr, because it's meaningless now that
the ownership is transferred to Java.)

```c
jni_zero::JniUniquePtr<Foo> Foo_create(JNIEnv* env) {
 // Constructs Foo and transfers the ownership to Java.
 return jni_zero::MakeUnique<Foo>();
}

// No explicit Foo_destroy needed in C++ API; handled by UniquePtr logic.
```

**Java Implementation:**

The UniquePtr class ensures that when destroy() is called, the underlying native
object is deleted.

As a measure to prevent memory leaks, on debug builds, we use
[LifetimeAssert](https://source.chromium.org/chromium/chromium/src/+/main:base/android/java/src/org/chromium/base/lifetime/LifetimeAssert.java)
to cause a crash when it's GCed without being destroyed. See implementation
details. Same for RawPtr.

```java
// Java
class Foo {
 // Strongly typed ownership
 private JniUniquePtr<NativeFoo> mHandle;

 public Foo() {
  // Receives ownership
  mHandle = FooJni.get().create();
 }

 public void destroy() {
  // Destroys the native object Foo
  // If we forget to call it, chrome crashes when mHandle is GCed on debug builds.
  mHandle.destroy();
 }
}
```

## Model 2: Long-borrowed Pointer (RawPtr)

This model applies when Java needs to hold a reference to a native object for an
extended period, but does *not* own the object. The native object's lifecycle is
managed by C++.

- **Behavior:** C++ passes a pointer incrementing the object ref count. Java
  creates a RawPtr object holding the T\* that is cast to `long`.
- **Safety:** non-zero refcount protects against UaF (via MiraclePtr mechanism).
- **Cleanup:** Java destroys RawPtr, which causes a JNI call to decrement the
  refcount for T\*.

Example: Native object owns the Java object

Native owns the Java object, but passes a pointer to itself so Java can call
back.

```c
class Foo {
public:
 Foo() {
  // Create RawPtr and pass to Java.
  // C++ does NOT keep a ref to the Foo*, only the Java object itself.
  j_foo_ = Java_Foo_Constructor(jni_zero::JniRawPtr<Foo>::Create(this));
 }
 // ...
};
```

Java obtains a handle to an existing C++ object.

```java
// Java
class Foo {
 private JniRawPtr<NativeFoo> mFooHandle;

 void onFinished() {
  // We are done with the reference.
  // destroy() decrements the refcount of Foo*, but Foo remains alive.
  // If we forget to call it, chrome crashes when mFooHandle is GCed on debug builds.
  if (mFooHandle != null) {
   mFooHandle.destroy();
   mFooHandle = null;
  }
 }
}
```

## Model 3: Short-borrowed Pointer (Ptr)

This model is for pointers that are valid only for the duration of a single JNI
method call. This is the most common "pass-through" scenario.

- **Behavior:** The pointer is passed to Java. The generated code automatically
  wraps it in a scope.
- **Safety:** Ptr is valid only within the call. Trying to store it or access it
  later results in an error.
- **Example:** Native pointers flow through Java.

**C++ Source:**

```c
void CallsJava(Foo* foo) {
 Java_MyClass_onEvent(foo);
}
```

**Java Source:**

```java
@CalledByNative
void onEvent(JniPtr<NativeFoo> ref) {
 // Safe to use within this method.
 // If we capture 'ref' in a closure/field, it will throw an error when accessed later.
 doSomething(ref);
}
```

**Generated Glue Code (The Magic):**

Because the parameter type is Ptr, JNI Zero generates a cleanup block
automatically.

```java
public static void onEvent(Object obj, long nativePtr) {
 // 1. Wrap (Lightweight, stack-like allocation)
 // JniPtrImpl is the internal implementation of Ptr (details below)
 JniPtrImpl ref = new JniPtrImpl(nativePtr);
 try {
  // 2. Invoke
  ((MyClass)obj).onEvent(ref);
 } finally {
  // 3. Auto-Destroy (Enforced by Ptr contract)
  // This invalidates the Java reference, preventing future access.
  ref.release();
 }
}
```

# Architectural Changes for C++ → Java Calls

To maintain zero-overhead on the native side, C++ always passes pointers as
primitive jlong values. However, Java methods expect typed wrapper objects
(JniPtr\<T>, JniRawPtr\<T>, JniUniquePtr\<T>).

To bridge this gap, JNI Zero generates an **intermediate static Java wrapper
(glue code)** for all @CalledByNative methods that use Safe JNI Pointers.\
**We do this in phase 2 of the implementation. In phase 1, we don't generate the
glue code.** See [roll out strategy]#implementation--rollout-plan for details.

**The Flow:**

1. **C++ Side:** Calls the generated static wrapper, passing the pointer as a
   jlong.
2. **Java Glue Code:**
   - Instantiates the appropriate Java wrapper object (e.g., new
     JniRawPtrImpl\<>(handle)).
   - For Ptr (Short-borrowed), wraps the call in a try-finally block to ensure
     destroy() is called.
   - Calls the original user-defined method with the typed object.

**Comparison:**

| Feature       | Traditional JNI Zero                         | Safe JNI Pointers                                  |
| :------------ | :------------------------------------------- | :------------------------------------------------- |
| **C++ Calls** | Direct JNI Reflection (e.g., CallVoidMethod) | Static Stub (e.g., CallStaticVoidMethod)           |
| **Target**    | The user's @CalledByNative method            | Generated GEN_JNI or inner class wrapper method    |
| **Arguments** | jlong pointer                                | **Also jlong** (converted to Objects in Java glue) |
| **Cleanup**   | N/A                                          | Automated via generated glue (try-finally destroy()) for short-borrow.Manual `destroy()` for long-borrow and owned pointers.                                                    |

## Visibility Restrictions (Caveat)

A consequence of the generated glue code architecture is that
**`@CalledByNative` methods using Safe Pointers can no longer be `private`.**

- **Traditional JNI:** The native runtime uses JNI reflection, which can bypass
  Java access modifiers, allowing `private` methods to be called directly.
- **Safe Pointers:** The call originates from a generated helper class (e.g.,
  `FooJni`) residing in the same package. Standard Java visibility rules apply.

> **Required Change:** Methods accepting `Ptr`, `RawPtr`, or `UniquePtr` must be
> at least **package-private**. Using `private` will result in a Java
> compile-time error.

```java
// ❌ BAD: Wrapper cannot access this
@CalledByNative
private void onEvent(JniPtr<NativeFoo> ref) { ... }

// ✅ GOOD: Package-private allows access from generated glue
@CalledByNative
void onEvent(JniPtr<NativeFoo> ref) { ... }
```

# Type Safety & Code Generation

To ensure type safety and seamless integration across build targets, we need a
robust mechanism to map Java types (e.g., JniPtr\<`NativeFoo>`) to their
corresponding C++ types (e.g., `::foo::Foo`).

### The Challenge

Currently, `jni_zero` processes files in isolation. With a straightforward
implementation, it cannot resolve `NativeFoo` to `::foo::Foo` if `NativeFoo` is
defined in a different file, creating an artificial limitation where users must
colocate type definitions or use tedious manual annotations.

### Proposed Solution: Metadata-Driven Resolution

We will use
[**GN Metadata**](https://gn.googlesource.com/gn/+/main/docs/reference.md#metadata_collection)
to propagate type information up the dependency graph. This treats JNI types
similarly to C++ headers: if you depend on the target, you can use the type
definitions.

When using JniPtr\<`NativeFoo>`, the `generate_jni` target aggregates metadata
from its dependencies. The code generator uses this "catalog" to resolve
`NativeFoo` to `::foo::Foo` and generates the correct C++ glue code.

**User Experience:** Users simply `import` the generated marker class.

```java
// Bar.java
import org.chromium.foo.Foo.NativeFoo;

void action(JniPtr<NativeFoo> handle); // Automatically maps to ::foo::Foo*
```

**Benefits:**

- **Ergonomics:** No extra annotations or paths required.
- **Robustness:** Relies on the build graph as the source of truth.
- **Phase 2 Ready:** Can be naturally extended to advanced glue code generation.

(See \[*Type parameter design discussion*\]#type-safety--code-generation for
detailed GN implementation, architecture, and alternatives considered.)

# R8 / ProGuard Implications

Because the C++ entry point shifts from the user's method to the generated glue
code, ProGuard rules must be updated to reflect this indirection.

1. **Entry Points:** The generated static glue methods become the new JNI entry
   points. The generator automatically adds them to the `-keep` rule, ensuring
   they are kept.
2. **User Methods:** The original user methods (e.g.,
   `void onEvent(JniPtr ref)`) are no longer called directly by native code.
   - From R8's perspective, these become regular Java-to-Java calls (invoked by
     the glue code).
   - **Optimization Opportunity:** R8 is technically free to rename, inline, or
     optimize the user methods, as long as the glue code can still reference
     them.
3. Future optimizations may be possible:
   - E.g. we might be able to write a custom R8 pass that converts all
     JniPtr\<`Foo>` to `long`.

# //base dependency

Since `RawPtr` uses BRP, which depends on //base, this abstraction would need to
be guarded with "build_with_chromium", and would not be usable by other
projects.

# Detailed Implementation

### Java Classes

The Java classes enforce safety through LifetimeAssert.

```java
// Best practice:
// - Use JniPtr<T> for function parameters.
// - Use JniRawPtr<T> or JniUniquePtr<T> for return values or stored fields.

// Interface for native pointers.
public interface JniPtr<T extends JniTypeToken> {
}

// We put it as package-private within jni zero, so that devs
// can't cast Ptr to JniPtrInner.

interface JniPtrInner<T extends JniTypeToken> extends JniPtr<T> {
  // Accessed by JNI generated code.
  long getNativePtr();
}

// Model 1: Owns T
public interface JniUniquePtr<T extends JniTypeToken> extends JniPtr<T> {
  void destroy();
  static <T extends JniTypeToken> JniUniquePtr<T> createForTesting(long fakePtr) {
    return new JniUniquePtrImpl<>(fakePtr, 0);
  }
}

public class JniUniquePtrImpl<T extends JniTypeToken> implements JniUniquePtr<T>, JniPtrInner<T> {
 // Guards against memory leaks (destroy() not called) in debug builds.
 private final LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
 private long mPtr;
 private long mDeleter;

 // Instantiated by generated code
 public JniUniquePtr(long ptr, long deleter) {
  mPtr = ptr;
  mDeleter = deleter; // This can be deleted in phase 2.
 }

 // Public API to release memory
 public void destroy() {
  LifetimeAssert.destroy(mLifetimeAssert);

  if (mPtr == 0) throw new RuntimeException("Pointer already destroyed (Possible double free attempt)!");

  long p = mPtr;
  mPtr = 0;
  // Calls C++ to delete the T.
  UniquePtrJni.get().delete(p, mDeleter);
 }

 @Override
 public long getNativePtr() {
  if (mPtr == 0) throw new RuntimeException("Use after free!");
  return mPtr;
 }

}

// Model 2: Owns T* (not T).
//
// Note that `RawPtr` itself is not thread-safe. If multiple
// threads access/destroy the same RawPtr object without synchronization, a data race
// will occur.
public interface JniRawPtr<T extends JniTypeToken> extends JniPtr<T> {
  void release();
  static <T extends JniTypeToken> JniRawPtr<T> createForTesting(long fakePtr) {
    return new JniRawPtrImpl<>(fakePtr);
  }
}

public class JniRawPtrImpl<T extends JniTypeToken> implements JniRawPtr<T>, JniPtrInner<T> {
 // Same lifetime assert constructs as UniquePtr are used (omitted here).

 // Points to T*
 private long mPtr;

 // Instantiated by generated code
 public JniRawPtr(long ptr) { mPtr = ptr; }

 // Public API to release memory
 public void release() {
  if (mPtr == 0) throw new RuntimeException("Use after free!");

  long p = mPtr;
  mPtr = 0;
  // Calls C++ to decrement T* refcount.
  RawPtrJni.get().release(p);
 }

 @Override
 public long getNativePtr() {
  if (mPtr == 0) throw new RuntimeException("Use after free!");
  return mPtr;
 }
}

// Model 3: Short-lived wrapper.
public class JniPtrImpl<T extends JniTypeToken> implements JniPtrInner<T> {
 private long mPtr;

 public JniPtrImpl(long ptr) { mPtr = ptr; }

 // Invalidates the reference (does NOT delete the underlying object)
 // Called automatically by generated glue code.
 public void release() {
  mPtr = 0;
 }

 @Override
 public long getNativePtr() {
  if (mPtr == 0) throw new RuntimeException("Use after free!");
  return mPtr;
 }
}
```

### C++ Helpers

```c
namespace jni_zero {

// Corresponds to java RawPtr.
template <typename T>
class JniRawPtr {
public:
 // Increment the refcount and store the ptr as is.
 static JniRawPtr Create(T* ptr) {
  #if PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)
   ptr = base::internal::RawPtrBackupRefImpl<>::WrapRawPtr(ptr);
  #endif
  return JniRawPtr(ptr);
 }
 void Destroy() {
  #if PA_BUILDFLAG(USE_RAW_PTR_BACKUP_REF_IMPL)
   base::internal::RawPtrBackupRefImpl<>::ReleaseWrappedPtr(ptr_);
  #endif
 }
private:
 JniRawPtr(T* ptr) : ptr_(ptr) {}
 T* ptr_;
};

// Deleter implementation for JniUniquePtr<T>
// We pass a pointer to a specialized DeleterBase so that CFI works.
struct DeleterBase {
 virtual void Destroy(void* ptr) = 0;
};

template <typename T>
struct TemplatedDeleter : public DeleterBase {
 void Destroy(void* ptr) override {
  delete static_cast<T*>(ptr);
 }
};

template <typename T>
jlong GetDeleterAddress() {
 static TemplatedDeleter<T> instance;
 return reinterpret_cast<jlong>(&instance);
}

// Corresponds to java UniquePtr
template <typename T>
class JniUniquePtr {
private:

 JniUniquePtr(T* ptr) : ptr_(ptr), deleter_(GetDeleterAddress<T>()) {}
 // We pass these two addresses to Java as long.
 T* ptr_;
 jlong deleter_;
};

// Helper to create a JniUniquePtr.
template <typename T, typename... Args>
JniUniquePtr<T> MakeUnique(Args&&... args) {
 T* t = new T(std::forward<Args>(args)...);
 return JniUniquePtr(t);
}

} // namespace jni_zero

```

# Case Studies

As real world examples, we have the following common cases.

- **Case A: Java owns the native object.** The Java object is responsible for
  the native object's lifecycle. It's an owned pointer.
- **Case B: Native owns the Java object.** Java retains a reference to native,
  but native controls the lifecycle. It's a long-borrowed pointer.
- **Case C: Java holds a non-owning pointer.** Java holds a reference but is not
  responsible for destruction. It's a long-borrowed pointer.
- **Case D: Native pointers flow through Java.** Native calls Java with a
  pointer, which might call back into Native, but the pointer is never stored
  long-term in Java. It's a short-borrowed pointer.

### Case A: Java owns the native object (Owned Pointer)

Java instantiates the native object and is responsible for destroying it.

**C++ Implementation:**

Use `jni_zero::MakeUnique<T>()` to create the object and transfer ownership.

```c
jni_zero::JniUniquePtr<Foo> Foo_create(JNIEnv* env) {
  // Transfers ownership to Java.
  return jni_zero::MakeUnique<Foo>();
}

// No Foo_destroy needed in C++ API; handled by UniquePtr logic.
```

**Java Implementation:**

Java receives a `JniUniquePtr<NativeFoo>`.

```java
class MyClass {
 private JniUniquePtr<NativeFoo> mHandle;

 public void init() {
  // Receives ownership
  mHandle = FooJni.get().create();
 }

 public void close() {
  // Destroys both the wrapper AND the native object Foo
  mHandle.destroy();
 }
}
```

### Case B: Native owns the Java object (Long-borrowed Pointer)

Native owns the Java object, but passes a pointer to itself so Java can call
back.

**C++ Implementation:**

C++ creates a `JniRawPtr` (which wraps `T*`) and passes it to Java.

```c
class Foo {
 public:
  Foo() {
    // Create RawPtr and pass to Java.
    // C++ does NOT keep a ref to the RawPtr, only the Java object itself.
    j_foo_ = Java_Foo_Constructor(jni_zero::JniRawPtr<Foo>::Create(this));
  }

  ~Foo() {
     // Tell Java to clean up its handle.
     Java_Foo_destroy(j_foo_);
  }

 private:
  ScopedJavaGlobalRef<jobject> j_foo_;
};
```

**Java Implementation:**

Java holds a `JniRawPtr`. It does **not** destroy the native object, but it must
destroy the `JniRawPtr` wrapper when told to do so.

```java
class Foo {
 private JniRawPtr<NativeFoo> mRawPtr;

 @CalledByNative
 Foo(JniRawPtr<NativeFoo> handle) {
  mRawPtr = handle;
 }

 @CalledByNative
 void destroy() {
  // We own the handle (wrapper), so must destroy it.
  // This decrements the Foo* refcount, but NOT Foo itself.
  if (mRawPtr != null) {
   mRawPtr.release();
   mRawPtr = null;
  }
 }
}
```

### Case C: Java holds a non-owning pointer (Long-borrowed Pointer)

Java obtains a handle to an existing C++ object but doesn't own it.

**Java Implementation:**

```java
class Bar {
 private JniRawPtr<NativeFoo> mFooHandle;

 void onFinished() {
  // We are done with the reference.
  // destroy() decrements the Foo* refcount on C++ heap,
  // but Foo remains alive.
  if (mFooHandle != null) {
   mFooHandle.release();
   mFooHandle = null;
  }
 }
}
```

### Case D: Native pointers flow through Java (Short-borrowed Pointer)

Native calls Java with a pointer, which is used only for the duration of the
call.

**C++ Implementation:**

```c
void CallsJava(Foo* foo) {
 Java_Helper_onEvent(foo);
}
```

**Java Implementation:**

Use JniPtr\<`T>` in the argument. The generated code handles cleanup.

```java
@CalledByNative
void onEvent(JniPtr<NativeFoo> ref) {
 // Safe to use within this method.
 // If we capture 'ref' in a closure/field, it will throw UaF when accessed later.
 doSomething(ref);
}
```

# Thread Safety Rationale

JniRawPtr and JniUniquePtr are **not thread-safe** and must be thread-confined
(similar to base::WeakPtr) or externally synchronized (similar to raw_ptr).

We considered introducing ThreadChecker to enforce thread confinement, but it's
not adopted because raw_ptr also doesn't enforce it.

- **Rationale:** Adding locking (Mutex) to every pointer access introduces
  unacceptable performance overhead for high-frequency JNI calls.

# Assessment

**Pros**

- **Guaranteed Safety:** Ptr mechanically enforces the destruction of the Java
  reference via generated try-finally blocks.
- **Performance:** All boundaries are crossed using jlong. No JNI object
  instantiation happens on the C++ side.
- **Clarity:** The distinction between JniPtr (borrow), JniRawPtr (long-borrow),
  and JniUniquePtr (owned) is explicit.
- **Type Safety:** Compile-time checking prevents mismatched pointer types.

**Cons**

- **Code Generation Logic:** Requires jni_zero to support complex type-based
  generation rules.
- **Performance**
  - **Creation:** Creating JniUniquePtr or JniRawPtr incurs an allocation cost
    because it instantiates objects on both the Native heap (to enable BRP
    protection) and the Java heap (for their Java counterparts)
  - **Destruction:** One JNI call is required for JniRawPtr.destroy().
    - **Mitigation:** These costs are typically paid only once per object
      lifecycle.

# Alternatives Considered

**Strict Annotation-driven Safety**

This idea attempted to enforce safety purely through annotations (e.g.,
@JniOwned, @JniBorrowed) and generated code, using raw longs everywhere to
completely avoid Java object allocation.

- *Why not selected:* Handling edge cases like "Callback capture"
  (short-borrowed) or "Non-owning pointers" (long-borrowed) became extremely
  complex. It required the generator to understand the "lifetime" of a long
  value, which is difficult to enforce statically in Java without wrapper
  objects. The wrapper object approach (JniPtr) provides a tangible handle for
  try-finally blocks to act upon.

**Others**

See
[Protecting JNI C++ Pointers](https://docs.google.com/document/u/0/d/1Vr-3yHVpEoYqOgGvBAyLO3MzL3JfNqorpW_1relfvGg/edit?resourcekey=0-cZ3jb2zumEzkyEUVulGXrA)
(internal doc) for other alternatives considered.

# Roll out strategy

We will adopt a phased approach to balance implementation complexity with
performance requirements.

**Phase 1: Foundation & Safety (No Java Glue)**

- **Goal:** Establish the Safe JNI Pointers API types (`JniUniquePtr`,
  `JniRawPtr`, `JniPtr`) and enforce UaF protection.
- **Implementation:**
  - Implement the Java wrapper classes and C++ helpers.
  - Update JNI Zero to support passing these objects, but **without** generating
    Java-side glue code.
  - C++ directly constructs the Java objects.
  - Guard support for this system behind a command-line flag so that Google3
    clients cannot yet use it.
- **Performance:** Medium overhead (C++ -> Java object creation).
- **Adoption:** Enable safe pointers on non-critical paths.

**Phase 2: Java-side Codegen**

- **Goal:** Reduce JNI boundary overhead to approach raw pointer performance.
- **Implementation:**
  - Implement Java-side glue code generation in JNI Zero.
  - Switch C++ generation to pass `jlong` to the static glue methods.
- **Performance:** Reduced overhead (Java-side object creation is faster than
  JNI `NewObject`).
- **Adoption:** Enable safe pointers in more places.

**Phase 3: Implement R8 pass to convert Ptr types to longs**

- **Goal:** Achieve ~zero overhead (there will still be overhead of having to
  destroy JniRawPtr handles)
- **Implementation:**
  - Convert all `JniPtr` fields to `long`
  - Convert all virtual invokes to static invokes that pass the long.
  - Convert all `= null` to `= 0`
  - Special case `destroy()`, which does `this.mPtr = null`, to be a static
    invoke + sibling `= 0`.

We will roll out directory by directory. Once a directory is migrated, enable a
linter to prevent new usages of raw long pointers.

# Benchmark Results

(from [dirty POC](https://crrev.com/c/7206396))

**Benchmark Results (Brya)**

1. **Amortized Performance after phase 1 (500,000 iterations)**

   **A. Java Creation from Native Handle (Java Instantiation)** *Java calls
   Native, retrieves an object handle directly in Java.*

   - `long` (Baseline): **42.8 ns**
   - `Ptr`: **90.3 ns**
   - `RawPtr`: **128.4** **ns**
   - `UniquePtr:` **132.4 ns**

   **B. Java -> Native Passing** *Java passes an existing wrapper object to a
   Native method.*

   - `long` (Baseline): **23.1 ns**
   - `Ptr`: **23.5 ns**

2. **Amortized Performance after phase 2 (500,000 iterations)**

   **A. Java Creation from Native Handle (Java Instantiation)** *Java calls
   Native, retrieves a `jlong` handle, and instantiates the wrapper object in
   Java.*

   - `long` (Baseline): **42.8 ns**
   - `Ptr`: **50.2ns**
   - `RawPtr`: **63.7 ns**
   - `UniquePtr:` **63.3ns**

   **B. Java -> Native Passing** *Java passes an existing wrapper object to a
   Native method.*

   - Same latency as before, because we already generate C++ side glue code on
     phase 1.

3. **Latency Distribution & GC Analysis (Batch size: 100)**

   - **A: RawPtr**
     - Max Latency: 637 us (vs 463 us for long)
     - p99 Latency: 17 us (vs 15 us for long)
     - p50 Latency: 5 us (vs 9 us for long)
   - **A: Ptr**
     - Max Latency: 418 us (vs 463 us for long)
     - p99 Latency: 16 us (vs 15 us for long)
     - p50 Latency: 10 us (vs 9 us for long)
   - **B: Ptr**
     - Max Latency: 418 us (vs 435 us for long)
     - p99 Latency: 15 us (vs 13 us for long)
     - p50 Latency: 9 us (vs 2 us for long)
   - **Observation:**
     - The "Stop the World" impact from GC is minimal. The increases are both
       about proportional to the average time increase.

## Summary

- **Analysis:**
  1. Creating and returning a Ptr is approximately 1.2x slower than returning a
     raw long.
  2. RawPtr and UniquePtr are approximately 1.5x slower.
- **Why:** This overhead is the expected cost of safety. It includes:
  1. **Refcount manipulation:** Increase/decrease refcount of T\* for memory
     safety for RawPtr.
  2. **Java Heap Allocation:** Instantiating the wrapper object (e.g.
     JniPtrImpl) in Java.
  3. **GC cost**: Collecting the temporary objects like JniPtr\<T>.

# Type parameter design

To ensure type safety and seamless integration across build targets, we need a
robust mechanism to map Java marker types (e.g., `JniPtr<NativeFoo>`) to their
corresponding C++ types (e.g., `::foo::Foo*`).

## Main Proposal: Public Nested Interfaces + GN Metadata

We propose defining the mapping natively in Java using a **`public` nested
interface**, and propagating this mapping via GN metadata. This requires zero
new GN templates and keeps the developer UX entirely within standard Java.

**1. Definition (Single Source of Truth):** Instead of introducing new GN
templates (like `java_cpp_type`) or scanning C++ headers, the developer
explicitly defines the marker inside the Java class that logically owns it.

```java
// Foo.java
package org.chromium.foo;

import org.jni_zero.JniTypeToken;
import org.jni_zero.JniType;

public class Foo {
    // Explicitly maps the Java token to the C++ type.
    // This flawlessly handles generic types like "std::vector<char>"!
    @JniType("::foo::Foo")
    public interface NativeFoo extends JniTypeToken {}

    @CalledByNative
    void onEvent(JniPtr<NativeFoo> ptr) { ... }
}
```

**2. Usage across files:** Because it is a standard `public` interface, other
files can safely import and pass it around.

```java
// Bar.java
import org.chromium.foo.Foo.NativeFoo;

class Bar {
    @CalledByNative
    void process(JniPtr<NativeFoo> ptr) { ... }
}
```

### Implementation Details (GN Metadata Integration)

To allow `jni_zero` to resolve `NativeFoo` to `::foo::Foo` during the isolated
parsing of downstream files (like `Bar.java`), we piggyback on the existing
`generate_jni` GN template:

1. **Extraction:** When `generate_jni` processes `Foo.java`, JNI Zero parses the
   `@JniType` annotation and outputs a temporary JSON catalog (e.g.,
   `{"org.chromium.foo.Foo$NativeFoo": "::foo::Foo"}`).
2. **GN Metadata Emission:** The `generate_jni` template emits this JSON file
   path via GN `metadata` (`jni_type_files = [...]`).
3. **Propagation & Resolution:** When `Bar.java`'s target adds a regular GN
   `deps` on `Foo.java`'s target, GN automatically aggregates the metadata
   catalog. `jni_zero` uses this catalog to correctly resolve the C++ type
   during isolated parsing.

This approach requires **zero new GN boilerplate**, keeps IDEs happy (since the
file physically exists), and crucially, establishes a strict **Single Owner**
for Phase 2 optimizations.
