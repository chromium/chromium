# Data Model

The SuperSize data model is a sorted flat list of symbols. Using a flat list is
simple, and allows arbitrary queries to be made on symbols.

[//tools/binary_size/libsupersize/models.py] contains the definition of all data
classes.

[//tools/binary_size/libsupersize/models.py]: /tools/binary_size/libsupersize/models.py

[TOC]

## Python API Reference

### SizeInfo

Represents the data within a `.size` file. Contains:

 * `build_config`: JSON metadata applicable to all symbols.
 * `containers`: List of Container instances used by symbols in this SizeInfo.
 * `raw_symbols`: List of Symbols.

### Symbol

Each symbol contains the following fields:

 * `container`: A (shared) Container instance.
 * `section_name`: E.g. ".text", ".rodata", ".data.rel.local"
 * `section`: The single character abbreviation of `section_name`.
    E.g. "t", "r", "d".
 * `size`: The number of bytes this symbol takes up, including padding that
    comes before |address|.
 * `padding`: The number of bytes of padding before |address|.
 * `address` (optional): The start address of the symbol.
 * `source_path` (optional): Path to the source file that caused this symbol to
    exist (e.g. `base64.cc`, `SomeClass.java`).
 * `object_path` (optional):
    * For native and pak: Path to associated object file. E.g.: `base/base64.o`
    * For dex: Package path. E.g.: `$APK/org/chromium/chrome/SomeClass.class`
 * `aliases`: List of symbols that represent the same bytes. The |aliases| of
   each symbol in this list points to the same list instance.
 * `num_aliases`: The number of symbols with the same address (including self).
 * `pss`: `size` / `num_aliases`.
 * `padding_pss`: `padding` / `num_aliases`.
 * `full_name`: Name for this symbol.
    * Symbols are not required to have unique names, or names as all (empty
      string is valid).
 * `template_name`: Derived from `full_name`. Name with parameter list removed,
       but template parameters present.
 * `name`: Derived from `full_name`. Names with templates and parameter list
       removed.
 * `component`: The team that owns this feature (optional, maybe be empty).
 * `flags`: Bitmask of flags. See `FLAG_*` constants in `models.py`.
 * `disassembly` (optional): The disassembly code for the symbol.

### Diffs

Diffs are represented in Python using `DeltaSizeInfo`, which contains a list of
`DeltaSymbol` instances. `DeltaSymbols` maintain the full fidelity of symbols in
the diff by storing a pointer to the before / after symbol that they represent.
See [diffs.md](diffs.md) for more details.

## Concepts

### Symbol Aliases

Aliases occur when multiple symbols refer to the same bytes (have the same
`address`, `size`, and `padding`).

Examples of where aliases are used:

 * Functions with identical code are de-deuped via identical code folding.
 * Functions that appear in multiple translation units (e.g. functions with
   inline linkage). These have the same name, but different paths.
   * Represented as one alias per path, but are collapsed into a single symbol
     with a path of `$COMMON_PREFIX/{shared}/$SYMBOL_COUNT` when the number of
     aliases is large.
     * E.g.: `base/{shared}/3`
 * String literals that are de-duped by identical code folding.
 * Pak entries with identical payloads.

### Path Normalization

 * Prefixes are removed: `out/Release/`, `gen/`, `obj/`
   * This causes generated files to overlay non-generated source tree, which is
     useful for attribution since the two generally mirror one another.
   * Generated symbols have the `FLAG_GENERATED` bit set.

### Overhead and Star Symbols

**Overhead symbols** are symbols with a name that starts with "Overhead:". They
track bytes that are generally unactionable. They are recorded as padding-only
symbols (e.g.: `size=10`, `padding=10`, `size_without_padding=0`) because
"padding" better associates with "overhead" vs. size.

* `Overhead: ELF file`: `elf_file_size - sum(elf_sections)`.
  * Captures bytes taken up by ELF headers and section alignment.
* `Overhead: APK file`: `apk_file_size - sum(compressed_file_sizes)`
  * Captures bytes taken up by `.zip` metadata and zipalign padding.
* `Overhead: ${NAME}.pak`: `pak_file_size - sum(pak_entries)`
* `Overhead: aggregate padding of diff'ed symbols`: Appears in symbol diffs to
  represent the per-section cumulative delta in padding.

**Star symbols** are symbols with a name that starts with "\*\*". They represent
sections of binary that are unattributed.

Examples:

 * `** Merge Globals` - Taken from linker map file. A section of data
   containing unnamed constants.
 * `** Symbol gap`: A gap between symbols that is larger than what could be
   plausibly be due to alignment.
 * `** ELF Section: .rel.dyn`: A native code ELF section that is not broken down
   into smaller symbols.
