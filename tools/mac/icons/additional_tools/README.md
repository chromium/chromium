# Useful icon tools

This directory contains tools that were used [in the 2015–2016
era](https://crbug.com/40430886) to construct Chrome icons. (Please accept our
deepest apologies for this being a Google-only bug; it contains a lot of
internal build information and images.)

Chromium has entered the era of icons being backed by asset catalogs and the era
of lovingly–hand-crafted `.icns` files is over. Nevertheless, these tools remain
useful for diagnostic purposes.

To compile these tools, run `make` in this directory. Note that `libpng` is
required.

## Tool list

- `makeicns`: This tool takes an `.iconset` directory of `.png` files and
creates an `.icns` file containing them. However, it cannot create the old-style
data/mask image pairs, which is why `makeicns2` is preferred.
- `makepng`: This tool is used by the `unmakeicns` tool. It reconstructs a
`.png` file from the uncompressed data of an old-style data/mask image pair. It
is available separately should this be a capability you require.
- `maketoc.py`: This tool can be used to both create a `TOC` section for an
`.icns` file, as well as verify one. The `makeicns2` tool automatically creates
a `TOC` section, but this tool's verification of these sections may be useful.
- `unmakeicns`: This tool takes an `.icns` file and unpacks it into its
constituent images. For the old-style data/mask image pairs, this tool will
reconstitute the two into a more useful `.png` file.
- `unpackicon`: This tool is used by the `unmakeicns` tool. It takes the data
from an old-style data/mask image pair, which is encoded with a simple
PackBits-esque RLE scheme, and outputs the data uncompressed. It is available
separately should this be a capability you require.
