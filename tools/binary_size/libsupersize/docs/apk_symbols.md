# APK Symbols

All files in an APK that are not broken down into sub-entries are tracked by a
symbol within the `.other` section. This includes:

 * `resources.arsc`
 * `assets/*`
 * `res/*`

## Algorithm

**During builds:**

* An `$output_dir/size-info/Foo.res.info` file is created that records the
  originating path for each android resource file.

**During `supersize archive`:**

For each zip entry:

 * One symbol is created using the compressed size of the entry.
 * Files within `res/` are given a source path using the mapping from
   `$output_dir/Foo.apk.res.info`
 * Other entries are given a source path of `$APK/$zip_path`.

Each zip entry incurs additional size overhead from:

 * Zip file local header,
   * Size dependent on filename length and zipalign overhead.
 * Zip file central directory entry,
 * An entry within V1 signature files (`META-INF/` files).
   * Applicable only for older `minSdkVersion` apks, and for system image apks.
 * `res/` files have an associated entry within `resources.arsc`.

Rather than include this overhead in each symbol, they are put altogether into a
single `Overhead: APK file` symbol. Because:

1. The symbol size is more understandable if it matches the `unzip -lv` output.
2. It prevents zipalign overhead from causing many small size changes in diffs.
3. When looking at size optimizations, it makes more sense to look at the total
   overhead rather than the per-symbol overhead.
