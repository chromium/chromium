// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This script is injected on-demand into the active tab. It clones the DOM,
 * processes it to be self-contained, and returns the serialized HTML along with
 * critical computed styles.
 */
(async () => {
  /**
   * Converts a relative URL to an absolute URL.
   * @param {string} url The URL to resolve.
   * @return {string} The resolved, absolute URL.
   */
  function toAbsoluteURL(url) {
    try {
      if (url.startsWith('data:') || url.startsWith('blob:') ||
          url.startsWith('#')) {
        return url;
      }
      return new URL(url, document.baseURI).href;
    } catch (e) {
      return url;
    }
  }

  /**
   * Recursively reads all CSS rules from a stylesheet, handling @import rules.
   * @param {CSSStyleSheet} sheet The stylesheet to read.
   * @param {string} baseUrl The base URL for resolving relative @import paths.
   * @return {Promise<string>} A promise that resolves to the aggregated CSS
   *     text.
   */
  async function getCssText(sheet, baseUrl) {
    let cssText = '';
    try {
      const rules = sheet.cssRules;
      for (const rule of rules) {
        if (rule instanceof CSSImportRule) {
          const importedSheetUrl = new URL(rule.href, baseUrl).href;
          try {
            const response = await fetch(importedSheetUrl);
            if (response.ok) {
              cssText += await response.text();
            }
          } catch (e) {
            console.log(
                '%cCould not fetch imported stylesheet: ' + importedSheetUrl,
                'color: red;', e);
          }
        } else {
          cssText += rule.cssText;
        }
      }
    } catch (e) {
      console.log(
          '%cCould not read cssRules from stylesheet: ' + sheet.href,
          'color: red;', e);
    }
    return cssText;
  }

  /**
   * Gathers all stylesheets in the document into a single, aggregated CSS
   * string.
   * @param {Document} doc The document to extract stylesheets from.
   * @return {Promise<string>} A promise that resolves to the complete CSS
   *     text.
   */
  async function aggregateStylesheets(doc) {
    const sheetPromises = Array.from(doc.styleSheets).map(
        sheet => getCssText(sheet, sheet.href || doc.baseURI));
    const cssTexts = await Promise.all(sheetPromises);
    return cssTexts.join('\n');
  }

  /**
   * Iterates through all elements and attributes in the document to convert
   * any relative URLs to absolute URLs.
   * @param {Document} docClone The cloned document to process.
   */
  function absolutifyUrls(docClone) {
    const allElements = docClone.querySelectorAll('*');
    for (const element of allElements) {
      for (const attr of element.attributes) {
        const attrName = attr.name.toLowerCase();
        let attrValue = attr.value;

        if (!attrValue) continue;

        if (['href', 'src', 'poster', 'data-src'].includes(attrName)) {
          attrValue = toAbsoluteURL(attrValue);
        } else if (attrName === 'srcset') {
          const srcsetRegex = /\s*(\S+)(.*?)(?:,|$)/g;
          const newCandidates = [];
          let match;
          while ((match = srcsetRegex.exec(attrValue))) {
            const url = match[1];
            const descriptors = (match[2] || '').trim();
            if (url) {
              newCandidates.push(`${toAbsoluteURL(url)} ${descriptors}`);
            }
          }
          attrValue = newCandidates.join(', ');
        } else if (attrName === 'style' && attrValue.includes('url(')) {
          attrValue = attrValue.replace(
              /url\((['"]?)(.*?)\1\)/gi,
              (match, quote, url) =>
                  `url(${quote}${toAbsoluteURL(url)}${quote})`);
        }
        element.setAttribute(attrName, attrValue);
      }

      // Promote common lazy-loading attributes.
      if (element.hasAttribute('data-src') && !element.getAttribute('src')) {
        element.setAttribute('src', element.getAttribute('data-src'));
      }
      if (element.hasAttribute('data-srcset') &&
          !element.getAttribute('srcset')) {
        element.setAttribute('srcset', element.getAttribute('data-srcset'));
      }
    }
  }

  /**
   * Reconstructs the document's DOCTYPE declaration as a string.
   * @param {Document} doc The document whose DOCTYPE should be reconstructed.
   * @return {string} The DOCTYPE as a string.
   */
  function reconstructDoctype(doc) {
    const doctype = doc.doctype;
    if (!doctype) {
      return '';
    }
    let str = `<!DOCTYPE ${doctype.name}`;
    if (doctype.publicId) {
      str += ` PUBLIC "${doctype.publicId}"`;
    }
    if (!doctype.publicId && doctype.systemId) {
      str += ' SYSTEM';
    }
    if (doctype.systemId) {
      str += ` "${doctype.systemId}"`;
    }
    return str + '>\n';
  }

  /******** Main Execution ********/

  // 1. Capture essential computed styles from the original page.
  const originalRootStyle = window.getComputedStyle(document.documentElement);
  const originalBodyStyle = window.getComputedStyle(document.body);
  const styles = {
    rootFontSize: originalRootStyle.fontSize,
    bodyFontSize: originalBodyStyle.fontSize,
    bodyBackgroundColor: originalBodyStyle.backgroundColor,
    bodyColor: originalBodyStyle.color,
  };

  // 2. Create a deep clone of the document to work on.
  const docClone = document.cloneNode(true);

  // 3. Process the clone.
  const allCss = await aggregateStylesheets(document);
  absolutifyUrls(docClone);

  // 4. Clean and modify the cloned DOM.
  docClone.querySelectorAll('script, link[rel="stylesheet"], style')
      .forEach(el => el.remove());
  const newStyle = docClone.createElement('style');
  newStyle.textContent = allCss;
  docClone.head.appendChild(newStyle);

  // 5. Serialize the final result.
  const doctypeString = reconstructDoctype(document);
  const finalHtml = doctypeString + docClone.documentElement.outerHTML;

  // 6. Return the complete package.
  return {html: finalHtml, styles: styles};
})();
