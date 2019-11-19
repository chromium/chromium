GN now lives at https://gn.googlesource.com/.

To roll the latest version of GN into Chromium:

1. Create a new Git branch.
2. Run `python //tools/gn/roll_gn.py`; this will modify //DEPS and
   //buildtools/DEPS to point to the most recent revision of GN and
   create a commit with a list of the changes included in the roll.
3. Run `git-cl upload` to upload the commit as a change for review.

If you don't want to roll to the latest version, you can specify the SHA-1
to roll to use as an argument to the script, and it will be used instead.
