# Native Symbols

This doc describes how SuperSize breaks down native binaries into symbols.

[TOC]

## Overview

Native symbols are those with a `section` of:

 * `.text` (executable code)
 * `.rodata` (read-only data)
 * `.data` (writable data)
 * `.data.rel.ro` (data that is read-only after ELF relocations are applied)
 * `.bss` (symbols that are zero-initialized. These consume no space in the
    binary, and so are generally ignored despite still being collected.

There are 3 modes that SuperSize can use to break an ELF down into symbols:

 * `linker_map` - Uses linker map + build directory to create symbols.
 * `dwarf` - Uses debug information to create symbols.
 * `sections` - Creates one symbol for each ELF section.

## Mode: linker_map

This is the mode that produces the largest number of symbols, and thus is the
preferred mode. Information provided only by this mode:

 * Path information for symbols outside of .text
   * DWARF information is complete for .text symbols (maybe because stack
   symbolization is a primary use-case?), but incomplete or missing for symbols
   in other sections.
 * String literals (.rodata symbols that look like `"some string dat..."`).
   * Linker map files contain `** merge strings` entries, which tell us where
     to string tables exist within `.rodata`.
 * `object_path`, which is useful for attributing STL usages to individual
   source files.
 * Path aliases - when an inline symbol is used by multiple source files, we
   attribute the symbol's cost equally among the files.
 * Linker-generated symbols. E.g. Switch tables.

### Data Sources

 * `build.ninja` is parsed to get:
   * List of `.o` and `.a` files that were inputs to the linker.
   * Mapping of `.cc` -> `.o` files.
 * All `.o` (and `.a`) files are parsed:
   * with `nm` to get symbol list.
   * Non-ThinLTO: with `nm` to get list of string literals
   * ThinLTO: with `llvm-bcanalyzer` to get list of string literals
 * ELF file is parse with `nm` to get list of symbol names that were
   identical-code-folded to the same address.
 * Linker map (created via `-Wl,-Map=output.map`) parsed to get:
   * Full list of symbols that comprise the binary,
   * Location of string tables (`** merge strings` entries).
   * Non-ThinLTO: `object_path` (`.o` file) associated with each symbol
   * Note:
     * With ThinLTO, `object_path` points to a hashed filename within the thinlto
       cache (not useful).
     * When multiple symbols are folded together due to Identical Code Folding,
       the linker map file lists only one of them.
 * ELF file string tables are parsed by looking for `\0` bytes and creating
   string literal symbols for each string therein.

### Algorithm

1. Create initial symbol list from linker map.
2. Assign object paths by seeing which `.o` files define each symbol (match up
   the names).
   * When multiple files define the same symbol, create symbol aliases.
3. Create string literal symbols from string tables, and assign them paths based
   on which `.o` files define the same string literal.
4. Assign `source_path` using the `.o` -> `.cc` mapping from `build.ninja`.
   * This means that `.h` files are never listed as sources. No information
     about inlined symbols is gathered (by design).
5. Create symbol aliases when `nm` reports multiple symbols mapping to the same
   address.
6. Normalize `source_path` by removing generated path prefix (and adding
   `FLAG_GENERATED`) when applicable.
7. Normalize symbol names.

## Mode: dwarf

Creates symbols using only an ELF with debug information enabled. Requires
compiler flag `-gmlt` to enable full source paths (rather than just basename).

### Algorithm

1. Create initial symbol list with `nm --print-size`.
2. Add name aliases using output from `nm` (this could have been done at the
   same time as the previous step, but is done as a separate step in order to
   share logic with `linker_map` mode.
3. Uses `dwarfdump` to find all `DW_AT_compile_unit` and `DW_AT_ranges` entries
   and create a map of address range -> source path.
4. Assign source paths based to .text symbols based on symbol address.

### Why not use Bloaty?

[Bloaty](https://github.com/google/bloaty) is an excellent tool, and produces
size information with similar fidelity to "dwarf" mode, as it uses the same
data source. We did not use bloaty since "dwarfdump" was already readily
available and gave similar results. It would be nice to also have a "bloaty"
mode so that we could more direclty compare outputs.

## Mode: sections

This mode uses `readelf -s` to create one symbol for each ELF section. It is
used for native files where no debug information or linker map file is
available, and for native files whose ABI do not match the `--abi-filter`.

## Data Normalization

Some manipulation happens in order to make names and paths more human-readable.

 * `(anonymous::)` is removed from names (and stored as a symbol flag).
 * `[clone]` suffix removed (and stored as a symbol flag).
 * `vtable for FOO` -> `Foo [vtable]`
 * Mangling done by linkers is undone (e.g. prefixing with "unlikely.")
 * Names are processed into:
   * `name`: Name without template and argument parameters.
   * `template_name`: Name without argument parameters.
   * `full_name`: Name with all parameters.
 * LLVM function outlining creates many `OUTLINED_FUNCTION_*` symbols. These are
   renamed to `** outlined functions` or `** outlined functions * (count)`,
   and are de-duped so an address can have at most one such symbol.
   * Update: Outlining was ARM64-only, and has been disabled in our build due
     to performance regressions.
