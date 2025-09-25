description("This test checks the SVGRect API");

var svgElement = document.createElementNS("http://www.w3.org/2000/svg", "svg");
var rect = svgElement.createSVGRect();

debug("");
debug("Check initial rect values");
shouldBe("rect.x", "0");
shouldBe("rect.y", "0");
shouldBe("rect.width", "0");
shouldBe("rect.height", "0");

debug("");
debug("Check assigning rects");
shouldBe("rect.x = 100", "100");
shouldBe("rect.y = 200", "200");
shouldBe("rect.width = 300", "300");
shouldBe("rect.height = 400", "400");
debug("Check that the rect contains the correct values");
shouldBe("rect.x", "100");
shouldBe("rect.y", "200");
shouldBe("rect.width", "300");
shouldBe("rect.height", "400");

debug("");
debug("Check assigning invalid rects");
shouldThrow("rect.x = rect");
shouldBeNull("rect.y = null");
shouldThrow("rect.width = 'aString'");
shouldThrow("rect.height = svgElement");

debug("");
debug("Check that the rect contains the correct values");
shouldBe("rect.x", "100");
shouldBe("rect.y", "0");
shouldBe("rect.width", "300");
shouldBe("rect.height", "400");

successfullyParsed = true;
