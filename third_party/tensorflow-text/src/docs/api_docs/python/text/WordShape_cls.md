description: Values for the 'pattern' arg of the wordshape op.

<div itemscope itemtype="http://developers.google.com/ReferenceObject">
<meta itemprop="name" content="text.WordShape" />
<meta itemprop="path" content="Stable" />
<meta itemprop="property" content="BEGINS_WITH_OPEN_QUOTE"/>
<meta itemprop="property" content="BEGINS_WITH_PUNCT_OR_SYMBOL"/>
<meta itemprop="property" content="ENDS_WITH_CLOSE_QUOTE"/>
<meta itemprop="property" content="ENDS_WITH_ELLIPSIS"/>
<meta itemprop="property" content="ENDS_WITH_EMOTICON"/>
<meta itemprop="property" content="ENDS_WITH_MULTIPLE_SENTENCE_TERMINAL"/>
<meta itemprop="property" content="ENDS_WITH_MULTIPLE_TERMINAL_PUNCT"/>
<meta itemprop="property" content="ENDS_WITH_PUNCT_OR_SYMBOL"/>
<meta itemprop="property" content="ENDS_WITH_SENTENCE_TERMINAL"/>
<meta itemprop="property" content="ENDS_WITH_TERMINAL_PUNCT"/>
<meta itemprop="property" content="HAS_CURRENCY_SYMBOL"/>
<meta itemprop="property" content="HAS_EMOJI"/>
<meta itemprop="property" content="HAS_MATH_SYMBOL"/>
<meta itemprop="property" content="HAS_MIXED_CASE"/>
<meta itemprop="property" content="HAS_NON_LETTER"/>
<meta itemprop="property" content="HAS_NO_DIGITS"/>
<meta itemprop="property" content="HAS_NO_PUNCT_OR_SYMBOL"/>
<meta itemprop="property" content="HAS_NO_QUOTES"/>
<meta itemprop="property" content="HAS_ONLY_DIGITS"/>
<meta itemprop="property" content="HAS_PUNCTUATION_DASH"/>
<meta itemprop="property" content="HAS_QUOTE"/>
<meta itemprop="property" content="HAS_SOME_DIGITS"/>
<meta itemprop="property" content="HAS_SOME_PUNCT_OR_SYMBOL"/>
<meta itemprop="property" content="HAS_TITLE_CASE"/>
<meta itemprop="property" content="IS_ACRONYM_WITH_PERIODS"/>
<meta itemprop="property" content="IS_EMOTICON"/>
<meta itemprop="property" content="IS_LOWERCASE"/>
<meta itemprop="property" content="IS_MIXED_CASE_LETTERS"/>
<meta itemprop="property" content="IS_NUMERIC_VALUE"/>
<meta itemprop="property" content="IS_PUNCT_OR_SYMBOL"/>
<meta itemprop="property" content="IS_UPPERCASE"/>
<meta itemprop="property" content="IS_WHITESPACE"/>
</div>

# text.WordShape

<!-- Insert buttons and diff -->

<table class="tfo-notebook-buttons tfo-api nocontent" align="left">

</table>

<a target="_blank" href="https://github.com/tensorflow/text/tree/master/tensorflow_text/python/ops/wordshape_ops.py">View
source</a>

Values for the 'pattern' arg of the wordshape op.

<!-- Placeholder for "Used in" -->

The supported wordshape identifiers are:

*   <a href="../text/WordShape_cls.md#BEGINS_WITH_OPEN_QUOTE"><code>WordShape.BEGINS_WITH_OPEN_QUOTE</code></a>:
    The input begins with an open quote.

    The following strings are considered open quotes:

    ```
          "  QUOTATION MARK
          '  APOSTROPHE
          `  GRAVE ACCENT
         ``  Pair of GRAVE ACCENTs
     \uFF02  FULLWIDTH QUOTATION MARK
     \uFF07  FULLWIDTH APOSTROPHE
     \u00AB  LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
     \u2018  LEFT SINGLE QUOTATION MARK
     \u201A  SINGLE LOW-9 QUOTATION MARK
     \u201B  SINGLE HIGH-REVERSED-9 QUOTATION MARK
     \u201C  LEFT DOUBLE QUOTATION MARK
     \u201E  DOUBLE LOW-9 QUOTATION MARK
     \u201F  DOUBLE HIGH-REVERSED-9 QUOTATION MARK
     \u2039  SINGLE LEFT-POINTING ANGLE QUOTATION MARK
     \u300C  LEFT CORNER BRACKET
     \u300E  LEFT WHITE CORNER BRACKET
     \u301D  REVERSED DOUBLE PRIME QUOTATION MARK
     \u2E42  DOUBLE LOW-REVERSED-9 QUOTATION MARK
     \uFF62  HALFWIDTH LEFT CORNER BRACKET
     \uFE41  PRESENTATION FORM FOR VERTICAL LEFT CORNER BRACKET
     \uFE43  PRESENTATION FORM FOR VERTICAL LEFT WHITE CORNER BRACKET
    ```

    Note: U+B4 (acute accent) not included.

*   <a href="../text/WordShape_cls.md#BEGINS_WITH_PUNCT_OR_SYMBOL"><code>WordShape.BEGINS_WITH_PUNCT_OR_SYMBOL</code></a>:
    The input starts with a punctuation or symbol character.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_CLOSE_QUOTE"><code>WordShape.ENDS_WITH_CLOSE_QUOTE</code></a>:
    The input ends witha closing quote character.

    The following strings are considered close quotes:

    ```
          "  QUOTATION MARK
          '  APOSTROPHE
          `  GRAVE ACCENT
         ''  Pair of APOSTROPHEs
     \uFF02  FULLWIDTH QUOTATION MARK
     \uFF07  FULLWIDTH APOSTROPHE
     \u00BB  RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK
     \u2019  RIGHT SINGLE QUOTATION MARK
     \u201D  RIGHT DOUBLE QUOTATION MARK
     \u203A  SINGLE RIGHT-POINTING ANGLE QUOTATION MARK
     \u300D  RIGHT CORNER BRACKET
     \u300F  RIGHT WHITE CORNER BRACKET
     \u301E  DOUBLE PRIME QUOTATION MARK
     \u301F  LOW DOUBLE PRIME QUOTATION MARK
     \uFE42  PRESENTATION FORM FOR VERTICAL RIGHT CORNER BRACKET
     \uFE44  PRESENTATION FORM FOR VERTICAL RIGHT WHITE CORNER BRACKET
     \uFF63  HALFWIDTH RIGHT CORNER BRACKET
    ```

    Note: U+B4 (ACUTE ACCENT) is not included.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_ELLIPSIS"><code>WordShape.ENDS_WITH_ELLIPSIS</code></a>:
    The input ends with an ellipsis (i.e. with three or more periods or a
    unicode ellipsis character).

*   <a href="../text/WordShape_cls.md#ENDS_WITH_EMOTICON"><code>WordShape.ENDS_WITH_EMOTICON</code></a>:
    The input ends with an emoticon.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_MULTIPLE_SENTENCE_TERMINAL"><code>WordShape.ENDS_WITH_MULTIPLE_SENTENCE_TERMINAL</code></a>:
    The input ends with multiple sentence-terminal characters.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_MULTIPLE_TERMINAL_PUNCT"><code>WordShape.ENDS_WITH_MULTIPLE_TERMINAL_PUNCT</code></a>:
    The input ends with multiple terminal-punctuation characters.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_PUNCT_OR_SYMBOL"><code>WordShape.ENDS_WITH_PUNCT_OR_SYMBOL</code></a>:
    The input ends with a punctuation or symbol character.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_SENTENCE_TERMINAL"><code>WordShape.ENDS_WITH_SENTENCE_TERMINAL</code></a>:
    The input ends with a sentence-terminal character.

*   <a href="../text/WordShape_cls.md#ENDS_WITH_TERMINAL_PUNCT"><code>WordShape.ENDS_WITH_TERMINAL_PUNCT</code></a>:
    The input ends with a terminal-punctuation character.

*   <a href="../text/WordShape_cls.md#HAS_CURRENCY_SYMBOL"><code>WordShape.HAS_CURRENCY_SYMBOL</code></a>:
    The input contains a currency symbol.

*   <a href="../text/WordShape_cls.md#HAS_EMOJI"><code>WordShape.HAS_EMOJI</code></a>:
    The input contains an emoji character.

    See http://www.unicode.org/Public/emoji/1.0//emoji-data.txt. Emojis are in
    unicode ranges `2600-26FF`, `1F300-1F6FF`, and `1F900-1F9FF`.

*   <a href="../text/WordShape_cls.md#HAS_MATH_SYMBOL"><code>WordShape.HAS_MATH_SYMBOL</code></a>:
    The input contains a mathematical symbol.

*   <a href="../text/WordShape_cls.md#HAS_MIXED_CASE"><code>WordShape.HAS_MIXED_CASE</code></a>:
    The input contains both uppercase and lowercase letterforms.

*   <a href="../text/WordShape_cls.md#HAS_NON_LETTER"><code>WordShape.HAS_NON_LETTER</code></a>:
    The input contains a non-letter character.

*   <a href="../text/WordShape_cls.md#HAS_NO_DIGITS"><code>WordShape.HAS_NO_DIGITS</code></a>:
    The input contains no digit characters.

*   <a href="../text/WordShape_cls.md#HAS_NO_PUNCT_OR_SYMBOL"><code>WordShape.HAS_NO_PUNCT_OR_SYMBOL</code></a>:
    The input contains no unicode punctuation or symbol characters.

*   <a href="../text/WordShape_cls.md#HAS_NO_QUOTES"><code>WordShape.HAS_NO_QUOTES</code></a>:
    The input string contains no quote characters.

*   <a href="../text/WordShape_cls.md#HAS_ONLY_DIGITS"><code>WordShape.HAS_ONLY_DIGITS</code></a>:
    The input consists entirely of unicode digit characters.

*   <a href="../text/WordShape_cls.md#HAS_PUNCTUATION_DASH"><code>WordShape.HAS_PUNCTUATION_DASH</code></a>:
    The input contains at least one unicode dash character.

    Note that this uses the Pd (Dash) unicode property. This property will not
    match to soft-hyphens and katakana middle dot characters.

*   <a href="../text/WordShape_cls.md#HAS_QUOTE"><code>WordShape.HAS_QUOTE</code></a>:
    The input starts or ends with a unicode quotation mark.

*   <a href="../text/WordShape_cls.md#HAS_SOME_DIGITS"><code>WordShape.HAS_SOME_DIGITS</code></a>:
    The input contains a mix of digit characters and non-digit characters.

*   <a href="../text/WordShape_cls.md#HAS_SOME_PUNCT_OR_SYMBOL"><code>WordShape.HAS_SOME_PUNCT_OR_SYMBOL</code></a>:
    The input contains a mix of punctuation or symbol characters, and
    non-punctuation non-symbol characters.

*   <a href="../text/WordShape_cls.md#HAS_TITLE_CASE"><code>WordShape.HAS_TITLE_CASE</code></a>:
    The input has title case (i.e. the first character is upper or title case,
    and the remaining characters are lowercase).

*   <a href="../text/WordShape_cls.md#IS_ACRONYM_WITH_PERIODS"><code>WordShape.IS_ACRONYM_WITH_PERIODS</code></a>:
    The input is a period-separated acronym. This matches for strings of the
    form "I.B.M." but not "IBM".

*   <a href="../text/WordShape_cls.md#IS_EMOTICON"><code>WordShape.IS_EMOTICON</code></a>:
    The input is a single emoticon.

*   <a href="../text/WordShape_cls.md#IS_LOWERCASE"><code>WordShape.IS_LOWERCASE</code></a>:
    The input contains only lowercase letterforms.

*   <a href="../text/WordShape_cls.md#IS_MIXED_CASE_LETTERS"><code>WordShape.IS_MIXED_CASE_LETTERS</code></a>:
    The input contains only uppercase and lowercase letterforms.

*   <a href="../text/WordShape_cls.md#IS_NUMERIC_VALUE"><code>WordShape.IS_NUMERIC_VALUE</code></a>:
    The input is parseable as a numeric value. This will match a fairly broad
    set of floating point and integer representations (but not Nan or Inf).

*   <a href="../text/WordShape_cls.md#IS_PUNCT_OR_SYMBOL"><code>WordShape.IS_PUNCT_OR_SYMBOL</code></a>:
    The input contains only punctuation and symbol characters.

*   <a href="../text/WordShape_cls.md#IS_UPPERCASE"><code>WordShape.IS_UPPERCASE</code></a>:
    The input contains only uppercase letterforms.

*   <a href="../text/WordShape_cls.md#IS_WHITESPACE"><code>WordShape.IS_WHITESPACE</code></a>:
    The input consists entirely of whitespace.

<!-- Tabular view -->
 <table class="responsive fixed orange">
<colgroup><col width="214px"><col></colgroup>
<tr><th colspan="2"><h2 class="add-link">Class Variables</h2></th></tr>

<tr>
<td>
BEGINS_WITH_OPEN_QUOTE<a id="BEGINS_WITH_OPEN_QUOTE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
BEGINS_WITH_PUNCT_OR_SYMBOL<a id="BEGINS_WITH_PUNCT_OR_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_CLOSE_QUOTE<a id="ENDS_WITH_CLOSE_QUOTE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_ELLIPSIS<a id="ENDS_WITH_ELLIPSIS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_EMOTICON<a id="ENDS_WITH_EMOTICON"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_MULTIPLE_SENTENCE_TERMINAL<a id="ENDS_WITH_MULTIPLE_SENTENCE_TERMINAL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_MULTIPLE_TERMINAL_PUNCT<a id="ENDS_WITH_MULTIPLE_TERMINAL_PUNCT"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_PUNCT_OR_SYMBOL<a id="ENDS_WITH_PUNCT_OR_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_SENTENCE_TERMINAL<a id="ENDS_WITH_SENTENCE_TERMINAL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
ENDS_WITH_TERMINAL_PUNCT<a id="ENDS_WITH_TERMINAL_PUNCT"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_CURRENCY_SYMBOL<a id="HAS_CURRENCY_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_EMOJI<a id="HAS_EMOJI"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_MATH_SYMBOL<a id="HAS_MATH_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_MIXED_CASE<a id="HAS_MIXED_CASE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_NON_LETTER<a id="HAS_NON_LETTER"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_NO_DIGITS<a id="HAS_NO_DIGITS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_NO_PUNCT_OR_SYMBOL<a id="HAS_NO_PUNCT_OR_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_NO_QUOTES<a id="HAS_NO_QUOTES"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_ONLY_DIGITS<a id="HAS_ONLY_DIGITS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_PUNCTUATION_DASH<a id="HAS_PUNCTUATION_DASH"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_QUOTE<a id="HAS_QUOTE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_SOME_DIGITS<a id="HAS_SOME_DIGITS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_SOME_PUNCT_OR_SYMBOL<a id="HAS_SOME_PUNCT_OR_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
HAS_TITLE_CASE<a id="HAS_TITLE_CASE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_ACRONYM_WITH_PERIODS<a id="IS_ACRONYM_WITH_PERIODS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_EMOTICON<a id="IS_EMOTICON"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_LOWERCASE<a id="IS_LOWERCASE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_MIXED_CASE_LETTERS<a id="IS_MIXED_CASE_LETTERS"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_NUMERIC_VALUE<a id="IS_NUMERIC_VALUE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_PUNCT_OR_SYMBOL<a id="IS_PUNCT_OR_SYMBOL"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_UPPERCASE<a id="IS_UPPERCASE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr><tr>
<td>
IS_WHITESPACE<a id="IS_WHITESPACE"></a>
</td>
<td>
<a href="../text/WordShape_cls.md"><code>text.WordShape</code></a>
</td>
</tr>
</table>
