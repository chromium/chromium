// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
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
    /**
     * @param {string} id
     */
    constructor(id) {
      this._infocard = document.getElementById(id);
      /** @type {HTMLHeadingElement} */
      this._sizeInfo = this._infocard.querySelector('.size-info');
      /** @type {HTMLParagraphElement} */
      this._pathInfo = this._infocard.querySelector('.path-info');
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
     * Updates the size header, which normally displayed the byte size of the
     * node followed by an abbreviated version.
     *
     * Example: "1,234 bytes (1.23 KiB)"
     * @param {TreeNode} node
     */
    _updateSize(node) {
      const {description, element, value} = getSizeContents(node);
      const sizeFragment = dom.createFragment([
        document.createTextNode(`${description} (`),
        element,
        document.createTextNode(')'),
      ]);

      // Update DOM
      setSizeClasses(this._sizeInfo, value);

      dom.replace(this._sizeInfo, sizeFragment);
    }

    /**
     * Updates the path text, which shows the idPath for directory nodes, and
     * srcPath / component for symbol nodes.
     * @param {TreeNode} node
     */
    _updatePaths(node) {
      let pathFragment;
      if (node.srcPath) {
        pathFragment = dom.createFragment([
            dom.textElement('span', 'Path: ', 'symbol-name-info'),
            document.createTextNode(node.srcPath),
            document.createElement('br'),
            dom.textElement('span', 'Component: ', 'symbol-name-info'),
            document.createTextNode(node.componet || '(No component)'),
        ]);
      } else {
        const path = node.idPath.slice(0, node.shortNameIndex);
        const boldShortName = dom.textElement(
            'span', node.fullName || shortName(node), 'symbol-name-info');
        pathFragment = dom.createFragment([
          document.createTextNode(path),
          boldShortName,
        ]);
      }

      // Update DOM
      dom.replace(this._pathInfo, pathFragment);
    }

    /**
     * Updates the icon and type text. The type label is pulled from the
     * title of the icon supplied.
     * @param {SVGSVGElement} icon Icon to display
     */
    _setTypeContent(icon) {
      const typeDescription = icon.querySelector('title').textContent;
      icon.setAttribute('fill', '#fff');

      this._typeInfo.textContent = typeDescription;
      this._iconInfo.removeChild(this._iconInfo.lastElementChild);
      this._iconInfo.appendChild(icon);
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
      this._updateSize(node);
      this._updatePaths(node);
      if (type !== this._lastType) {
        // No need to create a new icon if it is identical.
        const icon = getIconTemplate(type);
        this._setTypeContent(icon);
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

  class SymbolInfocard extends Infocard {
    /**
     * @param {SVGSVGElement} icon Icon to display
     */
    _setTypeContent(icon) {
      const color = icon.getAttribute('fill');
      super._setTypeContent(icon);
      this._iconInfo.style.backgroundColor = color;
    }
  }

  class ContainerInfocard extends Infocard {
    constructor(id) {
      super(id);
      this._tableBody = this._infocard.querySelector('tbody');
      this._ctx = this._infocard.querySelector('canvas').getContext('2d');

      /**
       * @type {{[type:string]: HTMLTableRowElement}} Rows in the container
       * infocard that represent a particular symbol type.
       */
      this._infoRows = {
        b: this._tableBody.querySelector('.bss-info'),
        d: this._tableBody.querySelector('.data-info'),
        r: this._tableBody.querySelector('.rodata-info'),
        t: this._tableBody.querySelector('.text-info'),
        R: this._tableBody.querySelector('.relro-info'),
        x: this._tableBody.querySelector('.dexnon-info'),
        m: this._tableBody.querySelector('.dex-info'),
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
     * @param {SVGSVGElement} icon Icon to display
     */
    _setTypeContent(icon) {
      super._setTypeContent(icon);
      icon.classList.add('canvas-overlay');
    }

    _flagsString(containerNode) {
      const flags = super._flagsString(containerNode);
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
     * @param {{size:number,count:number} | null} stats Total size of the
     * symbols of a given type in a container.
     * @param {number} percentage How much the size represents in relation to
     * the total size of the symbols in the container.
     */
    _updateBreakdownRow(row, stats, percentage) {
      if (stats == null || stats.size === 0) {
        if (row.parentElement != null) {
          this._tableBody.removeChild(row);
        }
        return;
      }

      const countColumn = row.querySelector('.count');
      const sizeColumn = row.querySelector('.size');
      const percentColumn = row.querySelector('.percent');

      const countString = stats.count.toLocaleString(_LOCALE, {
        useGrouping: true,
      });
      const sizeString = stats.size.toLocaleString(_LOCALE, {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
        useGrouping: true,
      });
      const percentString = percentage.toLocaleString(_LOCALE, {
        style: 'percent',
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
      });

      // Update DOM
      countColumn.textContent = countString;
      sizeColumn.textContent = sizeString;
      percentColumn.textContent = percentString;
      this._tableBody.appendChild(row);
    }

    /**
     * Update DOM for the container infocard
     * @param {TreeNode} containerNode
     */
    _updateInfocard(containerNode) {
      const extraRows = Object.assign({}, this._infoRows);
      const statsEntries = Object.entries(containerNode.childStats).sort(
        (a, b) => b[1].size - a[1].size
      );
      const diffMode = state.has('diff_mode');
      let totalSize = 0;
      for (const [, stats] of statsEntries) {
        totalSize += Math.abs(stats.size);
      }

      // Update DOM
      super._updateInfocard(containerNode);
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

  const _containerInfo = new ContainerInfocard('infocard-container');
  const _symbolInfo = new SymbolInfocard('infocard-symbol');

  /**
   * Displays an infocard for the given symbol on the next frame.
   * @param {TreeNode} node
   */
  function displayInfocard(node) {
    if (_CONTAINER_TYPE_SET.has(node.type[0])) {
      _containerInfo.updateInfocard(node);
      _containerInfo.setHidden(false);
      _symbolInfo.setHidden(true);
    } else {
      _symbolInfo.updateInfocard(node);
      _symbolInfo.setHidden(false);
      _containerInfo.setHidden(true);
    }
  }

  return displayInfocard;
})();
