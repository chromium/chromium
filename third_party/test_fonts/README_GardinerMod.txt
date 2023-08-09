Name: GardinerMod Font Family
Short Name: Aegyptus
URL: http://users.teilar.gr/~g1951d/
Version: 5.03
License: Public Domain
Security Critical: no
Shipped: no

Description:

A family of two derived fonts required for testing issues with font-family based
fallback font selection.

Based on Gardiner font by George Douros, from http://users.teilar.gr/~g1951d/
which is public domain, quoting from the site: "In lieu of a licence Fonts in
this site are offered free for any use; they may be installed, embedded, opened,
edited, modified, regenerated, posted, packaged and redistributed. George
Douros"

The modified version consists of two font files, both with the same family name,
each containing only one glyph. One contains a cat glyph, one contains a bug
glyph. The two fonts can be used to verify that font fallback is not based on
family name, but based on file handle, thus avoiding an issue with font family
name ambiguity.


Local Modifications:

Derived from Gardiner.ttf out of the Aegyptus package, created using the
following commands:

$ pyftsubset Gardiner.ttf U+131A3
$ mv Gardiner.ttf.subset GardinerBug.ttf
$ pyftsubset Gardiner.ttf U+130E0
$ mv Gardiner.ttf.subset GardinerCat.ttf
