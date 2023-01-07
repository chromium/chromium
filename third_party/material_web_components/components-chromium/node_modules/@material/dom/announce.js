/**
 * @license
 * Copyright 2020 Google Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
/**
 * Priorities for the announce function.
 */
export var AnnouncerPriority;
(function (AnnouncerPriority) {
    AnnouncerPriority["POLITE"] = "polite";
    AnnouncerPriority["ASSERTIVE"] = "assertive";
})(AnnouncerPriority || (AnnouncerPriority = {}));
/**
 * Data attribute added to live region element.
 */
export var DATA_MDC_DOM_ANNOUNCE = 'data-mdc-dom-announce';
/**
 * Announces the given message with optional priority, defaulting to "polite"
 */
export function announce(message, options) {
    Announcer.getInstance().say(message, options);
}
var Announcer = /** @class */ (function () {
    // Constructor made private to ensure only the singleton is used
    function Announcer() {
        this.liveRegions = new Map();
    }
    Announcer.getInstance = function () {
        if (!Announcer.instance) {
            Announcer.instance = new Announcer();
        }
        return Announcer.instance;
    };
    Announcer.prototype.say = function (message, options) {
        var _a, _b;
        var priority = (_a = options === null || options === void 0 ? void 0 : options.priority) !== null && _a !== void 0 ? _a : AnnouncerPriority.POLITE;
        var ownerDocument = (_b = options === null || options === void 0 ? void 0 : options.ownerDocument) !== null && _b !== void 0 ? _b : document;
        var liveRegion = this.getLiveRegion(priority, ownerDocument);
        // Reset the region to pick up the message, even if the message is the
        // exact same as before.
        liveRegion.textContent = '';
        // Timeout is necessary for screen readers like NVDA and VoiceOver.
        setTimeout(function () {
            liveRegion.textContent = message;
            ownerDocument.addEventListener('click', clearLiveRegion);
        }, 1);
        function clearLiveRegion() {
            liveRegion.textContent = '';
            ownerDocument.removeEventListener('click', clearLiveRegion);
        }
    };
    Announcer.prototype.getLiveRegion = function (priority, ownerDocument) {
        var documentLiveRegions = this.liveRegions.get(ownerDocument);
        if (!documentLiveRegions) {
            documentLiveRegions = new Map();
            this.liveRegions.set(ownerDocument, documentLiveRegions);
        }
        var existingLiveRegion = documentLiveRegions.get(priority);
        if (existingLiveRegion &&
            ownerDocument.body.contains(existingLiveRegion)) {
            return existingLiveRegion;
        }
        var liveRegion = this.createLiveRegion(priority, ownerDocument);
        documentLiveRegions.set(priority, liveRegion);
        return liveRegion;
    };
    Announcer.prototype.createLiveRegion = function (priority, ownerDocument) {
        var el = ownerDocument.createElement('div');
        el.style.position = 'absolute';
        el.style.top = '-9999px';
        el.style.left = '-9999px';
        el.style.height = '1px';
        el.style.overflow = 'hidden';
        el.setAttribute('aria-atomic', 'true');
        el.setAttribute('aria-live', priority);
        el.setAttribute(DATA_MDC_DOM_ANNOUNCE, 'true');
        ownerDocument.body.appendChild(el);
        return el;
    };
    return Announcer;
}());
//# sourceMappingURL=announce.js.map