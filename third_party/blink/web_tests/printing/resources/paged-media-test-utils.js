// The buffer to store the results.  We output the results after all
// tests finish.   Note that we cannot have a DOM element where the
// results are stored in HTMLs because the DOM element to store
// results may change the number of pages.
var _results = '';
var _errored = false;

function appendResult(result)
{
    _results += '<br>' + result;
}

function pageNumberForElementShouldBe(id, expectedPageNumber)
{
    var actualPageNumber = internals.pageNumber(document.getElementById(id));
    if (actualPageNumber == expectedPageNumber)
        appendResult('PASS: page number of "' + id + '" is ' + actualPageNumber);
    else {
        appendResult('FAIL: expected page number of "' + id + '" is ' + expectedPageNumber + '. Was ' + actualPageNumber);
        _errored = true;
    }
}

function numberOfPagesShouldBe(expectedNumberOfPages, pageWidthInPixels, pageHeightInPixels)
{
    // pageWidthInPixels and pageHeightInPixels can be omitted. If omitted, 800x600 is used.
    var actualNumberOfPages;
    if (pageWidthInPixels && pageHeightInPixels)
        actualNumberOfPages = internals.numberOfPages(pageWidthInPixels, pageHeightInPixels);
    else
        actualNumberOfPages = internals.numberOfPages();

    if (actualNumberOfPages == expectedNumberOfPages)
        appendResult('PASS: number of pages is ' + actualNumberOfPages);
    else {
        appendResult('FAIL: expected number of pages is ' + expectedNumberOfPages + '. Was ' + actualNumberOfPages);
        _errored = true;
    }
}

function runPrintingTest(testFunction)
{
    if (window.testRunner) {
        try {
            testFunction();
        } catch (err) {
            _results += '<p>Exception: ' + err.toString();
            _errored = true;
        }

        if (!_errored)
            _results += '<br>All tests passed';
    } else {
        _results += 'This test requires testRunner. You can test this manually with the above description.';
    }

    var resultElement = document.createElement('p');
    resultElement.innerHTML = _results;
    var output = document.getElementById("console") || document.body;
    output.appendChild(resultElement);
}

function createBlockWithRatioToPageHeight(id, heightInRatioToPageHeight)
{
    var element = document.createElement("div");
    element.id = id;
    element.style.height = (heightInRatioToPageHeight * 100) + "vh";
    document.getElementById("sandbox").appendChild(element);
    return element;
}

function createBlockWithNumberOfLines(id, childLines)
{
    var element = document.createElement("div");
    element.id = id;
    for (var i = 0; i < childLines; ++i) {
        element.appendChild(document.createTextNode("line" + i));
        element.appendChild(document.createElement("br"));
    }
    // Make sure that one page has about 20 lines.
    element.style.lineHeight = "5vh";
    document.getElementById("sandbox").appendChild(element);
    return element;
}
