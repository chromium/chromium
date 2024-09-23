This suite enables HTML parser changes for appearance:base-select without
actually enabling appearance:base-select. We might ship the HTML parser changes
first before appearance:base-select, so we need to test this case.

--enable-features=SelectParserRelaxation
--disable-features=CustomizableSelect
