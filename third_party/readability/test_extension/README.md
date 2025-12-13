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

*   **Clone Page** (`Clone`): Captures the DOM of the active webpage, processes
    it into a self-contained HTML document, and opens it in a new tab. This
    provides a static, offline-ready, and refreshable version of the page for
    analysis.

*   **Check Readerable** (`Readerable?`): Performs a quick, preliminary check to
    see if a page is likely to be distillable, based on Mozilla's
    `isProbablyReaderable()` heuristic.

*   **Distill Page** (`Distill`): Replaces the content of the current tab with
    the distilled article. This is useful for debugging layout transitions, as
    it keeps the DevTools instance and any mobile simulation active. On a
    cloned page, this happens in-place, and refreshing the tab reverts to the
    original cloned state.

*   **Distill Page (New Tab)** (`Distill New`): Opens the distilled content in a
    new tab. This is ideal for side-by-side comparisons with the original page
    or for testing on dynamic pages where reloading is undesirable.

*   **Extract Metadata** (`Metadata`): Displays a table of the metadata
    extracted by Readability.js (e.g., title, byline, excerpt) in a new tab.

*   **Boxify DOM** (`Boxify`): On a cloned page, this adds a black outline to
    every single element. This effect can be undone by refreshing the page.

*   **Visualize Readability** (`Visualize`): On a cloned page, this feature
    provides a visual debugging tool to see exactly how Readability processed
    the page.
    *   Elements that were kept in the final article are highlighted with a
        green overlay.
    *   Elements that were discarded are highlighted with a red overlay.
    *   "Leaf" elements that were discarded (like images or paragraphs) are
        de-emphasized to avoid obscuring the view.
    *   This effect can be undone by refreshing the page.

*   **Context-Aware Menu**: The extension's popup menu is dynamically generated
    based on the current page, showing only the relevant actions.

## Setup

1.  **Get Required Files**
    This extension does not bundle the `Readability.js` library or the core
    Chromium `dom_distiller` components. Before loading the extension, you must
    make the necessary files available inside the extension's directory. From
    the `third_party/readability/test_extension/` directory, run the following
    commands to create symbolic links:

    ```bash
    # Link to (modded) Readability.js files
    ln -s ../modded_src/Readability.js .
    ln -s ../modded_src/Readability-readerable.js .

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
    *   `Clone`: Opens a self-contained, static version of the page in a new
        tab.
    *   `Readerable?`: Checks if the page is likely to be distillable and
        shows the result in the popup.
    *   `Distill`: Replaces the current page's content with the distilled
        article.
    *   `Distill New`: Opens the distilled content in a new tab.
    *   `Metadata`: Opens a new tab with a table of the extracted metadata.

### 2. From a Cloned Page

*   On a cloned page, click the extension's icon again to see the available
    actions:
    *   `Readerable?`: Checks if the page is likely to be distillable and
        shows the result in the popup.
    *   `Distill`: Replaces the cloned page's content with the distilled
        article *in-place*.
    *   `Distill New`: Opens the distilled content in a new tab.
    *   `Metadata`: Opens a new tab with a table of the extracted metadata.
    *   `Boxify`: Draws an outline around every element on the page.
    *   `Visualize`: Highlights the elements on the page to show which were
        kept and which were discarded by the Readability algorithm.

## File Structure

*   `manifest.json`: The extension's manifest file.

### Core Logic
*   `background.js`: The service worker that orchestrates script injection and
    the creation of new pages.
*   `popup.html` / `popup.js`: Defines the UI and logic for the extension's
    dynamically generated popup menu.

### Scripts Injected into Webpages
*   `extractor.js`: Injected to capture a webpage's DOM.
*   `distiller.js`: A lightweight script that invokes the shared
    `article_processor.js`.
*   `metadata_extractor.js`: Injected to extract metadata.
*   `readability_checker.js`: Injected to run the `isProbablyReaderable()`
    check.

### Extension Pages
*   `cloned.html` / `cloned.js`: The page and script for displaying a cloned
    webpage and handling its specific actions, including in-place distillation
    and visualization.
*   `viewer.html` / `viewer.js`: The page and script for displaying a
    distilled article.
*   `metadata.html` / `metadata.js` / `metadata.css`: The page, script, and
    styling for displaying the extracted metadata table.

### Shared Modules & Scripts
*   `article_processor.js`: A shared script that centralizes the main invocation
    of `Readability.parse()` and the rendering of the final article HTML.
*   `article_renderer.js`: A shared script used by both `cloned.js` and
    `viewer.js` to render a distilled article, ensuring consistent output and
    avoiding code duplication.
*   `metadata_processor.js`: A shared script that centralizes the extraction of
    metadata from a document.

### Third-Party & Core Chromium Files (Developer-provided)
*   `Readability.js`: The core library being tested.
*   `Readability-readerable.js`: The standalone script for the
    `isProbablyReaderable()` check.
*   `distilledpage_common.css` / `distilledpage_new.css`: Core CSS files for
    styling the distilled content view.
*   `dom_distiller_viewer.js`: Core JavaScript module with helper functions for
    rendering and interacting with the distilled content.
