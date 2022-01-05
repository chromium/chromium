# Merging Size Diffs

When looking at a diff across milestones, it can be useful to filter out the
size impacts due to toolchain changes (e.g. compiler or R8 rolls).

You will need:
1) The `.size` files of the milestones you'd like to diff.
2) The `.sizediff` files of the commits to filter out.

Then run:

```
tools/binary_size/supersize console M97.size M98.size clang_roll.sizediff
...
  size_info1: Loaded from 97.size
  size_info2: Loaded from 98.size
  size_info3: Loaded from clang_roll.sizediff
  size_info4: Loaded from clang_roll.sizediff

>>> d = Diff(size_info1, size_info2)
>>> d2 = Diff(size_info4, size_info3)  # Note reversed order.
>>> d.raw_symbols += d2.raw_symbols
>>> SaveDeltaSizeInfo(d, 'm97_m98_normalized.sizediff')
Saved locally to m97_m98_normalized.sizediff. To share, run:
> gsutil.py cp m97_m98_normalized.sizediff gs://chrome-supersize/private-oneoffs
  Then view it at https://chrome-supersize.firebaseapp.com/viewer.html?load_url=https://storage.googleapis.com/chrome-supersize/private-oneoffs/m97_m98_normalized.sizediff
```
