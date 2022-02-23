# //tools/python

This directory is meant to contain Python code that is:

1) Not platform-specific (e.g. `//tools/android/python_utils`).
2) Useful to multiple other tools.

All Python code that is used by GN actions or templates within `//build` must
live under `//build`, since that directory cannot have deps outside of it.
However, code here can be used by GN actions or templates that live outside of
`//build`.

When adding code to this directory, or when adding a dep onto code that lives in
this directory, please consider whether or not duplicating the code would
actually a better choice. Code re-use is helpful, but dependencies also come
with a cost, especially when it comes to being able to test changes to shared
code.
