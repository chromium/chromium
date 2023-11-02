## Pak Symbols

Pak symbols are those with a `section` of:

 * `.pak.nontranslated` (entries in `resources.pak`, `chrome_100_percent.pak`)
 * `.pak.translations` (entries in `en_US.pak`, `fr.pak`, etc)
 * There is one symbols for each GRIT ID. The size of the symbol is the sum of
   the size of all pak entries with that ID across all `.pak` files.

## Algorithm

**During builds:**

 * [Grit] records the originating `.grd` file, textual ID, and numeric ID for
   each entry that it processes during a `grit()` step into a `.pak.info` file.
 * These `.pak.info` files are merged during `repack` and APK / App Bundle steps
   into a single `$output_dir/size-info/Foo.apk.pak.info`.

[Grit]: /tools/grit/README.md

**During `supersize archive`:**

1. When native symbols are created using `linker_map` mode, a map of `pak_id` ->
   `source_path` is created by looking for symbols named
   `ui::AllowlistedResource<$PAK_ID>()`.
2. Pak files are extracted and turned into dicts using grit.
3. The size of each pak entry is summed with other entries of the same ID.
4. Symbol aliases are created for pak entries with different IDs, but that
   refer to the same data (the `.pak` file format supports this).
5. Entry names ("textual IDs"), and the original `.grd` file paths are stored in
   the `full_name` of each symbol by using the information in the
   `$outdir/size-info/Foo.pak.info` file.
6. Path information is added to the symbols using the map from Step 1.
