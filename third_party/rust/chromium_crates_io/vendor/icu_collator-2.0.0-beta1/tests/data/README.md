The checked-in files in this directory are placeholders (smaller versions of the full test data files created for size purposes).
Both the placeholder file (labeled as "short") and the full test data file are available in the upstream ICU and CLDR repositories.

Those upstream test data files are maintained such that they correspond with the results given by the upstream collation data maintained in CLDR, which in turn is used in ICU.

When updating ICU4X to pull in new source data for collation, 
for example when a new version of Unicode data is pulled in that contains new collation data,
updating the collation test data files in ICU4X to the corresponding new version is also necessary for the collation tests to continue pass with the new collation data.

To run the full tests, replace the test data files in this directory with their full counterparts from the upstream repository.
The latest correct versions of the full test files from upstream, as found in these commits :
https://raw.githubusercontent.com/unicode-org/icu/289d9703a03494ef0d9dfec122169b22bd9fc84f/icu4c/source/test/testdata/riwords.txt
https://raw.githubusercontent.com/unicode-org/cldr/3fca77eda21cb22fa3d201f50b975da15ec72b7c/common/uca/CollationTest_CLDR_NON_IGNORABLE.txt
https://raw.githubusercontent.com/unicode-org/cldr/3fca77eda21cb22fa3d201f50b975da15ec72b7c/common/uca/CollationTest_CLDR_SHIFTED.txt


The latest correct versions of the checked-in placeholder files ("short" version files) for the last two come from upstream, as found in these commits:
https://raw.githubusercontent.com/unicode-org/cldr/3fca77eda21cb22fa3d201f50b975da15ec72b7c/common/uca/CollationTest_CLDR_NON_IGNORABLE_SHORT.txt
https://raw.githubusercontent.com/unicode-org/cldr/3fca77eda21cb22fa3d201f50b975da15ec72b7c/common/uca/CollationTest_CLDR_SHIFTED_SHORT.txt
