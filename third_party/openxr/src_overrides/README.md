# OpenXR src_overrides

This folder is intended to contain forked files of changes that we have made to
openxr files. The goal should be to eventually upstream these changes.

`patches/` contains .patch files generated via `git format-patch` and should be
generated based off of commits that *only* have changes to the `src_overrides`
file and should be generated *after* the file is forked. Sequential ordering
should be maintained (renaming the leading number if needed). Patches and files
should be removed when they no longer need to be forked due to consuming an
upstreamed version of the change. After a roll, the new version of the files can
be copied into `src_overrides` and the patches can be applied via `git am`. If
there are merge conflicts, patches should be re-exported, with care taken to
*try* to maintain only one upstreamable change per patch.

`//third_party/openxr/fork_overrides.py` can help with "resetting" the state of
the override files to give a clean attempt to try to run
`git am src_overrides/patches/*` before either making new changes for the
patches, or simply for a roll. Note that descriptions of changes going into
src_overrides should be made in `//third_party/openxr/README.chromium`.
