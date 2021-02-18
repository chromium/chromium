## Building the annotation checker.
We do not want every developer to have to build the auditor, and so we store the
pre-built binary in a Google Cloud Storage bucket and retrieve it via gclient
hooks.

To roll new versions of the binaries, you need to have write access to the
chromium-tools-traffic_annotation bucket. If you don't, contact the OWNERS list
in this folder, otherwise run:

# On Linux:
```bash
git new-branch roll_traffic_annotation_tools

# These GN flags produce an optimized, stripped binary that has no dependency
# on glib.
gn gen --args='is_official_build=true use_ozone=true' out/Default

ninja -C out/Default traffic_annotation_auditor
cp -p out/Default/traffic_annotation_auditor \
    tools/traffic_annotation/bin/linux64

strip tools/traffic_annotation/bin/linux64/traffic_annotation_auditor

third_party/depot_tools/upload_to_google_storage.py \
    -b chromium-tools-traffic_annotation \
    tools/traffic_annotation/bin/linux64/traffic_annotation_auditor
sed -i '/^LASTCHANGE=/d' tools/traffic_annotation/bin/README.md
cat build/util/LASTCHANGE >> tools/traffic_annotation/bin/README.md
git commit -a -m 'Roll traffic_annotation checkers'
git cl upload

```

# On Windows:
```bash
git new-branch roll_traffic_annotation_tools

# These GN flags produce an optimized, stripped binary that has no dependency
# on glib.
gn gen --args="is_official_build=true" out/Default

ninja -C out/Default traffic_annotation_auditor
cp -p out/Default/traffic_annotation_auditor.exe ^
    tools/traffic_annotation/bin/win32

python third_party/depot_tools/upload_to_google_storage.py ^
    -b chromium-tools-traffic_annotation ^
    tools/traffic_annotation/bin/win32/traffic_annotation_auditor.exe
sed -i "/^LASTCHANGE=/d" tools/traffic_annotation/bin/README.md
cat build/util/LASTCHANGE >> tools/traffic_annotation/bin/README.md
dos2unix tools/traffic_annotation/bin/README.md
git commit -a -m 'Roll traffic_annotation checkers'
git cl upload

```

and land the resulting CL.

The following line will be updated by the above script, and the modified
README should be committed along with the updated .sha1 checksums.

LASTCHANGE=29828294b2aec6ace08ea0136cb7fcf164d5e166-refs/heads/master@{#854268}
