# File Format

[TOC]

## Overview

There are three formats that SuperSize uses:

1. Full `.size` files:
   * Contains size information from running `supersize archive`.
   * The fields `padding`, `short_name`, and `template_name` are omitted and
     re-derived when loading.
2. Sparse `.size` files:
   * Can be created only via `supersize console`.
   * Contains a subset of symbols from a full `.size` file.
   * Encodes padding information for each symbol.
3. `.sizediff` files:
   * Created via `supersize console` or `supersize save_diff`.
   * Contains two nested sparse `.size` files, where unchanged symbols are
     removed from each.

## Format Details: .size

The file format is a `gzipped` encoding of symbols, where some symbol fields are
delta-encoded to improve compressibility. Text was chosen over binary because it
is easier to work with in Python and is easier than binary to debug. An effort
was made to make the text highly compressible in order to keep the file sizes as
small as possible.

Each `.size` file contains:

1. A list of section sizes.
2. Metadata (GN args, git revision, timestamp, etc),
3. A list of symbols.

The following specifies the `.size` file format.

### Header

* Line 0: Vanity line - says what tool created the file. Ignored when loading.
* Line 1: format version string. The current version is "Size File Format v1.1".
* Line 2: number of bytes for the header fields.
* Line 3+: the header fields, a stringified JSON object.

The JSON for the header fields looks like:

```json
{
  "build_config": {
    "git_revision": "f49193cacf8ed34160a04ada4acf2ad6a1a030c8",
    "gn_args": [ ... ],
    "tool_prefix": "third_party/llvm-build/Release+Asserts/bin/llvm-"
  },
  "containers": [
    {
      "name": "TrichromeLibrary.apk",
      "metadata": {
        ... (see models.py METADATA_ for list of keys)
      },
      "section_sizes": {
        ".rodata": 8344540,
        ".shstrtab": 287,
        ".text": 52389250
        ...
      }
    }, ...
  ],
  "has_components": true,
  "has_disassembly": true,
  "has_padding": false
}
```

### Path List

* Line 0: number of entries in the list.
* Lines 1..N: tuple of (`object_path`, `source_path`) where the two parts
    are tab-separated.

### Component List

* This section is only present if `has_components` is True in header fields.
* Line 0: number of entries in the list.
* Lines 1..N: Component names.

### Symbol Counts

* Line 0: Tab-separated list of `"<Container Name>section_names"`.
* Line 1: Tab-separated list of symbol counts, in the same order as the previous
  line.

### Numeric Values

In each section, the number of rows is the same as the number of items per line
in Symbol Counts. The values on a row are space separated, in the order of the
symbols in each group.

The numeric values are:
* Symbol addresses, delta-encoded
* Symbol sizes (in bytes)
* Symbol paddings (in bytes)
  * This section is only present if `has_padding` is True in header fields.
* Path indices, delta-encoded
  * Indices reference paths from the Path List.
* Component indices, delta-encoded
  * Indices reference components in the Component List.
  * This section is only present if `has_components` is True in header fields.

### Symbols

* Each line represents a single symbol.
* Values are tab-separated.
* Values include:
  * `full_name`,
  * `num_aliases` (omitted when identical to previous line),
  * `flags` (omitted when 0).

### Disassembly

* Line 0: Space-separated list of raw_symbols indices.
* Line 1: Line-separated list of disassembly length in bytes followed by the disassembly,
  in the same order as the previous line.


## Format Details .sizediff

The `.sizediff` file stores two sparse `.size` files.

### Header

* Line 0: Vanity line - says what tool created the file. Ignored when loading.
* Line 1: number of bytes for the header fields.
* Line 2+: the header fields, a stringified JSON object.

The JSON for the header fields looks like:

```json
{
  "before_length": 1234,
  "version": 1
}
```

### Before

A sparse `.size` file (gzipped).

### After

A sparse `.size` file (gzipped).
