Directory of scripts needed for troopering.

#### Mass cancelling builds and tasks

If you're cancelling builds because of a bad chromium/src revision, use the
`find_bad_builds.py` script. Example:

```
# Assuming that deadbeef is the git revision of the revert of 12345678, which
# landed and broke something.
./find_bad_builds.py deadbeef 12345678 | bb cancel -reason "CQ outage, see
crbug.com/XXXXXX"
```

To cancel many builds at once use the following command:

```
# Cancel scheduled builds in bucket "bucket".
bb ls -id -status scheduled chromium/bucket | bb cancel -reason unnecessary

# Cancel started builds for CI builder "builder".
bb ls -id -status started chromium/ci/builder | bb cancel -reason "bad builds"
```
