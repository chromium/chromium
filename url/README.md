# Chrome's URL library

## Layers

There are several conceptual layers in this directory. Going from the lowest
level up, they are:

### Parsing

The `url_parse.*` files are the parser. This code does no string
transformations. Its only job is to take an input string and split out the
components of the URL as best as it can deduce them, for a given type of URL.
Parsing can never fail, it will take its best guess. This layer does not
have logic for determining the type of URL parsing to apply, that needs to
be applied at a higher layer (the "util" layer below).

Because the parser code is derived (_very_ distantly) from some code in
Mozilla, some of the parser files are in `url/third_party/mozilla/`.

The main header to include for calling the parser is
`url/third_party/mozilla/url_parse.h`.

### Canonicalization

The `url_canon*` files are the canonicalizer. This code will transform specific
URL components or specific types of URLs into a standard form. For some
dangerous or invalid data, the canonicalizer will report that a URL is invalid,
although it will always try its best to produce output (so the calling code
can, for example, show the user an error that the URL is invalid). The
canonicalizer attempts to provide as consistent a representation as possible
without changing the meaning of a URL.

The canonicalizer layer is designed to be independent of the string type of
the embedder, so all string output is done through a `CanonOutput` wrapper
object. An implementation for `std::string` output is provided in
`url_canon_stdstring.h`.

The main header to include for calling the canonicalizer is
`url/url_canon.h`.

### Utility

The `url_util*` files provide a higher-level wrapper around the parser and
canonicalizer. While it can be called directly, it is designed to be the
foundation for writing URL wrapper objects (The GURL later and Blink's KURL
object use the Utility layer to implement the low-level logic).

The Utility code makes decisions about URL types and calls the correct parsing
and canonicalzation functions for those types. It provides an interface to
register application-specific schemes that have specific requirements.
Sharing this loigic between KURL and GURL is important so that URLs are
handled consistently across the application.

The main header to include is `url/url_util.h`.

### Google URL (GURL) and Origin

At the highest layer, a C++ object for representing URLs is provided. This
object uses STL. Most uses need only this layer. Include `url/gurl.h`.

Also at this layer is also the Origin object which exists to make security
decisions on the web. Include `url/origin.h`.

## Historical background

This code was originally a separate library that was designed to be embedded
into both Chrome (which uses STL) and WebKit (which didn't use any STL at the
time). As a result, the parsing, canonicalization, and utility code could
not use STL, or any other common code in Chromium like base.

When WebKit was forked into the Chromium repo and renamed Blink, this
restriction has been relaxed somewhat. Blink still provides its own URL object
using its own string type, so the insulation that the Utility layer provides is
still useful. But some STL strings and calls to base functions have gradually
been added in places where doing so is possible.

## Caution for terminologies

Due to historical usage, the term "Standard URL" is currently used within the
code to represent "[Special URLs][1]", except for "file:" scheme URL, as defined
in the URL Standard. However, this terminology is outdated and can lead to
confusion, particularly now that we are supporting [non-special URLs][2] as well
([crbug/1416006][3]). For the sake of consistency and clarity, it is recommended
to switch to the more accurate term "Special URL" throughout the codebase.
However, this change should be carefully planned and executed due to the
widespread use of the current terminology in both internal and third-party code.
For a while, "Standard URL" and "Special URL" are used interchangeably.

[1]: https://url.spec.whatwg.org/#is-special
[2]: https://url.spec.whatwg.org/#is-not-special
[3]: https://crbug.com/1416006
