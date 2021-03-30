libutf
------

[![Build Status](https://travis-ci.org/cls/libutf.svg)](https://travis-ci.org/cls/libutf)

This is a C89 UTF-8 library, with an API compatible with that of Plan 9's
libutf, but with a number of improvements:

 * Support for runes beyond the Basic Multilingual Plane.
 * `utflen` and `utfnlen` cannot overflow on 32- or 64-bit machines.
 * `chartorune` treats all invalid codepoints as though `Runeerror`.
 * `fullrune`, `utfecpy`, and `utfnlen` do not overestimate the length of
   malformed runes.
 * An extra function, `charntorune(p,s,n)`, equivalent to
   `fullrune(s,n) ? chartorune(p,s) : 0`.
 * `Runeerror` may be set to an alternative replacement value, such as -1, to be
   used instead of U+FFFD.

Differences to be aware of:

 * `UTFmax` is 6, though `runetochar` will never write more than 4 bytes. Plan
   9's `UTFmax` is 3.
 * `chartorune` may consume multiple bytes for each illegal rune. Plan 9 always
   consumes 1.
 * `runelen` and `runetochar` return 0 if the rune is too large to print. Plan 9
   erroneously returns `UTFmax`.
