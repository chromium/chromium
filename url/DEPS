include_rules = [
  # Limit files that can depend on icu.
  "-base/i18n",
  "-third_party/icu",
]

specific_include_rules = {
  "gurl_fuzzer.cc": [
    "+base/i18n",
  ],
  "url_(canon|idna)_icu(\.cc|_unittest\.cc)": [
    "+third_party/icu",
  ],
  "run_all_unittests\.cc": [
    "+mojo/core/embedder",
  ],
}
