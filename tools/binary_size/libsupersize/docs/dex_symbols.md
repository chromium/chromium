# Dex Symbols

Dex symbols are those with a `section` of:
 * `.dex.method` (Java methods)
 * `.dex` (All other forms of dex size. E.g. Class descriptors)

## Algorithm

**During builds:**

 * Java compile targets create a mapping between Java fully qualified names
   (FQN) and source files.
    * For `.java` files the FQN of the public class is mapped to the file.
    * For `.srcjar` files the FQN of the public class is mapped to the `.srcjar`
      file path.
    * A complete per-apk class FQN to source mapping is stored in the
      `$output_dir/size-info` dir.

**During `supersize archive`:**

1. `$ANDROID_SDK/cmdline-tools/apkanalyzer dex packages` is used to find the
   size and FQN of entries in across all dex files.
   * One symbol is created for each method and class entry in the output.
2. Source paths are added to symbols using the mapping from
   `$output_dir/Foo.apk.jar.info`
