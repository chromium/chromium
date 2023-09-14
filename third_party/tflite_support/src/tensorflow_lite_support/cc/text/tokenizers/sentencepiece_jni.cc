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

#include <jni.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "absl/strings/str_split.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/text/tokenizers/sentencepiece_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_jni_lib.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace support {

using ::tflite::support::text::tokenizer::SentencePieceTokenizer;
using ::tflite::support::utils::GetMappedFileBuffer;

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_SentencePieceTokenizer_nativeLoadResource(  // NOLINT
    JNIEnv* env, jobject obj, jobject model_buffer) {
  auto model = GetMappedFileBuffer(env, model_buffer);
  auto handle =
      absl::make_unique<SentencePieceTokenizer>(model.data(), model.size());
  return reinterpret_cast<jlong>(handle.release());
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_SentencePieceTokenizer_nativeUnloadResource(  // NOLINT
    JNIEnv* env, jobject obj, jlong handle) {
  delete reinterpret_cast<SentencePieceTokenizer*>(handle);
  return 0;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_SentencePieceTokenizer_nativeTokenize(  // NOLINT
    JNIEnv* env, jobject thiz, jlong handle, jstring jtext) {
  return nativeTokenize(env, handle, jtext);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_SentencePieceTokenizer_nativeConvertTokensToIds(  // NOLINT
    JNIEnv* env, jobject thiz, jlong handle, jobjectArray jtokens) {
  return nativeConvertTokensToIds(env, handle, jtokens);
}

}  // namespace support
}  // namespace tflite
