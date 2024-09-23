include_rules = [
  # Limit files that can depend on icu.
  "-base/i18n",
  "-third_party/icu",
  "+third_party/jni_zero",
]

specific_include_rules = {
  "gurl_fuzzer.cc": [
    "+base/i18n",
  ],
  "url_(canon|idna)_icu(\.cc|_unittest\.cc|_fuzzer\.cc|_test_helpers\.h)": [
    "+base/i18n",
    "+third_party/icu",
  ],
  "run_all_unittests\.cc": [
    "+mojo/core/embedder",
  ],
}
