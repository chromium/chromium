from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent

# All font tables with tag, short description and link to some relevant spec.
#
# Commented tables are those we don't currently support or don't intend to
# support.
TABLES = [
#   ("acnt", "Accent attachment table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6acnt.html"),
    ("ankr", "Anchor point table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6ankr.html"),
    ("avar", "Axis variation table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/avar"),
    ("BASE", "Baseline table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/base"),
#   ("bdat", "Bitmap data table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6bdat.html"),
#   ("bhed", "Bitmap font header table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6bhed.html"),
#   ("bloc", "Bitmap location table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6bloc.html"),
#   ("bsln", "Baseline table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6bsln.html"),
    ("CFF ", "Compact font format table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cff"),
    ("CFF2", "Compact font format 2.0 table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cff2"),
    ("CBDT", "Color bitmap data table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cbdt"),
    ("CBLC", "Color bitmap location table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cblc"),
    ("COLR", "Color table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/colr"),
    ("CPAL", "Color palette table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cpal"),
    ("cmap", "Character to glyph mapping table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cmap"),
    ("cvar", "CVT variations table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cvar"),
    ("cvt ", "Control value table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/cvt"),
    ("DSIG", "Digital signature table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/dsig"),
    ("EBDT", "Embedded bitmap data table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/ebdt"),
    ("EBLC", "Embedded bitmap location data table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/eblc"),
#   ("EBSC", "Embedded bitmap scaling data table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/ebsc"),
    ("feat", "Font feature table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6feat.html"),
#   ("fdsc", "Font descriptor table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6fdsc.html"),
#   ("fmtx", "Font metrics table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6fmtx.html"),
#   ("fond", "Font family compatibility table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6fond.html"),
    ("fpgm", "Font program table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/fpgm"),
    ("fvar", "Font variations table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/fvar"),
    ("gasp", "Grid-fitting and scan-conversion procedure table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/gasp"),
    ("GDEF", "Glyph definition table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/gdef"),
#   ("gcid", "Glyph CID table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6gcid.html"),
    ("glyf", "TrueType glyph data table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/glyf"),
    ("GPOS", "Glyph positioning table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/gpos"),
    ("GSUB", "Glyph substitution table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/gsub"),
    ("gvar", "Glyph variation table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/gvar"),
    ("hdmx", "Horizontal device metrics.", "https://learn.microsoft.com/en-us/typography/opentype/spec/hdmx"),
    ("head", "Font header table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/head"),
    ("hhea", "Horizontal header table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/hhea"),
    ("hmtx", "Horizontal metrics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/hmtx"),
    ("HVAR", "Horizontal metrics variation table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/hvar"),
    ("IFT ", "Incremental font transfer table.", "https://www.w3.org/TR/IFT/#font-format-extensions"),
    ("IFTX", "Incremental font transfer table.", "https://www.w3.org/TR/IFT/#font-format-extensions"),
    ("JSTF", "Justification table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/jstf"),
#   ("just", "Justification table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6just.html"),
    ("kern", "Kerning table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/kern"),
    ("kerx", "Extended kerning table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6kerx.html"),
#   ("lcar", "Ligature caret table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6lcar.html"),
    ("loca", "Index to location table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/loca"),
    ("ltag", "Language tag table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6ltag.html"),
    ("MATH", "Mathematical typesetting table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/math"),
    ("maxp", "Maximum profile table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/maxp"),
    ("meta", "Metadata table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/meta"),
#   ("mort", "Metamorphosis table (deprecated).", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6mort.html"),
    ("morx", "Extended metamorphosis table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6morx.html"),
    ("MVAR", "Metrics variation table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/mvar"),
    ("name", "Naming table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/name"),
#   ("opbd", "Optical bounds table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6opbd.html"),
    ("OS/2", "OS/2 and Windows-specific metrics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/os2"),
#   ("PCLT", "HP Printer Control Language table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/pclt"),
    ("post", "PostScript information table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/post"),
    ("prep", "Control value program table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/prep"),
#   ("prop", "Glyph properties table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6prop.html"),
    ("sbix", "Standard bitmap graphics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/sbix"),
    ("STAT", "Style attributes table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/stat"),
    ("SVG ", "Scalable vector graphics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/svg"),
    ("trak", "Tracking table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6trak.html"),
#   ("VDMX", "Vertical Device Metrics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/vdmx"),
    ("VARC", "Variable composite/component table.", "https://github.com/harfbuzz/boring-expansion-spec/blob/main/VARC.md"),
    ("vhea", "Vertical header table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/vhea"),
    ("vmtx", "Vertical metrics table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/vmtx"),
    ("VORG", "Vertical origin table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/vorg"),
    ("VVAR", "Vertical metrics variation table.", "https://learn.microsoft.com/en-us/typography/opentype/spec/vvar"),
#   ("Zapf", "Glyph reference table.", "https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6Zapf.html"),    
]

def tag_to_name(tag: str):
    return tag.lower().replace("/", "").removesuffix(" ")

def generate():
    buf = ""
    buf += "// THIS FILE IS AUTOGENERATED.\n"
    buf += "// Any changes to this file will be overwritten.\n"
    buf += "// Use ../../scripts/gen_tables.py to regenerate.\n\n"

    # Per table data
    buf += "/// Holds some data for each possible table in a font.\n";
    buf += "#[derive(Clone, Default, Debug)]\n"
    buf += "struct PerTableData<T> {\n"
    for table in TABLES:
        buf += "    /// " + table[1] + "\n"
        buf += "    {}: T,\n".format(tag_to_name(table[0]))
    buf += "}\n\n"
    buf += "impl<T> PerTableData<T> {\n"
    buf += "    /// Calls `f` with the tag and associated data for each table.\n"
    buf += "    fn init_all(&mut self, f: impl Fn(Tag, &mut T)) {\n"
    for table in TABLES:
        buf += "        f(Tag::new(b\"{}\"), &mut self.{});\n".format(table[0], tag_to_name(table[0]))
    buf += "    }\n"
    buf += "}\n\n"

    # Table data provider
    buf += "trait TableDataProvider<'a> where Self: 'a {\n"
    buf += "    type Entry;\n\n"
    buf += "    fn tables(&self) -> &'a PerTableData<Self::Entry>;\n"
    buf += "    fn table_state(&self, tag: Tag, entry: &'a Self::Entry) -> Option<TableState<'a>>;\n\n"
    for table in TABLES:
        tag = table[0]
        name = tag_to_name(tag)
        buf += "    fn {}(&self) -> Option<TableState<'a>> {{\n".format(name)
        buf += "        self.table_state(Tag::new(b\"{}\"), &self.tables().{})\n".format(tag, name)
        buf += "    }\n\n"
    buf += "}\n\n"

    # Per table accessors for FontTables
    buf += "impl FontTables {\n"
    for table in TABLES:
        tag = table[0]
        trimmed_tag = tag.removesuffix(" ");
        name = tag_to_name(tag)
        desc = table[1]
        url = table[2]
        buf += "    /// {} data.\n".format(desc[:-1])
        buf += "    ///\n"
        buf += "    /// See the [{}]({}) specification.\n".format(trimmed_tag, url)
        buf += "    pub fn {}_data(&self) -> Option<&'_ [u8]> {{\n".format(name)
        buf += "        self.{}_state().map(|state| state.data)\n".format(name)
        buf += "    }\n\n"
        buf += "    fn {}_state(&self) -> Option<TableState<'_>> {{\n".format(name)
        buf += "        match &self.0 {\n"
        buf += "            TableSource::None => None,\n"
        buf += "            TableSource::Blob(blob) => blob.{}(),\n".format(name)
        buf += "            TableSource::Function(func) => func.{}(),\n".format(name)
        buf += "        }\n"
        buf += "    }\n\n"
    buf += "}\n"
    return buf

if __name__ == "__main__":    
    data = generate()
    Path(SCRIPT_DIR.joinpath("../data/generated/generated_tables.rs")).write_text(data, encoding="utf-8")
