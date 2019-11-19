// The smallest unit of layout measurement in Chrome is 1/64th of a pixel
// (one LayoutUnit), which is slightly less than our tolerance of 0.025.  This
// ensures that rounding errors of one LayoutUnit or less will not cause
// test failures.
var TOLERANCE = 0.025;

var FONT_SIZE_START = 10;
var FONT_SIZE_BASELINE = 12;
var FONT_SIZE_STEP = 0.25;
var FONT_SIZE_END = 25;

var PASS = 0;
var FAIL = 1;

function numberToNode(n)
{
    return document.createTextNode(n.toFixed(2));
}

function createElement(type, opt_textContent, opt_className)
{
    var el = document.createElement(type);
    if (opt_className)
        el.className = opt_className;
    if (opt_textContent)
        el.appendChild(document.createTextNode(opt_textContent));
    return el;
}

function runTest(containerEl, pangram, opt_writingMode)
{
    var cont = document.getElementById('test');

    var el = createElement('div', undefined, 'header');
    el.appendChild(createElement('div', 'Font Size'));
    el.appendChild(createElement('div', 'Width'));
    el.appendChild(createElement('div', 'Normalized'));
    el.appendChild(createElement('div', 'Diff'));
    el.appendChild(createElement('span', 'Content'));
    containerEl.appendChild(el);

    var referenceElement;
    for (var fontSize = FONT_SIZE_START;
            fontSize < FONT_SIZE_END;
            fontSize += FONT_SIZE_STEP) {
        var el = createElement('div');
        el.appendChild(createElement('div'));
        el.appendChild(createElement('div'));
        el.appendChild(createElement('div'));
        el.appendChild(createElement('div', undefined, 'results'));
        var textSpan = createElement('span');
        el.appendChild(textSpan);
        textSpan.appendChild(document.createTextNode(pangram));
        textSpan.style.fontSize = fontSize;
        containerEl.appendChild(el);
        if (fontSize == FONT_SIZE_BASELINE)
            referenceElement = el;
    }

    referenceElement.className = 'reference';
    var rect = referenceElement.lastChild.getBoundingClientRect();
    var expectedWidth = opt_writingMode == 'vertical' ? rect.height : rect.width;

    var failures = 0;
    for (var row, i = 0; row = containerEl.children[i + 1]; i++) {
        var rect = row.lastChild.getBoundingClientRect();
        var fontSize = FONT_SIZE_START + (FONT_SIZE_STEP * i);
        var width = opt_writingMode == 'vertical' ? rect.height : rect.width;
        var normalizedWidth = (width / fontSize) * FONT_SIZE_BASELINE;
        row.children[0].appendChild(numberToNode(fontSize));
        row.children[1].appendChild(numberToNode(width));
        row.children[2].appendChild(numberToNode(normalizedWidth));
        row.children[3].appendChild(numberToNode(normalizedWidth - expectedWidth));
        if (Math.abs(expectedWidth - normalizedWidth) <= TOLERANCE) {
            row.classList.add('size-pass');
        } else {
            row.classList.add('size-fail');
            failures++
        }
    }

    return failures ? FAIL : PASS;
}
