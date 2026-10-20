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

1. All `.dex` files in the apk are parsed (in a background process) by
   `dex_parser.py` to find the size and FQN of entries across all dex files.
   * One symbol is created for each class, method, and field.
   * Sizes are computed the same way as dexlib2 (which `apkanalyzer` uses):
     * Classes: `class_def_item`, `type_id_item`, and their share of
       `class_data_item`, `annotations_directory_item`, etc.
     * Methods: `method_id_item`, `encoded_method`, and `code_item`.
     * Fields: `field_id_item`, `encoded_field`, and static initial values.
     * Members that are referenced but not defined (e.g. framework methods)
       are attributed the size of their `method_id_item` / `field_id_item`.
   * Names are deobfuscated using the R8 `.mapping` file. Methods are matched
     using their residual signatures (signatures within the dex file) so that
     methods whose signatures were changed by R8 are deobfuscated correctly.
     When a method has inlined frames, the outer-most frame is used.
2. String literals used by code are attributed to the classes that use them.
   Remaining strings are grouped into aggregate symbols (e.g.
   `** .dex (type strings)`).
3. Source paths are added to symbols using the mapping from
   `$output_dir/Foo.apk.jar.info`
