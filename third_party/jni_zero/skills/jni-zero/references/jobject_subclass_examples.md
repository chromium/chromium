# JNI Zero Refactoring Examples

## 1. Instance Methods

**Before:**

```cpp
void TokenAndroid::FromJavaToken(JNIEnv* env, const JavaRef<jobject>& j_token) {
  uint64_t high = Java_TokenBase_getHighForSerialization(env, j_token);
}
```

**After:**

```cpp
// In .h
static base::Token FromJavaToken(JNIEnv* env, const JavaRef<JTokenBase>& j_token);

// In .cc
...
base::Token TokenAndroid::FromJavaToken(JNIEnv* env, const JavaRef<JTokenBase>& j_token) {
  uint64_t high = static_cast<uint64_t>(j_token->getHighForSerialization(env));
}
```

## 2. Static Methods

**Before:**

```cpp
void ApplicationStatusListener::NotifyApplicationStateChange(ApplicationState state) {
  Java_ApplicationStatus_registerThreadSafeNativeApplicationStateListener(AttachCurrentThread());
}
```

**After:**

```cpp
// In .cc
...
void ApplicationStatusListener::NotifyApplicationStateChange(ApplicationState state) {
  JApplicationStatusClass::registerThreadSafeNativeApplicationStateListener(AttachCurrentThread());
}
```

## 3. System Classes (e.g., ParcelFileDescriptor)

**Before:**

```cpp
int ContentUriGetFd(const JavaRef<jobject>& java_parcel_file_descriptor) {
  int fd = Java_ContentUriUtils_getFd(env, java_parcel_file_descriptor);
}
```

**After:**

```cpp
// In .cc
...
int ContentUriGetFd(const JavaRef<jobject>& java_parcel_file_descriptor) {
  int fd = JContentUriUtilsClass::getFd(env, java_parcel_file_descriptor);
}
```

## 4. Converting ScopedJavaLocalRef<jobject>()

**Before:**

```cpp
return ScopedJavaLocalRef<jobject>();
```

**After:**

```cpp
return nullptr;
```
