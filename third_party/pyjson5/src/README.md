# pyjson5

A Python implementation of the JSON5 data format.

[JSON5](https://json5.org) extends the
[JSON](http://www.json.org) data interchange format to make it
slightly more usable as a configuration language:

* JavaScript-style comments (both single and multi-line) are legal.

* Object keys may be unquoted if they are legal ECMAScript identifiers

* Objects and arrays may end with trailing commas.

* Strings can be single-quoted, and multi-line string literals are allowed.

There are a few other more minor extensions to JSON; see the above page for
the full details.

This project implements a reader and writer implementation for Python;
where possible, it mirrors the
[standard Python JSON API](https://docs.python.org/library/json.html)
package for ease of use.

There is one notable difference from the JSON api: the `load()` and
`loads()` methods support optionally checking for (and rejecting) duplicate
object keys; pass `allow_duplicate_keys=False` to do so (duplicates are
allowed by default).

This is an early release. It has been reasonably well-tested, but it is
**SLOW**. It can be 1000-6000x slower than the C-optimized JSON module,
and is 200x slower (or more) than the pure Python JSON module.

**Please Note:** This library only handles JSON5 documents, it does not
allow you to read arbitrary JavaScript. For example, bare integers can
be legal object keys in JavaScript, but they aren't in JSON5.

## Known issues

* Did I mention that it is **SLOW**?

* The implementation follows Python3's `json` implementation where
  possible. This means that the `encoding` method to `dump()` is
  ignored, and unicode strings are always returned.

* The `cls` keyword argument that `json.load()`/`json.loads()` accepts
  to specify a custom subclass of ``JSONDecoder`` is not and will not be
  supported, because this implementation uses a completely different
  approach to parsing strings and doesn't have anything like the
  `JSONDecoder` class.

* The `cls` keyword argument that `json.dump()`/`json.dumps()` accepts
  is also not supported, for consistency with `json5.load()`. The `default`
  keyword *is* supported, though, and might be able to serve as a
  workaround.

## Running the tests
To run the tests, setup a venv and install the required dependencies with
`pip install -e '.[dev]'`, then run the tests with `python setup.py test`.


## Version History / Release Notes

* v0.9.13 (2023-03-16)
    * [GitHub PR #64](https://github.com/dpranke/pyjson5/pull/64)
      Remove a field from one of the JSON benchmark files to
      reduce confusion in Chromium.
    * No code changes.
* v0.9.12 (2023-01-02)
    * Fix GitHub Actions config file to no longer test against
      Python 3.6 or 3.7. For now we will only test against an
      "oldest" release (3.8 in this case) and a "current"
      release (3.11 in this case).
* v0.9.11 (2023-01-02)
    * [GitHub issue #60](https://github.com/dpranke/pyjson5/issues/60)
      Fixed minor Python2 compatibility issue by referring to
      `float("inf")` instead of `math.inf`.
* v0.9.10 (2022-08-18)
    * [GitHub issue #58](https://github.com/dpranke/pyjson5/issues/58)
      Updated the //README.md to be clear that parsing arbitrary JS
      code may not work.
    * Otherwise, no code changes.
* v0.9.9 (2022-08-01)
    * [GitHub issue #57](https://github.com/dpranke/pyjson5/issues/57)
      Fixed serialization for objects that subclass `int` or `float`:
      Previously we would use the objects __str__ implementation, but
      that might result in an illegal JSON5 value if the object had
      customized __str__ to return something illegal. Instead,
      we follow the lead of the `JSON` module and call `int.__repr__`
      or `float.__repr__` directly.
    * While I was at it, I added tests for dumps(-inf) and dumps(nan)
      when those were supposed to be disallowed by `allow_nan=False`.
* v0.9.8 (2022-05-08)
    * [GitHub issue #47](https://github.com/dpranke/pyjson5/issues/47)
      Fixed error reporting in some cases due to how parsing was handling
      nested rules in the grammar - previously the reported location for
      the error could be far away from the point where it actually happened.

* v0.9.7 (2022-05-06)
    * [GitHub issue #52](https://github.com/dpranke/pyjson5/issues/52)
      Fixed behavior of `default` fn in `dump` and `dumps`. Previously
      we didn't require the function to return a string, and so we could
      end up returning something that wasn't actually valid. This change
      now matches the behavior in the `json` module. *Note: This is a
      potentially breaking change.*
* v0.9.6 (2021-06-21)
    * Bump development status classifier to 5 - Production/Stable, which
      the library feels like it is at this point. If I do end up significantly
      reworking things to speed it up and/or to add round-trip editing,
      that'll likely be a 2.0. If this version has no reported issues,
      I'll likely promote it to 1.0.
    * Also bump the tested Python versions to 2.7, 3.8 and 3.9, though
      earlier Python3 versions will likely continue to work as well.
    * [GitHub issue #46](https://github.com/dpranke/pyjson5/issues/36)
      Fix incorrect serialization of custom subtypes
    * Make it possible to run the tests if `hypothesis` isn't installed.

* v0.9.5 (2020-05-26)
    * Miscellaneous non-source cleanups in the repo, including setting
      up GitHub Actions for a CI system. No changes to the library from
      v0.9.4, other than updating the version.

* v0.9.4 (2020-03-26)
    * [GitHub pull #38](https://github.com/dpranke/pyjson5/pull/38)
      Fix from fredrik@fornwall.net for dumps() crashing when passed
      an empty string as a key in an object.

* v0.9.3 (2020-03-17)
    * [GitHub pull #35](https://github.com/dpranke/pyjson5/pull/35)
      Fix from pastelmind@ for dump() not passing the right args to dumps().
    * Fix from p.skouzos@novafutur.com to remove the tests directory from
      the setup call, making the package a bit smaller.

* v0.9.2 (2020-03-02)
    * [GitHub pull #34](https://github.com/dpranke/pyjson5/pull/34)
      Fix from roosephu@ for a badly formatted nested list.

* v0.9.1 (2020-02-09)
    * [GitHub issue #33](https://github.com/dpranke/pyjson5/issues/33):
       Fix stray trailing comma when dumping an object with an invalid key.

* v0.9.0 (2020-01-30)
    * [GitHub issue #29](https://github.com/dpranke/pyjson5/issues/29):
       Fix an issue where objects keys that started with a reserved
       word were incorrectly quoted.
    * [GitHub issue #30](https://github.com/dpranke/pyjson5/issues/30):
       Fix an issue where dumps() incorrectly thought a data structure
       was cyclic in some cases.
    * [GitHub issue #32](https://github.com/dpranke/pyjson5/issues/32):
       Allow for non-string keys in dicts passed to ``dump()``/``dumps()``.
       Add an ``allow_duplicate_keys=False`` to prevent possible
       ill-formed JSON that might result.

* v0.8.5 (2019-07-04)
    * [GitHub issue #25](https://github.com/dpranke/pyjson5/issues/25):
      Add LICENSE and README.md to the dist.
    * [GitHub issue #26](https://github.com/dpranke/pyjson5/issues/26):
      Fix printing of empty arrays and objects with indentation, fix
      misreporting of the position on parse failures in some cases.

* v0.8.4 (2019-06-11)
    * Updated the version history, too.

* v0.8.3 (2019-06-11)
    * Tweaked the README, bumped the version, forgot to update the version
      history :).

* v0.8.2 (2019-06-11)
    * Actually bump the version properly, to 0.8.2.

* v0.8.1 (2019-06-11)
    * Fix bug in setup.py that messed up the description. Unfortunately,
      I forgot to bump the version for this, so this also identifies as 0.8.0.

* v0.8.0 (2019-06-11)
    * Add `allow_duplicate_keys=True` as a default argument to
      `json5.load()`/`json5.loads()`. If you set the key to `False`, duplicate
      keys in a single dict will be rejected. The default is set to `True`
      for compatibility with `json.load()`, earlier versions of json5, and
      because it's simply not clear if people would want duplicate checking
      enabled by default.

* v0.7 (2019-03-31)
    * Changes dump()/dumps() to not quote object keys by default if they are
      legal identifiers. Passing `quote_keys=True` will turn that off
      and always quote object keys.
    * Changes dump()/dumps() to insert trailing commas after the last item
      in an array or an object if the object is printed across multiple lines
      (i.e., if `indent` is not None). Passing `trailing_commas=False` will
      turn that off.
    * The `json5.tool` command line tool now supports the `--indent`,
      `--[no-]quote-keys`, and `--[no-]trailing-commas` flags to allow
      for more control over the output, in addition to the existing
      `--as-json` flag.
    * The `json5.tool` command line tool no longer supports reading from
      multiple files, you can now only read from a single file or
      from standard input.
    * The implementation no longer relies on the standard `json` module
      for anything. The output should still match the json module (except
      as noted above) and discrepancies should be reported as bugs.

* v0.6.2 (2019-03-08)
    * Fix [GitHub issue #23](https://github.com/dpranke/pyjson5/issues/23) and
      pass through unrecognized escape sequences.

* v0.6.1 (2018-05-22)
    * Cleaned up a couple minor nits in the package.

* v0.6.0 (2017-11-28)
    * First implementation that attempted to implement 100% of the spec.

* v0.5.0 (2017-09-04)
    * First implementation that supported the full set of kwargs that
      the `json` module supports.
