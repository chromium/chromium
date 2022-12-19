# Merging Size Diffs

When looking at a diff across milestones, it can be useful to filter out the
size impacts due to toolchain changes (e.g. compiler or R8 rolls).

You will need:
1) The `.size` files of the milestones you'd like to diff.
2) The `.sizediff` files of the commits to filter out.

Then run:

```
tools/binary_size/create_patched_supersize_diff.py before_size.size load_size.size *.sizediff --output m97_m98_normalized.sizediff
```

And then run the `gsutil.py` command that it prints out.
