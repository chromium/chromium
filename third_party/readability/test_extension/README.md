*   `article_renderer.js`: A shared script used by both `cloned.js` and
    `viewer.js` to render a distilled article, ensuring consistent output and
    avoiding code duplication.
*   `utils.js`: A shared script for common helper functions used across
    different extension pages.

### Third-Party & Core Chromium Files (Developer-provided)

## Overview

This is a Chromium extension that serves as a development and testing tool for
Mozilla's [Readability.js](https://github.com/mozilla/readability) library.

It provides a suite of tools for developers and testers to analyze how Mozilla's
Readability.js library processes web pages. This provides insight into the
distillation algorithm.

## Features

*   **Clone**: Captures the DOM of the active webpage, processes it into a
    self-contained HTML document, and opens it in a new tab. This provides a
    static, offline-ready, and refreshable version of the page for analysis.

*   **Readerable?**: Performs a quick, preliminary check to see if a page is
    likely to be distillable, based on Mozilla's `isProbablyReaderable()`
    heuristic.

*   **Distill**: Replaces the content of the current tab with the distilled
    article. This is useful for debugging layout transitions, as it keeps the
    DevTools instance and any mobile simulation active. On a cloned page, this
    happens in-place, and refreshing the tab reverts to the original cloned
    state.

*   **Distill New**: Opens the distilled content in a new tab. This is ideal
    for side-by-side comparisons with the original page or for testing on
    dynamic pages where reloading is undesirable.

## Setup

1.  **Get Required Files**
    This extension does not bundle the `Readability.js` library or the core
    Chromium `dom_distiller` components. Before loading the extension, you must
    make the necessary files available inside the extension's directory. From
    the `third_party/readability/test_extension/` directory, run the following
    commands to create symbolic links:

    ```bash
    # Link to Readability.js files
    ln -s ../src/Readability.js .
    ln -s ../src/Readability-readerable.js .

    # Link to dom_distiller core files
    ln -s ../../../components/dom_distiller/core/css/distilledpage_common.css .
    ln -s ../../../components/dom_distiller/core/css/distilledpage_new.css .
    ln -s ../../../components/dom_distiller/core/javascript/dom_distiller_viewer.js .
    ```

2.  **Load the Extension in Chrome**
    *   Open Chrome and navigate to `chrome://extensions`.
    *   Enable "Developer mode" using the toggle in the top-right corner.
    *   Click the "Load unpacked" button.
    *   Select the `third_party/readability/test_extension` directory.
    *   The extension should now be loaded and ready to use.

## How to Use

### 1. From a Live Webpage

*   Navigate to any article or webpage you want to test.
*   Click the extension's icon in the Chrome toolbar to see the available
    actions:
    *   **"Clone"**: Opens a self-contained, static version of the page in a new
        tab.
    *   **"Readerable?"**: Checks if the page is likely to be distillable and
        shows the result in the popup.
    *   **"Distill"**: Replaces the current page's content with the distilled
        article.
    *   **"Distill New"**: Opens the distilled content in a new tab.

### 2. From a Cloned Page

*   On a cloned page, click the extension's icon again to see the available
    actions:
    *   **"Readerable?"**: Checks if the page is likely to be distillable and
        shows the result in the popup.
    *   **"Distill"**: Replaces the cloned page's content with the distilled
        article *in-place*.
    *   **"Distill New"**: Opens the distilled content in a new tab.
