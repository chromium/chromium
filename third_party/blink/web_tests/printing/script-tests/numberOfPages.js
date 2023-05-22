description("Test for internals.numberOfPages()");

function test()
{
    createBlockWithRatioToPageHeight("firstPage", 0.6);
    createBlockWithRatioToPageHeight("secondPage", 0.6);

    numberOfPagesShouldBe(2);
    numberOfPagesShouldBe(2, 1000, 10000);

    document.body.removeChild(document.getElementById("sandbox"));
}

var successfullyParsed = true;
