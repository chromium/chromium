/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_jni_lib.h"

namespace tflite {
namespace support {

using ::tflite::support::text::tokenizer::Tokenizer;
using ::tflite::support::text::tokenizer::TokenizerResult;
using ::tflite::support::utils::CheckNotNull;
using ::tflite::support::utils::JStringToString;
using ::tflite::support::utils::kIllegalStateException;

jobjectArray nativeTokenize(JNIEnv* env, jlong handle, jstring jtext) {
  if (handle == 0) {
    env->ThrowNew(env->FindClass(kIllegalStateException),
                  "Vocab not initialized!");
    return nullptr;
  }

  Tokenizer* tokenizer = reinterpret_cast<Tokenizer*>(handle);

  // Get the tokenization results.
  const TokenizerResult tokenize_result =
      tokenizer->Tokenize(JStringToString(env, jtext));
  std::vector<std::string> subwords = tokenize_result.subwords;

  jclass string_class = CheckNotNull(env, env->FindClass("java/lang/String"));
  jobjectArray result = CheckNotNull(
      env, env->NewObjectArray(subwords.size(), string_class, nullptr));

  for (int i = 0; i < subwords.size(); ++i) {
    jstring text = CheckNotNull(env, env->NewStringUTF(subwords[i].data()));
    if (env->ExceptionCheck()) {
      return nullptr;
    }

    env->SetObjectArrayElement(result, i, text);
  }

  return result;
}

jintArray nativeConvertTokensToIds(JNIEnv* env, jlong handle,
                                   jobjectArray jtokens) {
  if (handle == 0) {
    env->ThrowNew(env->FindClass(kIllegalStateException),
                  "vocab not initialized!");
    return nullptr;
  }

  Tokenizer* tokenizer = reinterpret_cast<Tokenizer*>(handle);

  // Get the token ids.
  const int count = env->GetArrayLength(jtokens);
  jintArray result = env->NewIntArray(count);
  jint* jid_ptr = env->GetIntArrayElements(result, nullptr);

  for (int i = 0; i < count; i++) {
    auto jstr =
        reinterpret_cast<jstring>(env->GetObjectArrayElement(jtokens, i));
    const char* token = env->GetStringUTFChars(jstr, JNI_FALSE);
    int id;
    tokenizer->LookupId(token, &id);
    jid_ptr[i] = id;
    env->ReleaseStringUTFChars(jstr, token);
  }
  env->ReleaseIntArrayElements(result, jid_ptr, 0);
  return result;
}

}  // namespace support
}  // namespace tflite
