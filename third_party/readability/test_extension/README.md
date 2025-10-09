# Readability.js Test Extension

## Overview

This is a Chromium extension that serves as a development and testing tool for
Mozilla's [Readability.js](https://github.com/mozilla/readability) library.

It provides a suite of tools for developers and testers to analyze how Mozilla's
Readability.js library processes web pages. This provides insight into the
distillation algorithm.

## Features

*   **Clone Page**: Captures the DOM of the active webpage, processes it into a
    self-contained HTML document, and opens it in a new tab. This provides a
    static, offline-ready, and refreshable version of the page for analysis.
*   **Readerable?**: Performs a quick, preliminary check to see if a page is
    likely to be distillable, based on Mozilla's `isProbablyReaderable()`
    heuristic.

## Setup

1.  **Get Readability Files**
    This extension does not bundle the `Readability.js` library. Before loading
    the extension, you must make the necessary files available inside the
    extension's directory. From the `third_party/readability/test_extension/`
    directory, run the following command:

    *   **Create a symbolic link (recommended):**
        ```bash
        ln -s ../src/Readability-readerable.js .
        ```

2.  **Load the Extension in Chrome**
    *   Open Chrome and navigate to `chrome://extensions`.
    *   Enable "Developer mode" using the toggle in the top-right corner.
    *   Click the "Load unpacked" button.
    *   Select the `third_party/readability/test_extension` directory.
    *   The extension should now be loaded and ready to use.

## How to Use

*   Navigate to any article or webpage you want to test.
*   Click the extension's icon in the Chrome toolbar to see the available
    actions:
    *   **"Clone"**: Opens a self-contained, static version of the page in a new
        tab.
    *   **"Readerable?"**: Checks if the page is likely to be distillable and
        shows the result in the popup.
