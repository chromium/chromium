# Modded Readability.js

This directory contains Chromium's locally modified version of Mozilla's
`Readability.js` and `Readability-readerable.js`. These scripts are used by
the DOM Distiller component to extract the main content of web pages for
Reader Mode.

Modifications to the upstream library should be done directly in these files.
Always ensure that regexes and common logic are kept in sync between
`Readability.js` and `Readability-readerable.js`.

## Readability Debugging Tool

This directory also provides `run_readability.cjs`, a lightweight command-line
script to test and debug the distillation logic against arbitrary HTML files
without needing a full Chromium build.

### Prerequisites

The runner requires `jsdom` to simulate a browser environment in Node.js. To
install it locally within this directory (without committing to the tree), run:

```bash
cd third_party/readability/modded_src
npm install --prefix . jsdom
```

### Usage

Run the script using `node` (specifically `.cjs` for CommonJS compatibility).

**Tip:** Save your test page as `input.html` and distilled results as
`output.html` in this directory. These filenames are already ignored by git.

```bash
cd third_party/readability/modded_src
node run_readability.cjs input.html output.html
```

*   `<input.html>`: Path to the raw HTML file you wish to distill.
*   `[output.html]`: (Optional) Path to save the distilled HTML content.
    If omitted, content is printed to standard output.

### Capturing Debug Output

Readability's internal debug logs (which trace scoring and node removal) are
printed to `stderr` during execution. Capture them by redirecting output:

```bash
node run_readability.cjs input.html output.html 2> debug.log
```

### AI-Assisted Debugging

This script is ideal for command-line AI agents (like Gemini CLI) to
autonomously test distillation fixes:
1. Obtain the raw HTML of the problematic page.
2. Run `run_readability.cjs input.html output.html 2> debug.log`.
3. Analyze `debug.log` to understand node scoring or regex matching.
4. Modify `Readability.js` and re-run to verify the fix.

### Caveats
*   **No JavaScript Execution:** `jsdom` does not execute scripts. `<noscript>`
    tags may be treated as active content.
*   **Post-processing:** This script *only* runs `Readability.js`. Downstream
    Chromium post-processing steps are not applied.
