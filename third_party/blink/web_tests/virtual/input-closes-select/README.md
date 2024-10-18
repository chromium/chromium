This suite enables InputClosesSelect flag, which re-adds legacy behavior to the
HTML parser which turns `<select><input>` into `<select></select><input>` to
de-risk the launch of SelectParserRelaxation.

--enable-features=SelectParserRelaxation,InputClosesSelect
