# SuperSize Tiger Viewer

This is the source for SuperSize Tiger Viewer. The viewer runs entirely in
browser and has no server component beyond static file serving. `.size` files
are parsed using WebAssembly (`caspian/` directory) within a Web Worker, and
node information is sent to the main page via JSON on-demand (when tree nodes
are expanded).

The project is deployed at https://chrome-supersize.firebaseapp.com. Googlers:
see [this doc] for deployment instructions.

To run the viewer locally:

1. Install [Firebase CLI]
2. Run `./upload_html_viewer.py --local`

*** note
**Note:** Authentication does not work when running locally, so fetching `.size`
files from GCS does not work.
***

The WebAssembly files will be fetched from the deployed instance if you have
not built them locally. To build them, see [caspian/README.md].

[Firebase CLI]: https://firebase.google.com/docs/cli#install_the_firebase_cli
[this doc]: https://docs.google.com/document/d/1qstcG9DxtwoohCnslvLs6Z7UfvO-dlkNi0s8PH-HYtI/edit?usp=sharing
[caspian/README.md]: /tools/binary_size/libsupersize/viewer/caspian/README.md

## Developer Overview

### static/index.html

This uses JSON files to populate the dropdowns:

 * `gs://chrome-supersize/milestones/milestones.json`
   * Updated by a Googler via: [`//tools/binary_size/generate_milestone_reports.py`]
 * `gs://chrome-supersize/official_builds/canary_reports.json`
   * Updated by official builders via: [`//tools/binary_size/generate_official_build_report.py`]

All `.size` files pointed to by this launcher are restricted to Googlers.

[`//tools/binary_size/generate_milestone_reports.py`] /tools/binary_size/generate_milestone_reports.py
[`//tools/binary_size/generate_official_build_report.py`] /tools/binary_size/generate_official_build_report.py

### static/viewer.html

This is the viewer webapp. It uses a service worker (`sw.js`) and app manifest
(`manifest.json`) to be more slick (and work offline).
