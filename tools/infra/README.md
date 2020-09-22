Directory of scripts needed for troopering.

#### Mass cancelling builds and tasks

To cancel many builds at once use the following command:

```
# Cancel scheduled builds in bucket "bucket".
bb ls -id -status scheduled chromium/bucket | bb cancel -reason unnecessary

# Cancel started builds for CI builder "builder".
bb ls -id -status started chromium/ci/builder | bb cancel -reason "bad builds"
```
