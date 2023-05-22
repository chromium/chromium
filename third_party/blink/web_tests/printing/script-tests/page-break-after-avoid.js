description("Test for page-break-after:avoid");

function test()
{
    createBlockWithRatioToPageHeight("page1", 0.5).style.pageBreakBefore = "always";
    // A block 'page2-1' must move to the next page because it has 'page-break-after:avoid'
    // and both 'page2-1' and 'page2-2' cannot be placed in the current page at the same time.
    createBlockWithRatioToPageHeight("page2-1", 0.3).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page2-2", 0.3);

    createBlockWithRatioToPageHeight("page3-1", 0.5).style.pageBreakBefore = "always";
    createBlockWithRatioToPageHeight("page3-2", 0.3).style.pageBreakAfter = "avoid";
    // A page break can occur between lines inside of 'page3-3'.
    createBlockWithNumberOfLines("page3-3", 10).style.breakInside = "auto";

    createBlockWithRatioToPageHeight("page5", 0.5).style.pageBreakBefore = "always";
    // It seems unrealistic, but block 'page6-1' must move to the next page.
    createBlockWithRatioToPageHeight("page6-1", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-2", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-3", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-4", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-5", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-6", 0.1).style.pageBreakAfter = "avoid";
    createBlockWithRatioToPageHeight("page6-7", 0.1);


    pageNumberForElementShouldBe("page1", 1);
    pageNumberForElementShouldBe("page2-1", 2);
    pageNumberForElementShouldBe("page2-2", 2);

    pageNumberForElementShouldBe("page3-1", 3);
    pageNumberForElementShouldBe("page3-2", 3);
    pageNumberForElementShouldBe("page3-3", 3);

    pageNumberForElementShouldBe("page5", 5);
    pageNumberForElementShouldBe("page6-1", 6);
    // Omit tests for intermediate blocks.
    pageNumberForElementShouldBe("page6-7", 6);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
