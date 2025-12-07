# Process for rolling new versions of UnRAR

Let `$url` be the URL with the current version of UnRAR, and do the
following to overwrite the current source with the new source:
```
wget -O unrar.tar.gz $url
rm -r third_party/unrar/src/
tar xf unrar.tar.gz
mv unrar/ third_party/unrar/src
rm unrar.tar.gz
```

Commit these changes and upload a patchset. This will make it possible
to generate the patch file later, and make review easier. Let
`$raw_src_hash` be the resulting git hash.

Now apply the Chromium patch file. This command will apply the hunks
that apply, and create .rej files in //third_party/unrar/src for hunks
that do not:
```
git apply --reject third_party/unrar/patches/chromium_changes.patch
```

For each .rej file, identify whether it still applies and manually fix
the conflict, then delete the .rej file. Then regenerate the
`chromium_changes.patch` file with:
```
git diff $raw_src_hash -- third_party/unrar/src/* > third_party/unrar/patches/chromium_changes.patch
```

Update README.chromium to mention the new version and source URL and
upload a CL. You did it!
