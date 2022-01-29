# SuperSize Tiger Viewer

Webapp for viewing `.size` and `.sizediff` files.

Hosted at: https://chrome-supersize.firebaseapp.com

[TOC]

## Running a Local Instance

To run the viewer locally:

1. Install [Firebase CLI]
2. Run `./upload_html_viewer.py --local`

The WebAssembly files are fetched from the deployed instance if you have not
built them locally. To build them, see [caspian/README.md].

*** note
**Note:** `index.html` dropdowns are not populated when running locally due to
CORS restrictions. We should fix this by having `index.html` use the JSON api to
perform authenticated fetches, like `viewer.html` does.
***

[Firebase CLI]: https://firebase.google.com/docs/cli#install_the_firebase_cli
[caspian/README.md]: /tools/binary_size/libsupersize/viewer/caspian/README.md

## Deployment Info

1. Test your change on a staging instance (which also does not support
   authenticated fetches)
   ```sh
   ./upload_html_viewer.py --staging
   ```
   * `index.html` dropdowns don't work due to CORS (same as for `--local`).
   * `viewer.html` authenticated fetches (`?load_url=` of
     non-`chromium-binary-size-trybot-results` URLs) do not work due to us being
     unable to allowlist the staging domain (which is random each time).

2. Deploy to prod instance ([see here] for details on who has permissions).
   ```sh
   ./upload_html_viewer.py --prod
   ```
3. Test your changes on the newly deployed instance (use DevTools to force
   Service Worker caching to update).

[see here]: https://docs.google.com/document/d/1qstcG9DxtwoohCnslvLs6Z7UfvO-dlkNi0s8PH-HYtI/edit?usp=sharing

## Code Overview

The viewer has no server component beyond static file serving. `.size` files
are parsed using WebAssembly (`caspian/` directory). The `.wasm` module runs
within a Web Worker in order to not block the browser's UI thread. Node
information is sent to the main page via JSON on-demand (when tree nodes
are expanded).

### Code Style

Code should follow Chrome's [styleguide] where possible.

[styleguide]: https://chromium.googlesource.com/chromium/src/+/main/styleguide/web/web.md

### static/index.html

This uses JSON files to populate the dropdowns:

 * `gs://chrome-supersize/milestones/milestones.json`
   * Updated by a Googler via: [`//tools/binary_size/generate_milestone_reports.py`]
 * `gs://chrome-supersize/official_builds/canary_reports.json`
   * Updated by official builders via: [`//tools/binary_size/generate_official_build_report.py`]

All `.size` files pointed to by this launcher are restricted to Googlers.

[`//tools/binary_size/generate_milestone_reports.py`]: /tools/binary_size/generate_milestone_reports.py
[`//tools/binary_size/generate_official_build_report.py`]: /tools/binary_size/generate_official_build_report.py

### static/viewer.html

This is the main WebApp. It uses a Service Worker (`sw.js`) and an App Manifest
(`manifest.json`) to be more slick (and work offline).

### Type Checking

The TypeScript Compiler is used to check JavaScript syntax and JSDoc. This is
manually invoked using:

``` bash
python3 tools/binary_size/libsupersize/viewer/check_js.py
```
