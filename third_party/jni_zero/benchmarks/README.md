# Benchmark tl;dr:

1. The time for JNI calls is negligible, comparable to a main memory load so
   unless you are doing JNI in a big loop, you can ignore its cost.
1. `AttachCurrentThread()` costs the same as a trivial JNI call.
1. Sending a few parameters (< 50bytes) in a JNI call might only add 25-50%
   extra latency.
1. For sending a lot of data, primitive arrays are the most performant way,
   even if java has to convert a List to array + unbox first.
1. If you need to traverse in java vs copying it over, traversing Java arrays is
   2x faster than doing the same for Java Lists but it is still much faster to
   convert all the way to c++ vector.
1. Converting a java List to a c++ vector before iterating is ~10x faster than
   traversing the java List directly and 5x faster than traversing a java array
   directly (for arrays ~= 10,000 long). This includes the time for type
   conversion.
1. Direct access to primitive arrays is incredibly fast (eg: `ByteArrayView` or
   direct JNI API calls from `<jni.h>`).
1. non-ASCII utf-16 strings are the fastest to convert to java strings, followed
   by ASCII strings in general and finally the slowest is non-ASCII utf-16 to utf-8
   conversion (because optimizations).
1. Integer boxing and unboxing is could be expensive if done through JNI
   and very cheap when done on java's side.


# How to run the benchmarks

1. Add a dep from a java target onto `//third_party/jni_zero/benchmarks:benchmark_java`.
1. Add a dep from a native target onto `//third_party/jni_zero/benchmarks:benchmark_native_side`.
1. Add a call from java to `org.jni_zero.benchmark.Benchmark.runBenchmark()`.

## How to run the generated classes benchmark

The generated classes benchmark uses a large number of generated classes and thus they are not committed into the repo.

1. Run the script `generated/generate.py` and it will create the generated files in its own directory.
1. Set `_enable_generated_benchmark = true` in `BUILD.gn`.
1. Add a call to `BenchmarkJni.get().runGeneratedClassesBenchmark()` from `org.jni_zero.benchmark.Benchmark.runBenchmark()`

# Benchmark Detailed Results:

The numbers here are not exact since its hard to control for things like garbage
collection and how busy was the phone was at the time. Trivial benchmarks show the
most variance.

## Trivial calls without parameters or return values
### Java -> C++

```java
// Java
BenchmarkJni.get().callMe();
```

```c++
// C++
static void JNI_Benchmark_CallMe(JNIEnv* env) {}
```
&nbsp;

|                | Pixel 7A    | Samsung Galaxy A13 |
| -------------- | :---------: | :----------------: |
| Time per Call  | 30 ns      |  130 ns             |


### C++ -> Java

```c++
// C++
Java_Benchmark_callMe(env);
```

```java
// Java
@CalledByNative
static void callMe() {}
```
&nbsp;

|                | Pixel 7A    | Samsung Galaxy A13 |
| -------------- | :---------: | :----------------: |
| Time per Call  | 50 ns      |  380 ns             |

### AttachCurrentThread()

```c++
// C++
AttachCurrentThread();
```
&nbsp;

|                | Pixel 7A    | Samsung Galaxy A13 |
| -------------- | :---------: | :----------------: |
| Time per Call  | 80 ns      |  360 ns             |

## Sending Primitive containers (primitive arrays or collections with autoboxed primitives).

### Java int[10000] -> C++ jintArray and reading the jintArray's element's memory directly via GetIntArrayElements JNI API (vs copying it to a vector).

This is the fastest possible way of sending data since there is no conversion needed and no extra copies into a vector.
```java
// Java
int[] intArray = new int[10000];
BenchmarkJni.get().sendLargeIntArray(intArray);
```
```c++
// C++
void JNI_Benchmark_SendLargeIntArray(
    JNIEnv* env,
    const jni_zero::JavaParamRef<jintArray>& j_array) {
  size_t array_size = static_cast<size_t>(env->GetArrayLength(j_array.obj()));
  jint* array = env->GetIntArrayElements(j_array.obj(), nullptr);
  for (size_t i = 0; i < array_size; i++) {
    count += array[i];
  }
  env->ReleaseIntArrayElements(j_array.obj(), array, 0);
}
```
&nbsp;

|                           | Pixel 7A    | Samsung Galaxy A13 |
| --------------            | :---------: | :----------------: |
| Time per 10000 int array  | 19,000 ns   |  23,000 ns            |
| Time per int (amortized)  | 1.9 ns      |  2.3 ns             |


### Java int[10000] -> C++ std::vector<int> using @JniType conversions.
```java
// Java
int[] intArray = new int[10000];
BenchmarkJni.get().sendLargeIntArrayConverted(intArray);
```
```c++
// C++
void JNI_Benchmark_SendLargeIntArrayConverted(
    JNIEnv* env,
    std::vector<int32_t>& array) {
  for (size_t i = 0; i < array.size(); i++) {
    count += array[i];
  }
}
```
&nbsp;

|                           | Pixel 7A    | Samsung Galaxy A13 |
| --------------            | :---------: | :----------------: |
| Time per 10000 int array  | 27,000 ns   |  66,000 ns          |
| Time per int (amortized)  | 2.7 ns      |  6.6 ns             |


### C++ std::vector<int>(10000) -> Java int[] using @JniType conversions.

```c++
// C++
std::vector<int> array(10000);
Java_Benchmark_receiveLargeIntArray(env, array);
```

```java
// Java
@CalledByNative
static void receiveLargeIntArray(@JniType("std::vector<int32_t>") int[] array) {
    for (int i = 0; i < array.length; i++) {
        count += array[i];
    }
}
```
&nbsp;

|                           | Pixel 7A    | Samsung Galaxy A13 |
| --------------            | :---------: | :----------------: |
| Time per 10000 int array  | 42,700 ns   |  150,000 ns          |
| Time per int (amortized)  | 4.3 ns      |  15 ns             |


### Converting an ArrayList<Integer>(10000) to an int[] via stream().mapToInt() and then to an std::vector<int> (all conversion time counted).


```java
// Java
List<Integer> integerList = new ArrayList(10000);
int[] streamedIntArray =
        integerList.stream().mapToInt((integer) -> integer.intValue()).toArray();
BenchmarkJni.get().sendLargeIntArrayConverted(streamedIntArray);
```

```c++
// C++
static void JNI_Benchmark_SendLargeIntArrayConverted(
    JNIEnv* env,
    std::vector<int32_t>& array) {
  for (size_t i = 0; i < array.size(); i++) {
    count += array[i];
  }
}
```
&nbsp;

|                               | Pixel 7A      | Samsung Galaxy A13 |
| --------------                | :---------:   | :----------------: |
| Time per 10000 Integer List   | 145,000 ns    |  1,200,000 ns       |
| Time per Integer (amortized)  | 14.5 ns       |  120 ns             |


### Traversing a Java Integer[10000] array from C++ using GetObjectArrayElements and doing the unboxing manually.


```java
// Java
Integer[] integerArray = new Integer[10000];
BenchmarkJni.get().sendLargeObjectArray(integerArray);
```

```c++
// C++
void JNI_Benchmark_SendLargeObjectArray(
    JNIEnv* env,
    const JavaParamRef<jobjectArray>& j_array) {
  size_t array_size = static_cast<size_t>(env->GetArrayLength(j_array.obj()));
  for (size_t i = 0; i < array_size; i++) {
    count += JNI_Integer::Java_Integer_intValue(
        env, JavaParamRef(env, env->GetObjectArrayElement(j_array.obj(), i)));
  }
}
```
&nbsp;

|                               | Pixel 7A      | Samsung Galaxy A13 |
| --------------                | :---------:   | :----------------: |
| Time per 10000 Integer array  | 800,000 ns   |  6,000,000 ns       |
| Time per Integer (amortized)  | 80 ns        |  600 ns             |


### Traversing a Java List<Integer>(10000) from C++ using List.get() and doing the unboxing manually.

```java
// Java
List<Integer> integerList = new ArrayList(10000);
BenchmarkJni.get().sendLargeObjectList(integerList);
```

```c++
// C++
static void JNI_Benchmark_SendLargeObjectList(
    JNIEnv* env,
    const JavaParamRef<jobject>& j_list) {
  size_t array_size = static_cast<size_t>(CollectionSize(env, j_list));
  for (size_t i = 0; i < array_size; i++) {
    count += JNI_Integer::Java_Integer_intValue(env, ListGet(env, j_list, i));
  }
}
```
&nbsp;

|                               | Pixel 7A      | Samsung Galaxy A13 |
| --------------                | :---------:   | :----------------: |
| Time per 10000 Integer List   | 1,500,000 ns  |  11,700,000 ns       |
| Time per Integer (amortized)  | 150 ns        |  1170 ns             |


## Sending naked integers as parameters (not in a container). {.numbered}

### Sending 10000 ints from Java -> C++ one at a time (each call sends a single int as a parameter).


```java
// Java
for (int i = 0; i < 10000; i++) {
    BenchmarkJni.get().sendSingleInt(i);
}
```

```c++
// C++
static void JNI_Benchmark_SendSingleInt(JNIEnv* env, jint param) {
  count += param;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10000 ints       | 300,000 ns   |  1,400,000 ns       |
| Time per int              | 40 ns        |  140 ns             |


### Sending 10000 ints from C++ -> Java one at a time (each call sends a single int as a parameter).


```c++
// C++
for (int i = 0; i < 10000; i++) {
  Java_Benchmark_receiveSingleInt(env, i);
}
```

```java
// Java
@CalledByNative
static void receiveSingleInt(int param) {
    count += param;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10000 ints       | 452,000 ns    |  3,400,000 ns       |
| Time per int              | 45.2 ns       |  340 ns             |

### Sending 100000 ints 10 at a time from Java -> C++

```java
// Java
for (int i = 0; i < 1000; i++) {
    BenchmarkJni.get()
            .send10Ints(i, i, i, i, i, i, i, i, i, i);
}
```

```c++
// C++
static void JNI_Benchmark_Send10Ints(JNIEnv* env,
                                     jint a,
                                     jint b,
                                     jint c,
                                     jint d,
                                     jint e,
                                     jint f,
                                     jint g,
                                     jint h,
                                     jint i,
                                     jint j) {
  count += a + b + c + d + e + f + g + h + i + j;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10 ints          | 60 ns         | 170 ns       |
| Time per int              | 6 ns          | 17 ns             |


### Sending 100000 ints 10 at a time from C++ -> Java

```c++
// C++
for (int i = 0; i < 10000; i++) {
  Java_Benchmark_receive10Ints(env, i, i, i, i, i, i, i, i, i, i);
}
```

```java
// Java
@CalledByNative
static void receive10Ints(
        int a, int b, int c, int d, int e, int f, int g, int h, int i, int j) {
    count += a + b + c + d + e + f + g + h + i + j;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10 ints          | 100 ns        | 550 ns            |
| Time per int              | 10 ns         | 55 ns             |

### Sending 10000 Integers from Java -> C++ ints converted using @JniType one at a time (each call sends a single Integer as a parameter).

```java
// Java
for (int i = 0; i < 10000; i++) {
    BenchmarkJni.get().sendSingleInteger(i);
}
```

```c++
// C++
static void JNI_Benchmark_SendSingleInteger(
    JNIEnv* env,
    const JavaParamRef<jobject>& param) {
  count += JNI_Integer::Java_Integer_intValue(env, param);
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10000 Integers   | 1,100,000 ns   | 6,500,000 ns       |
| Time per Integer          | 110 ns         | 650 ns             |


### Sending 10000 ints from C++ -> Java Integers converted using @JniType one at a time (each call sends a single int as a parameter).


```c++
// C++
for (int i = 0; i < 10000; i++) {
  Java_Benchmark_receiveSingleInteger(env, i);
}
```

```java
// Java
@CalledByNative
static void receiveSingleInteger(@JniType("int32_t") Integer param) {
    count += param;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10000 Integers   | 1,500,000 ns  | 10,000,000 ns       |
| Time per Integer          | 150 ns        | 1000 ns             |


### Sending 100000 Integers 10 at a time from Java -> C++

```java
// Java
Integer a = 1;
for (int k = 0; k < 10000; k++) {
    BenchmarkJni.get().send10Integers(a, a, a, a, a, a, a, a, a, a);
}
```

```c++
// C++
static void JNI_Benchmark_Send10Integers(JNIEnv* env,
                                         const JavaParamRef<jobject>& a,
                                         const JavaParamRef<jobject>& b,
                                         const JavaParamRef<jobject>& c,
                                         const JavaParamRef<jobject>& d,
                                         const JavaParamRef<jobject>& e,
                                         const JavaParamRef<jobject>& f,
                                         const JavaParamRef<jobject>& g,
                                         const JavaParamRef<jobject>& h,
                                         const JavaParamRef<jobject>& i,
                                         const JavaParamRef<jobject>& j) {
  count += JNI_Integer::Java_Integer_intValue(env, a);
  count += JNI_Integer::Java_Integer_intValue(env, b);
  count += JNI_Integer::Java_Integer_intValue(env, c);
  count += JNI_Integer::Java_Integer_intValue(env, d);
  count += JNI_Integer::Java_Integer_intValue(env, e);
  count += JNI_Integer::Java_Integer_intValue(env, f);
  count += JNI_Integer::Java_Integer_intValue(env, g);
  count += JNI_Integer::Java_Integer_intValue(env, h);
  count += JNI_Integer::Java_Integer_intValue(env, i);
  count += JNI_Integer::Java_Integer_intValue(env, j);
}
```
&nbsp;

|                               | Pixel 7A      | Samsung Galaxy A13 |
| --------------                | :---------:   | :----------------: |
| Time per 10 Integers          | 800 ns        | 4,500 ns       |
| Time per Integer              | 80 ns         | 450 ns             |


### Sending 100000 Integers 10 at a time from C++ -> Java

```c++
// C++
for (int i = 0; i < 10000; i++) {
  Java_Benchmark_receive10IntegersConverted(
      env, i, i, i, i, i, i, i, i, i, i);
}
```

```java
// Java
@CalledByNative
static void receive10IntegersConverted(
        @JniType("int") Integer a,
        @JniType("int") Integer b,
        @JniType("int") Integer c,
        @JniType("int") Integer d,
        @JniType("int") Integer e,
        @JniType("int") Integer f,
        @JniType("int") Integer g,
        @JniType("int") Integer h,
        @JniType("int") Integer i,
        @JniType("int") Integer j) {
    count += a + b + c + d + e + f + g + h + i + j;
}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 10 Integers      | 1400 ns       | 7,000 ns           |
| Time per Integer          | 140 ns        | 700 ns             |


## Sending Strings

```java
// Java Strings init.
StringBuilder sb = new StringBuilder();
for (int i = 0; i < 1000; i++) {
    sb.append('a');
}
String asciiString = sb.toString();
sb = new StringBuilder();
for (int i = 0; i < 1000; i++) {
    sb.append('ق');
}
String nonAsciiString = sb.toString();
```
```c++
// C++ strings init.
std::string u8_ascii_string = "";
std::string u8_non_ascii_string = "";
std::u16string u16_ascii_string = u"";
std::u16string u16_non_ascii_string = u"";
for (int i = 0; i < 1000; i++) {
  u8_ascii_string += "a";
  u8_non_ascii_string += "ق";
  u16_ascii_string += u"a";
  u16_non_ascii_string += u"ق";
}
```

### Sending a 1000 long ASCII String from Java to C++ std::string

```java
// Java
BenchmarkJni.get().sendAsciiStringConvertedToU8(asciiString);
```
```c++
// C++
static void JNI_Benchmark_SendAsciiStringConvertedToU8(JNIEnv* env,
                                                       std::string& param) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 1000 ns        | 4000 ns           |
| Time per character        | 1 ns           | 4 ns             |

### Sending a 1000 long ASCII String from Java to C++ std::u16string

```java
// Java
BenchmarkJni.get().sendAsciiStringConvertedToU16(asciiString);
```
```c++
// C++
static void JNI_Benchmark_SendAsciiStringConvertedToU16(JNIEnv* env,
                                                        std::u16string& param) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 600 ns        | 1700 ns           |
| Time per character        | 0.6 ns        | 1.7 ns             |

### Sending a 1000 long non-ASCII String from Java to C++ std::string

```java
// Java
BenchmarkJni.get().sendNonAsciiStringConvertedToU8(nonAsciiString);
```
```c++
static void JNI_Benchmark_SendNonAsciiStringConvertedToU8(JNIEnv* env,
                                                          std::string& param) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 4000 ns      | 16,000 ns           |
| Time per character        | 4 ns         | 16 ns             |

### Sending a 1000 long non-ASCII String from Java to C++ std::u16string

```java
// Java
BenchmarkJni.get().sendNonAsciiStringConvertedToU16(nonAsciiString);
```
```c++
static void JNI_Benchmark_SendNonAsciiStringConvertedToU16(
    JNIEnv* env,
    std::u16string& param) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 200 ns        | 1400 ns             |
| Time per character        | 0.2 ns        |  1.4 ns            |

### Sending a 1000 long ASCII std::string from C++ to Java

```c++
// C++
Java_Benchmark_receiveU8String(env, u8_ascii_string);
```
```java
// Java
@CalledByNative
static void receiveU8String(@JniType("std::string") String s) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 3000 ns       | 11,500 ns          |
| Time per character        | 3 ns          | 11.5 ns            |

### Sending a 1000 long ASCII std::u16string from C++ to Java

```c++
// C++
Java_Benchmark_receiveU16String(env, u16_ascii_string);
```
```java
// Java
@CalledByNative
static void receiveU16String(@JniType("std::u16string") String s) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 2000 ns       | 8,400 ns          |
| Time per character        | 2 ns          | 8.4 ns            |

### Sending a 1000 long non-ASCII std::string from C++ to Java

```c++
// C++
Java_Benchmark_receiveU8String(env, u8_non_ascii_string);
```
```java
// Java
@CalledByNative
static void receiveU8String(@JniType("std::string") String s) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 5500 ns       | 25,000 ns          |
| Time per character        | 5.5 ns        | 25 ns            |

### Sending a 1000 long non-ASCII std::u16string from C++ to Java

```c++
// C++
Java_Benchmark_receiveU16String(env, u16_non_ascii_string);
```
```java
// Java
@CalledByNative
static void receiveU16String(@JniType("std::u16string") String s) {}
```
&nbsp;

|                           | Pixel 7A      | Samsung Galaxy A13 |
| --------------            | :---------:   | :----------------: |
| Time per 1000 characters  | 1000 ns       | 3,200 ns          |
| Time per character        | 1 ns          | 3.2 ns            |