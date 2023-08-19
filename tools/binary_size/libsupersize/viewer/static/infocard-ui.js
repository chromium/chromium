// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and methods for the info cards that display informations about
 * symbols as the user hovers or focuses on them.
 */

const displayInfocard = (() => {
  const _CANVAS_RADIUS = 40;

  const _FLAG_LABELS = new Map([
    [_FLAGS.ANONYMOUS, 'anon'],
    [_FLAGS.STARTUP, 'startup'],
    [_FLAGS.UNLIKELY, 'unlikely'],
    [_FLAGS.REL, 'rel'],
    [_FLAGS.REL_LOCAL, 'rel.loc'],
    [_FLAGS.GENERATED_SOURCE, 'gen'],
    [_FLAGS.CLONE, 'clone'],
    [_FLAGS.HOT, 'hot'],
    [_FLAGS.COVERAGE, 'covered'],
    [_FLAGS.UNCOMPRESSED, 'uncompressed'],
  ]);

  class Infocard {
    /** @param {!Element} infocardElt */
    constructor(infocardElt) {
      this._infocard = infocardElt;
      /** @type {HTMLSpanElement} */
      this._sizeInfo = this._infocard.querySelector('.size-info');
      /** @type {HTMLSpanElement} */
      this._addressInfo = this._infocard.querySelector('.address-info');
      /** @type {HTMLSpanElement} */
      this._paddingInfo = this._infocard.querySelector('.padding-info');
      /** @type {HTMLParagraphElement} */
      this._detailsInfo = this._infocard.querySelector('.details-info');
      /** @type {HTMLDivElement} */
      this._iconInfo = this._infocard.querySelector('.icon-info');
      /** @type {HTMLSpanElement} */
      this._typeInfo = this._infocard.querySelector('.type-info');
      /** @type {HTMLSpanElement} */
      this._flagsInfo = this._infocard.querySelector('.flags-info');

      /**
       * Last symbol type displayed.
       * Tracked to avoid re-cloning the same icon.
       * @type {string}
       */
      this._lastType = '';
    }

    /**
     * Displays the disassembly overlay.
     * @param {string} disassembly
     */
    _showDisassemblyOverlay(disassembly) {
      const divModal = g_el.divDisassemblyModal;
      const divCode = divModal.querySelector('.div-code');
      const linkDownload = /** @type {!HTMLAnchorElement} */ (
          divModal.querySelector('.link-download'));
      const btnClose = /** @type {!HTMLButtonElement} */ (
          divModal.querySelector('.btn-close'));
      const diffHtml = Diff2Html.html(disassembly, {
        drawFileList: false,
        matching: 'lines',
        outputFormat: 'side-by-side',
      });
      divCode.innerHTML = diffHtml;
      divModal.style.display = '';
      const blob = new Blob([disassembly], {type: 'text/plain'});
      const objectUrl = URL.createObjectURL(blob);
      linkDownload.href = objectUrl;
      btnClose.onclick = () => {
        URL.revokeObjectURL(objectUrl);
        divModal.style.display = 'none';
      };
    }

    /**
     * Updates the header, which normally displayed the byte size of the node
     * followed by an abbreviated version.
     *
     * Example: "1,234 bytes (1.23 KiB)"
     * @param {TreeNode} node
     */
    _updateHeader(node) {
      const sizeContents = getSizeContents(node);
      const sizeFragment = dom.createFragment([
        document.createTextNode(`${sizeContents.description} (`),
        sizeContents.element,
        document.createTextNode(')'),
      ]);

      const addressNodes = [];
      if ('address' in node) {
        const span = document.createElement('span');
        const addressHex = node.address.toString(16);
        span.textContent = `${node.type}@0x${addressHex}`;
        span.setAttribute('title', `${formatNumber(node.address)}`);
        addressNodes.push(span);
      }
      const addressFragment = dom.createFragment(addressNodes);

      const paddingNodes = [];
      if ('padding' in node) {
        const span = document.createElement('span');
        span.textContent = `Padding: ${formatNumber(node.padding, 0, 2)} bytes`;
        paddingNodes.push(span);
      }
      const paddingFragment = dom.createFragment(paddingNodes);

      // Update DOM
      setSizeClasses(
          this._sizeInfo, sizeContents.value, state.stMethodCount.get());
      dom.replace(this._sizeInfo, sizeFragment);
      dom.replace(this._addressInfo, addressFragment);
      dom.replace(this._paddingInfo, paddingFragment);
    }

    /**
     * Updates the details text, which shows the idPath for directory nodes, or
     * {container (if nonempty), srcPath, component, fullName} for symbol nodes.
     * @param {TreeNode} node
     */
    _updateDetails(node) {
      // List of window.Nodes, but called |elements| to avoid confusion.
      const elements = [];

      // srcPath is set only for leaf nodes.
      if (node.srcPath !== undefined) {
        const add_field = (title, text) => {
          const div = document.createElement('div');
          div.appendChild(dom.textElement('span', title, 'symbol-name-info'));
          div.appendChild(text.href ? text : document.createTextNode(text));
          elements.push(div);
        };
        if (node.container !== '') add_field('Container: ', node.container);
        add_field('Source Path: ', node.srcPath || '(No path)');
        add_field('Object Path: ', node.objPath || '(No path)');
        add_field('Component: ', node.component || '(No component)');
        add_field('Full Name: ', node.fullName || '');
        if (node.disassembly && node.disassembly !== '') {
          const eltAnchor = document.createElement('a')
          eltAnchor.appendChild(document.createTextNode('Show Disassembly'))
          eltAnchor.href = '#';
          eltAnchor.addEventListener('click', (e) => {
            e.preventDefault();
            this._showDisassemblyOverlay(node.disassembly)
          });
          add_field('Disassembly: ', eltAnchor);
        }

      } else {
        const path = node.idPath.slice(0, node.shortNameIndex);
        elements.push(document.createTextNode(path));
        const boldShortName = dom.textElement(
            'span', node.fullName || shortName(node), 'symbol-name-info');
        elements.push(boldShortName);
      }

      // Update DOM.
      dom.replace(this._detailsInfo, dom.createFragment(elements));
    }

    /**
     * Returns the type label of a node. By default this is pulled from the
     * title of the associated icon.
     * @param {TreeNode} node
     * @param {!SVGSVGElement} icon
     */
    _getTypeDescription(node, icon) {
      return icon.querySelector('title').textContent;
    }

    /**
     * @param {TreeNode} node
     * @return {!SVGSVGElement} The created icon.
     */
    _setTypeContent(node) {
      const icon = getIconTemplate(node.type[0]);
      icon.setAttribute('fill', '#fff');
      this._typeInfo.textContent = this._getTypeDescription(node, icon);
      this._iconInfo.replaceChild(icon, this._iconInfo.lastElementChild);
      return icon;
    }

    /**
     * Returns a string representing the flags in the node.
     * @param {TreeNode} node
     */
    _flagsString(node) {
      if (!node.flags) {
        return '';
      }

      const flagsString = Array.from(_FLAG_LABELS)
        .filter(([flag]) => hasFlag(flag, node))
        .map(([, part]) => part)
        .join(',');
      return `{${flagsString}}`;
    }

    /**
     * Toggle wheter or not the card is visible.
     * @param {boolean} isHidden
     */
    setHidden(isHidden) {
      if (isHidden) {
        this._infocard.setAttribute('hidden', '');
      } else {
        this._infocard.removeAttribute('hidden');
      }
    }

    /**
     * Updates the DOM for the info card.
     * @param {TreeNode} node
     */
    _updateInfocard(node) {
      const type = node.type[0];

      // Update DOM
      this._updateHeader(node);
      this._updateDetails(node);
      // If possible, skip making new type content.
      if (type !== this._lastType || type === _ARTIFACT_TYPES.GROUP) {
        this._setTypeContent(node);
        this._lastType = type;
      }
      this._flagsInfo.textContent = this._flagsString(node);
    }

    /**
     * Updates the card on the next animation frame.
     * @param {TreeNode} node
     */
    updateInfocard(node) {
      cancelAnimationFrame(Infocard._pendingFrame);
      Infocard._pendingFrame = requestAnimationFrame(() =>
        this._updateInfocard(node)
      );
    }
  }
  /** @type {number} */
  Infocard._pendingFrame = 0;

  class SymbolInfocard extends Infocard {
    /**
     * @param {TreeNode} node
     * @return {!SVGSVGElement} The created icon.
     */
    _setTypeContent(node) {
      const icon = super._setTypeContent(node);
      this._iconInfo.style.backgroundColor = getIconStyle(node.type[0]).color;
      return icon;
    }
  }

  class ArtifactInfocard extends Infocard {
    /** @param {!Element} infocardElt */
    constructor(infocardElt) {
      super(infocardElt);
      this._tableBody = this._infocard.querySelector('tbody');
      this._tableHeader = this._infocard.querySelector('thead');
      this._ctx = this._infocard.querySelector('canvas').getContext('2d');

      /**
       * @type {{[type:string]: HTMLTableRowElement}} Rows in the artifact
       * infocard that represent a particular symbol type.
       */
      this._infoRows = {
        a: this._tableBody.querySelector('.arsc-info'),
        b: this._tableBody.querySelector('.bss-info'),
        d: this._tableBody.querySelector('.data-info'),
        r: this._tableBody.querySelector('.rodata-info'),
        t: this._tableBody.querySelector('.text-info'),
        R: this._tableBody.querySelector('.relro-info'),
        x: this._tableBody.querySelector('.dexother-info'),
        m: this._tableBody.querySelector('.dexmethod-info'),
        p: this._tableBody.querySelector('.pak-info'),
        P: this._tableBody.querySelector('.paknon-info'),
        o: this._tableBody.querySelector('.other-info'),
      };

      /**
       * Update the DPI of the canvas for zoomed in and high density screens.
       */
      const _updateCanvasDpi = () => {
        this._ctx.canvas.height = _CANVAS_RADIUS * 2 * devicePixelRatio;
        this._ctx.canvas.width = _CANVAS_RADIUS * 2 * devicePixelRatio;
        this._ctx.scale(devicePixelRatio, devicePixelRatio);
      };

      _updateCanvasDpi();
      window.addEventListener('resize', _updateCanvasDpi);
    }

    /**
     * @param {TreeNode} node
     * @param {!SVGSVGElement} icon
     */
    _getTypeDescription(node, icon) {
      const depth = node.idPath.replace(/[^/]/g, '').length;
      if (depth === 0) {
        const t = /** @type {string} */ (state.stGroupBy.get());
        if (t) {
          // Format, e.g., "generated_type" to "Generated type".
          return (t[0].toUpperCase() + t.slice(1)).replace(/_/g, ' ');
        }
      }
      return super._getTypeDescription(node, icon);
    }

    /**
     * @param {TreeNode} node
     * @return {!SVGSVGElement} The created icon.
     */
    _setTypeContent(node) {
      const icon = super._setTypeContent(node);
      icon.classList.add('canvas-overlay');
      return icon;
    }

    _flagsString(artifactNode) {
      const flags = super._flagsString(artifactNode);
      return flags ? `- contains ${flags}` : '';
    }

    /**
     * Draw a border around part of a pie chart.
     * @param {number} angleStart Starting angle, in radians.
     * @param {number} angleEnd Ending angle, in radians.
     * @param {string} strokeColor Color of the pie slice border.
     * @param {number} lineWidth Width of the border.
     */
    _drawBorder(angleStart, angleEnd, strokeColor, lineWidth) {
      this._ctx.strokeStyle = strokeColor;
      this._ctx.lineWidth = lineWidth;
      this._ctx.beginPath();
      this._ctx.arc(40, 40, _CANVAS_RADIUS, angleStart, angleEnd);
      this._ctx.stroke();
    }

    /**
     * Draw a slice of a pie chart.
     * @param {number} angleStart Starting angle, in radians.
     * @param {number} angleEnd Ending angle, in radians.
     * @param {string} fillColor Color of the pie slice.
     */
    _drawSlice(angleStart, angleEnd, fillColor) {
      // Update DOM
      this._ctx.fillStyle = fillColor;
      // Move cursor to center, where line will start
      this._ctx.beginPath();
      this._ctx.moveTo(40, 40);
      // Move cursor to start of arc then draw arc
      this._ctx.arc(40, 40, _CANVAS_RADIUS, angleStart, angleEnd);
      // Move cursor back to center
      this._ctx.closePath();
      this._ctx.fill();
    }

    /**
     * Update a row in the breakdown table with the given values.
     * @param {HTMLTableRowElement} row
     * @param {?TreeNodeChildStats} stats Total size of the symbols of a given
     *   type in the artifact.
     * @param {number} percentage How much the size represents in relation to
     *   the total size of the symbols in the artifact.
     */
    _updateBreakdownRow(row, stats, percentage) {
      if (!stats?.size) {  // Subsumes |size| === 0.
        if (row.parentElement) {
          this._tableBody.removeChild(row);
        }
        return;
      }

      const countColumn = row.querySelector('.count');
      const sizeColumn = row.querySelector('.size');
      const percentColumn = row.querySelector('.percent');
      const addedColumn = row.querySelector('.added');
      const removedColumn = row.querySelector('.removed');
      const changedColumn = row.querySelector('.changed');

      const countString = formatNumber(stats.count);
      const sizeString = formatNumber(stats.size, 2, 2);
      const percentString = formatPercent(percentage, 2, 2);

      const diffMode = state.getDiffMode();
      if (diffMode && stats.added !== undefined) {
        addedColumn.removeAttribute('hidden');
        removedColumn.removeAttribute('hidden');
        changedColumn.removeAttribute('hidden');
        countColumn.setAttribute('hidden', '');

        addedColumn.textContent = formatNumber(stats.added);
        removedColumn.textContent = formatNumber(stats.removed);
        changedColumn.textContent = formatNumber(stats.changed);
      } else {
        addedColumn.setAttribute('hidden', '');
        removedColumn.setAttribute('hidden', '');
        changedColumn.setAttribute('hidden', '');
        countColumn.removeAttribute('hidden');
      }

      // Update DOM
      countColumn.textContent = countString;
      sizeColumn.textContent = sizeString;
      percentColumn.textContent = percentString;
      this._tableBody.appendChild(row);
    }

    /**
     * Update DOM for the artifact infocard
     * @param {TreeNode} artifactNode
     */
    _updateInfocard(artifactNode) {
      const extraRows = Object.assign({}, this._infoRows);
      const statsEntries = Object.entries(artifactNode.childStats).sort(
        (a, b) => b[1].size - a[1].size
      );
      const diffMode = state.getDiffMode();
      let totalSize = 0;
      for (const [, stats] of statsEntries) {
        totalSize += Math.abs(stats.size);
      }

      const countColumn = this._tableHeader.querySelector('.count');
      const addedColumn = this._tableHeader.querySelector('.added');
      const removedColumn = this._tableHeader.querySelector('.removed');
      const changedColumn = this._tableHeader.querySelector('.changed');

      // The WebAssembly worker supports added/removed/changed in diff view,
      // so displaying count isn't useful.
      // In non-diff view, we don't have added/removed/changed information, so
      // we just display a count.
      if (diffMode && statsEntries.length > 0 &&
          statsEntries[0][1].added !== undefined) {
        addedColumn.removeAttribute('hidden');
        removedColumn.removeAttribute('hidden');
        changedColumn.removeAttribute('hidden');
        countColumn.setAttribute('hidden', '');
      } else {
        addedColumn.setAttribute('hidden', '');
        removedColumn.setAttribute('hidden', '');
        changedColumn.setAttribute('hidden', '');
        countColumn.removeAttribute('hidden');
      }

      // Update DOM
      super._updateInfocard(artifactNode);
      let angleStart = 0;
      for (const [type, stats] of statsEntries) {
        delete extraRows[type];
        const {color} = getIconStyle(type);
        const percentage = stats.size / totalSize;
        this._updateBreakdownRow(this._infoRows[type], stats, percentage);

        const arcLength = Math.abs(percentage) * 2 * Math.PI;
        if (arcLength > 0) {
          const angleEnd = angleStart + arcLength;

          this._drawSlice(angleStart, angleEnd, color);
          if (diffMode) {
            const strokeColor = stats.size > 0 ? '#ea4335' : '#34a853';
            this._drawBorder(angleStart, angleEnd, strokeColor, 16);
          }
          angleStart = angleEnd;
        }
      }

      // Hide unused types
      for (const row of Object.values(extraRows)) {
        this._updateBreakdownRow(row, null, 0);
      }
    }
  }

  const _artifactInfo = new ArtifactInfocard(g_el.divInfocardArtifact);
  const _symbolInfo = new SymbolInfocard(g_el.divInfocardSymbol);

  /**
   * Displays an infocard for the given symbol on the next frame.
   * @param {TreeNode} node
   */
  function displayInfocard(node) {
    if (_ARTIFACT_TYPE_SET.has(node.type[0])) {
      _artifactInfo.updateInfocard(node);
      _artifactInfo.setHidden(false);
      _symbolInfo.setHidden(true);
    } else {
      _symbolInfo.updateInfocard(node);
      _symbolInfo.setHidden(false);
      _artifactInfo.setHidden(true);
    }
  }

  return displayInfocard;
})();
