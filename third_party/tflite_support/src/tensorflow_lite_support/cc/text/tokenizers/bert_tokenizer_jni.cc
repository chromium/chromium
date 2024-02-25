/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

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

#include <string>

#include "absl/memory/memory.h"  // from @com_google_absl
#include "tensorflow_lite_support/cc/text/tokenizers/bert_tokenizer.h"
#include "tensorflow_lite_support/cc/text/tokenizers/tokenizer_jni_lib.h"
#include "tensorflow_lite_support/cc/utils/jni_utils.h"

namespace tflite {
namespace support {

using ::tflite::support::text::tokenizer::BertTokenizer;
using ::tflite::support::text::tokenizer::BertTokenizerOptions;
using ::tflite::support::utils::StringListToVector;

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_BertTokenizer_nativeLoadResource(  // NOLINT
    JNIEnv* env, jobject thiz, jobject vocab_list, jint max_bytes_per_token,
    jint max_chars_per_sub_token, jstring jsuffix_indicator,
    jboolean use_unknown_token, jstring junknown_token,
    jboolean split_unknown_chars) {
  // Convert java.util.List<String> into std::vector<string>
  std::vector<std::string> vocab = StringListToVector(env, vocab_list);

  // Convert jstrings to std::string
  const char* raw_suffix_indicator =
      env->GetStringUTFChars(jsuffix_indicator, JNI_FALSE);
  std::string suffix_indicator(raw_suffix_indicator);

  const char* raw_unknown_token =
      env->GetStringUTFChars(junknown_token, JNI_FALSE);
  std::string unknown_token(raw_unknown_token);

  auto handle = absl::make_unique<BertTokenizer>(
      vocab, BertTokenizerOptions{
                 .max_bytes_per_token = max_bytes_per_token,
                 .max_chars_per_subtoken = max_chars_per_sub_token,
                 .suffix_indicator = suffix_indicator,
                 .use_unknown_token = static_cast<bool>(use_unknown_token),
                 .unknown_token = unknown_token,
                 .split_unknown_chars = static_cast<bool>(split_unknown_chars),
                 .delim_str = text::tokenizer::kDefaultDelimRe,
                 .include_delim_str = text::tokenizer::kDefaultIncludeDelimRe});

  env->ReleaseStringUTFChars(jsuffix_indicator, raw_suffix_indicator);
  env->ReleaseStringUTFChars(junknown_token, raw_unknown_token);

  return reinterpret_cast<jlong>(handle.release());
}

extern "C" JNIEXPORT jlong JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_BertTokenizer_nativeUnloadResource(  // NOLINT
    JNIEnv* env, jobject thiz, jlong handle) {
  delete reinterpret_cast<BertTokenizer*>(handle);
  return 0;
}

extern "C" JNIEXPORT jobjectArray JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_BertTokenizer_nativeTokenize(
    JNIEnv* env, jobject thiz, jlong handle, jstring jtext) {
  return nativeTokenize(env, handle, jtext);
}

extern "C" JNIEXPORT jintArray JNICALL
Java_org_tensorflow_lite_support_text_tokenizers_BertTokenizer_nativeConvertTokensToIds(  // NOLINT
    JNIEnv* env, jobject thiz, jlong handle, jobjectArray jtokens) {
  return nativeConvertTokensToIds(env, handle, jtokens);
}

}  // namespace support
}  // namespace tflite
