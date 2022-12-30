# SuperSize

SuperSize is comprised of two parts:

1. A command-line tool for creating and inspecting `.size` and `.sizediff` files,
2. A web app for visualizing `.size` and `.sizediff` files.

For more details, see [//tools/binary_size/libsupersize/docs].

[//tools/binary_size/libsupersize/docs]: /tools/binary_size/libsupersize/docs

[TOC]

## Why SuperSize?

Chrome on Android needs to be as lean as possible. Having a tool that can show
why binary grows & shrinks helps keep it lean.

The [android-binary-size trybot] uses SuperSize to show an APK Breakdown on
every Chromium code review.

SuperSize is also used when creating [milestone size reports] (Googlers only).

[android-binary-size trybot]: /docs/speed/binary_size/android_binary_size_trybot.md
[milestone size reports]: https://goto.google.com/chromemilestonesizes

## Is SuperSize a Generic Tool?

No. It works only for binaries built using Chrome's custom build system. E.g.:

 * It assumes `.ninja` build rules are available.
 * It uses heuristic for locating `.so` given `.apk`.
 * It requires the `size-info` build directory to analyze `.pak` and `.dex`
   files.

## SuperSize Usage

### supersize archive

Collect size information into a `.size` file.

*** note
**Note:** Refer to
[diagnose_bloat.py](https://cs.chromium.org/search/?q=file:diagnose_bloat.py+gn_args)
for list of GN args to build a release binary (or just use the tool with --single).
***

Example Usage:

```bash
# Android:
autoninja -C out/Release chrome_public_apk
tools/binary_size/supersize archive chrome.size -f out/Release/apks/ChromePublic.apk -v

# Linux:
autoninja -C out/Release chrome
tools/binary_size/supersize archive chrome.size -f out/Release/chrome -v
```

### supersize console

Starts a Python interpreter where you can run custom queries, or run pre-made
queries from `canned_queries.py`.

Example Usage:

```bash
# Prints size information and exits (does not enter interactive mode).
tools/binary_size/supersize console chrome.size --query='Print(size_info)'

# Enters a Python REPL (it will print more guidance).
tools/binary_size/supersize console chrome.size
```

Example Session:

```python
>>> ShowExamples()  # Get some inspiration.
...
>>> sorted = size_info.symbols.WhereInSection('t').Sorted()
>>> Print(sorted)  # Have a look at the largest symbols.
...
>>> sym = sorted.WhereNameMatches('TrellisQuantizeBlock')[0]
>>> Disassemble(sym)  # Time to learn assembly.
...
>>> help(canned_queries)
...
>>> Print(canned_queries.TemplatesByName(depth=-1))
...
>>> syms = size_info.symbols.WherePathMatches(r'skia').Sorted()
>>> Print(syms, verbose=True)  # Show full symbol names with parameter types.
...
>>> # Dump all string literals from skia files to "strings.txt".
>>> Print((t[1] for t in ReadStringLiterals(syms)), to_file='strings.txt')
```

### supersize save_diff

Creates a `.sizediff` file given two `.size` files. A `.sizediff` file contains
two `.size` files, with all unchanged symbols removed.

Example Usage:

```bash
tools/binary_size/supersize save_diff before.size after.size out.sizediff
```

### supersize diff

A convenience command equivalent to:
`console before.size after.size --query='Print(Diff(size_info1, size_info2))'`

Example Usage:

```bash
tools/binary_size/supersize diff before.size after.size --all
```

## Sharing .size(diff) Files

There is a GCS bucket available for Googlers to share SuperSize files (requires
a one-time `gsutil.py config` to login).

To share publicly:

```sh
FILENAME=descriptive_name.sizediff
gsutil.py cp -a public-read "$FILENAME" gs://chrome-supersize/oneoffs/$USER/
echo "Share via: https://chrome-supersize.firebaseapp.com/viewer.html?load_url=https://storage.googleapis.com/chrome-supersize/oneoffs/$USER/$(basename $FILENAME)"
```

To share to Googlers only:

```sh
FILENAME=descriptive_name.sizediff
gsutil.py cp "$FILENAME" gs://chrome-supersize/private-oneoffs/$USER/
echo "Share via: https://chrome-supersize.firebaseapp.com/viewer.html?load_url=https://storage.googleapis.com/chrome-supersize/private-oneoffs/$USER/$(basename $FILENAME)"
```

To delete a file you uploaded by mistake:

```sh
gsutil.py rm gs://chrome-supersize/oneoffs/$USER/filename
```
