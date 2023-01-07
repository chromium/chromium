# Copyright 2017 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys


_MAPPINGS = {
    '_ZL18extFromUUseMappingaji': (
        'extFromUUseMapping(signed char, unsigned int, int)'),
    '_ZL18extFromUUseMapping2aji': 'extFromUUseMapping(aj, int)',
    '_ZL18ucnv_extMatchFromUPKiiPKtiS2_iPjaa': (
        'ucnv_extMatchFromU(int const*, int, unsigned short const*, int, unsign'
        'ed short const*, int, unsigned int*, signed char, signed char)'),
    '_ZN12_GLOBAL__N_1L37kAnimationFrameTimeHistogramClassPathE': (
        '(anonymous namespace)::kAnimationFrameTimeHistogramClassPath'),
    '_ZN12_GLOBAL__N_1L20kSystemClassPrefixesE': (
        '(anonymous namespace)::kSystemClassPrefixes'),
    '_ZL35kMethodsAnimationFrameTimeHistogram': (
        'kMethodsAnimationFrameTimeHistogram'),
    '_ZN4base7androidL22kBaseRegisteredMethodsE': (
        'base::android::kBaseRegisteredMethods'),
    '_ZN4base7android12_GLOBAL__N_125g_renderer_histogram_codeE': (
        'base::android::(anonymous namespace)::g_renderer_histogram_code'),
    '_ZN4base7android12_GLOBAL__N_124g_library_version_numberE': (
        'base::android::(anonymous namespace)::g_library_version_number'),
    ('_ZZL13SaveHistogramP7_JNIEnvRKN4base7android12JavaParamRefIP8_jobjectEERK'
     'NS3_IP8_jstringEERKNS3_IP11_jlongArrayEEiE24atomic_histogram_pointer'): (
         'SaveHistogram(_JNIEnv*, base::android::JavaParamRef<_jobject*> const&'
         ', base::android::JavaParamRef<_jstring*> const&, base::android::JavaP'
         'aramRef<_jlongArray*> const&, int)::atomic_histogram_pointer'),
    '_ZN12_GLOBAL__N_135g_AnimationFrameTimeHistogram_clazzE': (
        '(anonymous namespace)::g_AnimationFrameTimeHistogram_clazz'),
    '_ZN5blink23ContiguousContainerBase11shrinkToFitEv': (
        'blink::ContiguousContainerBase::shrinkToFit()'),
    '_ZN5blink23ContiguousContainerBase11shrinkToFitEv2.part.1234.isra.2': (
        'blink::ContiguousContainerBase::shrinkToFit() [clone .part.1234] '
        '[clone .isra.2]'),
    '_ZN5blink23ContiguousContainerBaseC2EOS0_': (
        'blink::ContiguousContainerBase::ContiguousContainerBase('
        'blink::ContiguousContainerBase&&)'),
    '_ZN5blink12PaintChunker18releasePaintChunksEv': (
        'blink::PaintChunker::releasePaintChunks()'),
    '_ZN10_GLOBAL__N5blink12PaintChunker18releasePaintChunksEv.part.1': (
        '(anonymous namespace)::blink::PaintChunker::releasePaintChunks() '
        '[clone .part.1]'),
    '_ZN6chrome5mojom11FilePatcher5Name_E': 'chrome::mojom::FilePatcher::Name_',
    '_ZTV18ChromeMainDelegate': 'vtable for ChromeMainDelegate',
    '_ZTVN6chrome5mojom34FieldTrialRecorderRequestValidatorE': (
        'vtable for chrome::mojom::FieldTrialRecorder'),
    '_ZTVN6chrome5mojom23FieldTrialRecorderProxyE': (
        'vtable for chrome::mojom::FieldTrialRecorderProxy'),
    ('_ZZN10_GLOBAL__N5blink20CSSValueKeywordsHash13findValueImplEPKcjE15value_'
        'word_list'): ('(anonymous namespace)::blink::CSSValueKeywordsHash::'
        'findValueImpl(char const*, unsigned int)::value_word_list'),
    '_ZTV25ChromeMainDelegateAndroid': 'vtable for ChromeMainDelegateAndroid',
    '_ZTVN4mojo15MessageReceiverE': 'vtable for mojo::MessageReceiver',
    '_ZN6google8protobuf8internal19pLinuxKernelCmpxchgE': (
        'google::protobuf::internal::pLinuxKernelCmpxchg'),
    '_ZN6google8protobuf8internal25pLinuxKernelMemoryBarrierE': (
        'google::protobuf::internal::pLinuxKernelMemoryBarrier'),
}


def main():
  for line in sys.stdin:
    sys.stdout.write(_MAPPINGS[line.rstrip()])
    sys.stdout.write('\n')


if __name__ == '__main__':
  main()
