individual per-changelog files go here
in .rst format, which are pulled in by
changelog to
be rendered into the changelog.rst file.
At release time, the files here are removed and written
directly into the changelog.

Rationale is so that multiple changes being merged
into gerrit don't produce conflicts.   Note that
gerrit does not support custom merge handlers unlike
git itself.


