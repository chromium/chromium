# Process for rolling new versions of UnRAR

The Crowbar workflow in crowbar.txtpb and copy.bara.sky imports src/
from the UnRAR source tarball and applies patches/chromium_changes.patch.
Crowbar cannot detect new UnRAR releases yet (b/552678988), so a roll
starts by hand when the "UnRAR source" link on
https://www.rarlab.com/rar_add.htm changes. Let `$url` be the new link.

In crowbar.txtpb, set `url` and `name` to the new tarball, `digest` to
`sha256:` plus the output of `curl -sfL $url | sha256sum`, and `version`
to the version in unrar/version.hpp of the tarball. Then run from the
Chromium root:
```
crowbar build --no-dry-run ./third_party/unrar
```

If chromium_changes.patch no longer applies, remove the `patch.apply`
line from copy.bara.sky, run the command above, and commit the result.
Let `$raw_src_hash` be that commit. Put the `patch.apply` line back and
apply the Chromium patch file. This command will apply the hunks that
apply, and create .rej files in //third_party/unrar/src for hunks that
do not:
```
git apply --reject --directory=third_party/unrar third_party/unrar/patches/chromium_changes.patch
```

For each .rej file, identify whether it still applies and manually fix
the conflict, then delete the .rej file. Then regenerate the
`chromium_changes.patch` file with:
```
git diff --relative=third_party/unrar --src-prefix=a/ --dst-prefix=b/ $raw_src_hash -- third_party/unrar/src > third_party/unrar/patches/chromium_changes.patch
```
and run the crowbar command again; it should leave src/ unchanged.

copy.bara.sky lists every file it imports, and crowbar skips new
upstream files without a warning. If the new version adds a file
Chromium needs, the roll fails to build: a missing header, or undefined
symbols for a missing .cpp. Add that file to copy.bara.sky and BUILD.gn,
then run the crowbar command again to import it.

Update README.chromium to mention the new version and source URL and
upload a CL. You did it!
