VERS_1.0 {
  # Export JNI and native C symbols.
  global:
    # Required for libunwind. This is needed if built and then run internally.
    google_find_phdr;
    Java_*;
    JNI_OnLoad;
    JNI_OnUnload;
    TfLite*;

  # Hide everything else.
  local:
    *;
};
