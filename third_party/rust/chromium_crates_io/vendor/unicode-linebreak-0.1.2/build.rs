//! Parses the rules into a state machine using a pair table. Each value in the table specifies the
//! next state and whether it's an forced/allowed break. To handles rules such as
//!
//! B SP* ÷ A
//!
//! the extra state BSP is employed in the pair table friendly equivalent rules
//!
//! (B | BSP) ÷ A, Treat (B | BSP) SP as if it were BSP, Treat BSP as if it were SP
#![recursion_limit = "512"]

use regex::Regex;
use std::env;
use std::error::Error;
use std::fs::File;
use std::io::{BufRead, BufReader, BufWriter, Write};
use std::iter;
use std::path::Path;
use std::str::FromStr;

include!("src/shared.rs");

impl FromStr for BreakClass {
    type Err = &'static str;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Ok(match s {
            "BK" => BK,
            "CR" => CR,
            "LF" => LF,
            "CM" => CM,
            "NL" => NL,
            "SG" => SG,
            "WJ" => WJ,
            "ZW" => ZW,
            "GL" => GL,
            "SP" => SP,
            "ZWJ" => ZWJ,
            "B2" => B2,
            "BA" => BA,
            "BB" => BB,
            "HY" => HY,
            "CB" => CB,
            "CL" => CL,
            "CP" => CP,
            "EX" => EX,
            "IN" => IN,
            "NS" => NS,
            "OP" => OP,
            "QU" => QU,
            "IS" => IS,
            "NU" => NU,
            "PO" => PO,
            "PR" => PR,
            "SY" => SY,
            "AI" => AI,
            "AL" => AL,
            "CJ" => CJ,
            "EB" => EB,
            "EM" => EM,
            "H2" => H2,
            "H3" => H3,
            "HL" => HL,
            "ID" => ID,
            "JL" => JL,
            "JV" => JV,
            "JT" => JT,
            "RI" => RI,
            "SA" => SA,
            "XX" => XX,
            _ => return Err("Invalid break class"),
        })
    }
}

const NUM_CLASSES: usize = 43;
static BREAK_CLASS_TABLE: [&str; NUM_CLASSES] = [
    "BK", "CR", "LF", "CM", "NL", "SG", "WJ", "ZW", "GL", "SP", "ZWJ", "B2", "BA", "BB", "HY",
    "CB", "CL", "CP", "EX", "IN", "NS", "OP", "QU", "IS", "NU", "PO", "PR", "SY", "AI", "AL", "CJ",
    "EB", "EM", "H2", "H3", "HL", "ID", "JL", "JV", "JT", "RI", "SA", "XX",
];

fn default_value(codepoint: u32) -> BreakClass {
    match codepoint {
        // The unassigned code points in the following blocks default to "ID"
        0x3400..=0x4DBF | 0x4E00..=0x9FFF | 0xF900..=0xFAFF => ID,
        // All undesignated code points in Planes 2 and 3, whether inside or outside of allocated blocks, default to "ID"
        0x20000..=0x2FFFD | 0x30000..=0x3FFFD => ID,
        // All unassigned code points in the following Plane 1 range, whether inside or outside of allocated blocks, also default to "ID"
        0x1F000..=0x1FAFF | 0x1FC00..=0x1FFFD => ID,
        // The unassigned code points in the following block default to "PR"
        0x20A0..=0x20CF => PR,
        // All code points, assigned and unassigned, that are not listed explicitly are given the value "XX"
        _ => XX,
    }
}

#[derive(Copy, Clone)]
#[repr(u8)]
enum ExtraState {
    ZWSP = sot + 1,
    OPSP,
    QUSP,
    CLSP,
    CPSP,
    B2SP,
    HLHYBA,
    RIRI,
}

use ExtraState::*;

/// The number of classes plus the eot state.
const NUM_CLASSES_EOT: usize = NUM_CLASSES + 1;
const NUM_STATES: usize = NUM_CLASSES + 10;

/// Separate implementation to prevent infinite recursion.
#[doc(hidden)]
macro_rules! rules2table_impl {
    // Operators
    (($len:ident $($args:tt)*) '÷' $($tt:tt)+) => {rules2table_impl! {(NUM_CLASSES_EOT $($args)* '÷') $($tt)+}};
    (($len:ident $($args:tt)*) '×' $($tt:tt)+) => {rules2table_impl! {(NUM_CLASSES_EOT $($args)* '×') $($tt)+}};
    (($len:ident $($args:tt)*) '!' $($tt:tt)+) => {rules2table_impl! {(NUM_CLASSES_EOT $($args)* '!') $($tt)+}};

    // Perform operator
    (($len:ident $pair_table:ident $($first:ident)? $operator:literal $($second:ident)?) $(, $($tt:tt)*)?) => {
        $(rules2table_impl! {(NUM_STATES $pair_table) $($tt)*})?

        #[allow(unused)] let first = 0..NUM_STATES; // Default to ALL
        $(let first = $first;)?
        #[allow(unused)] let second = 0..NUM_CLASSES_EOT; // Default to ALL
        $(let second = $second;)?
        for i in first {
            for j in second.clone() {
                let cell = &mut $pair_table[i][j];
                match $operator {
                    '!' => *cell |= ALLOWED_BREAK_BIT | MANDATORY_BREAK_BIT,
                    '÷' => *cell |= ALLOWED_BREAK_BIT,
                    '×' => *cell &= !(ALLOWED_BREAK_BIT | MANDATORY_BREAK_BIT),
                    _ => unreachable!("Bad operator"),
                }
            }
        }
    };

    (($len:ident $($args:tt)*) Treat X $($tt:tt)*) => {
        rules2table_impl! {(NUM_CLASSES_EOT $($args)* treat_x) $($tt)*}
    };
    (($len:ident $($args:tt)*) Treat $($tt:tt)*) => {
        rules2table_impl! {(NUM_STATES $($args)* treat) $($tt)*}
    };
    (($len:ident $($args:tt)*) * as if it were X where X = $($tt:tt)*) => {
        rules2table_impl! {(NUM_STATES $($args)* as_if_it_were_x_where_x_is) $($tt)*}
    };

    (($len:ident $pair_table:ident treat_x $second:ident as_if_it_were_x_where_x_is $X:ident) $(, $($tt:tt)*)?) => {
        $(rules2table_impl! {(NUM_STATES $pair_table) $($tt)*})?

        for i in $X {
            for j in $second.clone() {
                $pair_table[i][j] = i as u8;
            }
        }
    };
    (($len:ident $pair_table:ident treat $first:ident $second:ident) as if it were $cls:ident $(, $($tt:tt)*)?) => {
        $(rules2table_impl! {(NUM_STATES $pair_table) $($tt)*})?

        let cls = $cls as u8;
        for i in $first {
            for j in $second.clone() {
                $pair_table[i][j] = cls;
            }
        }
    };
    (($len:ident $pair_table:ident treat $first:ident) as if it were $cls:ident $(, $($tt:tt)*)?) => {
        $(rules2table_impl! {(NUM_STATES $pair_table) $($tt)*})?

        for j in $first.clone().filter(|&j| j < NUM_CLASSES_EOT) {
            for row in $pair_table.iter_mut() {
                row[j] = row[$cls as usize];
            }
        }
        for i in $first {
            $pair_table.copy_within($cls as usize..$cls as usize + 1, i);
        }
    };

    // All classes pattern
    (($len:ident $($args:tt)*) ALL $($tt:tt)*) => {
        let indices = 0..$len;
        rules2table_impl! {(NUM_CLASSES_EOT $($args)* indices) $($tt)*}
    };
    // Single class pattern
    (($len:ident $($args:tt)*) $cls:ident $($tt:tt)*) => {
        let indices = iter::once($cls as usize);
        rules2table_impl! {(NUM_CLASSES_EOT $($args)* indices) $($tt)*}
    };
    // Parse (X | ...) patterns
    (($len:ident $($args:tt)*) ($($cls:ident)|+) $($tt:tt)*) => {
        let indices = [$($cls as usize),+].iter().cloned();
        rules2table_impl! {(NUM_CLASSES_EOT $($args)* indices) $($tt)*}
    };
    // Parse [^ ...] patterns
    (($len:ident $($args:tt)*) [^$($cls:ident)+] $($tt:tt)*) => {
        let excluded = [$($cls as usize),+];
        let indices = (0..$len).filter(|i| !excluded.contains(i)).collect::<Vec<_>>();
        let indices = indices.iter().cloned();
        rules2table_impl! {(NUM_CLASSES_EOT $($args)* indices) $($tt)*}
    };

    (($len:ident $pair_table:ident)) => {}; // Exit condition
}

/// Returns a pair table conforming to the specified rules.
///
/// The rule syntax is a modified subset of the one in Unicode Standard Annex #14.
macro_rules! rules2table {
    ($($tt:tt)+) => {{
        let mut pair_table = [[
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
            24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43,
        ]; NUM_STATES];
        rules2table_impl! {(NUM_STATES pair_table) $($tt)+}
        pair_table
    }};
}

trait IteratorExt: Iterator {
    /// Tests if all elements of the iterator are equal.
    fn all_equal(&mut self) -> bool
    where
        <Self as Iterator>::Item: PartialEq,
        Self: Sized,
    {
        if let Some(first) = self.next() {
            self.all(|x| x == first)
        } else {
            true
        }
    }
}

impl<I: Iterator> IteratorExt for I {}

fn main() -> Result<(), Box<dyn Error>> {
    println!("cargo:rerun-if-changed=LineBreak.txt");
    assert!(NUM_STATES <= 0x3F, "Too many states");

    let pair_table = rules2table! {
        // Non-tailorable Line Breaking Rules
        // LB1 Assign a line breaking class to each code point of the input. Resolve AI, CB, CJ,
        // SA, SG, and XX into other line breaking classes depending on criteria outside the scope
        // of this algorithm.
        Treat (AI | SG | XX | SA) as if it were AL, Treat CJ as if it were NS,
        // Start and end of text:
        sot '×', // LB2 Never break at the start of text.
        '!' eot, // LB3 Always break at the end of text.
        // Mandatory breaks:
        BK '!', // LB4 Always break after hard line breaks.
        // LB5 Treat CR followed by LF, as well as CR, LF, and NL as hard line breaks.
        CR '×' LF, CR '!', LF '!', NL '!',
        '×' (BK | CR | LF | NL), // LB6 Do not break before hard line breaks.
        // Explicit breaks and non-breaks:
        '×' SP, '×' ZW, // LB7 Do not break before spaces or zero width space.
        // LB8 Break before any character following a zero-width space, even if one or more spaces
        // intervene.
        (ZW | ZWSP) '÷', Treat (ZW | ZWSP) SP as if it were ZWSP, Treat ZWSP as if it were SP,
        // ZWJ '×', // XXX Handled explicitly // LB8a Do not break after a zero width joiner.
        // Combining marks:
        // LB9 Do not break a combining character sequence; treat it as if it has the line breaking
        // class of the base character in all of the following rules. Treat ZWJ as if it were CM.
        Treat X (CM | ZWJ)* as if it were X where X = [^BK CR LF NL SP ZW sot eot ZWSP OPSP QUSP CLSP CPSP B2SP],
        Treat (CM | ZWJ) as if it were AL, // LB10 Treat any remaining combining mark or ZWJ as AL.
        // Word joiner:
        '×' WJ, WJ '×', // LB11 Do not break before or after Word joiner and related characters.
        // Non-breaking characters:
        GL '×', // LB12 Do not break after NBSP and related characters.

        // Tailorable Line Breaking Rules
        [^SP BA HY sot eot ZWSP OPSP QUSP CLSP CPSP B2SP] '×' GL, // LB12a Do not break before NBSP and related characters, except after spaces and hyphens.
        // LB13 Do not break before ‘]’ or ‘!’ or ‘;’ or ‘/’, even after spaces.
        '×' CL, '×' CP, '×' EX, '×' IS, '×' SY,
        // LB14 Do not break after ‘[’, even after spaces.
        (OP | OPSP) '×', Treat (OP | OPSP) SP as if it were OPSP, Treat ZWSP as if it were SP,
        // LB15 Do not break within ‘”[’, even with intervening spaces.
        (QU | QUSP) '×' OP, Treat (QU | QUSP) SP as if it were QUSP, Treat QUSP as if it were SP,
        // LB16 Do not break between closing punctuation and a nonstarter (lb=NS), even with
        // intervening spaces.
        (CL | CLSP | CP | CPSP) '×' NS,
        Treat (CL | CLSP) SP as if it were CLSP, Treat CLSP as if it were SP,
        Treat (CP | CPSP) SP as if it were CPSP, Treat CPSP as if it were SP,
        // LB17 Do not break within ‘——’, even with intervening spaces.
        (B2 | B2SP) '×' B2, Treat (B2 | B2SP) SP as if it were B2SP, Treat B2SP as if it were SP,
        // Spaces:
        SP '÷', // LB18 Break after spaces.
        // Special case rules:
        '×' QU, QU '×', // LB19 Do not break before or after quotation marks, such as ‘”’.
        '÷' CB, CB '÷', // LB20 Break before and after unresolved CB.
        // LB21 Do not break before hyphen-minus, other hyphens, fixed-width spaces, small kana,
        // and other non-starters, or after acute accents.
        '×' BA, '×' HY, '×' NS, BB '×',
        // LB21a Don't break after Hebrew + Hyphen. // XXX Use a single state, HLHYBA, for HLHY and HLBA
        HLHYBA '×', Treat HL (HY | BA) as if it were HLHYBA, Treat HLHYBA as if it were HY,
        SY '×' HL, // LB21b Don’t break between Solidus and Hebrew letters.
        '×' IN, // LB22 Do not break before ellipses.
        // Numbers:
        (AL | HL) '×' NU, NU '×' (AL | HL), // LB23 Do not break between digits and letters.
        // LB23a Do not break between numeric prefixes and ideographs, or between ideographs and
        // numeric postfixes.
        PR '×' (ID | EB | EM), (ID | EB | EM) '×' PO,
        // LB24 Do not break between numeric prefix/postfix and letters, or between letters and
        // prefix/postfix.
        (PR | PO) '×' (AL | HL), (AL | HL) '×' (PR | PO),
        // LB25 Do not break between the following pairs of classes relevant to numbers:
        CL '×' PO, CP '×' PO, CL '×' PR, CP '×' PR, NU '×' PO, NU '×' PR, PO '×' OP, PO '×' NU, PR '×' OP, PR '×' NU, HY '×' NU, IS '×' NU, NU '×' NU, SY '×' NU,
        // Korean syllable blocks
        // LB26 Do not break a Korean syllable.
        JL '×' (JL | JV | H2 | H3), (JV | H2) '×' (JV | JT), (JT | H3) '×' JT,
        // LB27 Treat a Korean Syllable Block the same as ID.
        (JL | JV | JT | H2 | H3) '×' IN, (JL | JV | JT | H2 | H3) '×' PO, PR '×' (JL | JV | JT | H2 | H3),
        // Finally, join alphabetic letters into words and break everything else.
        (AL | HL) '×' (AL | HL), // LB28 Do not break between alphabetics (“at”).
        IS '×' (AL | HL), // LB29 Do not break between numeric punctuation and alphabetics (“e.g.”).
        // LB30 Do not break between letters, numbers, or ordinary symbols and opening or closing
        // parentheses.
        (AL | HL | NU) '×' OP, CP '×' (AL | HL | NU),
        // LB30a Break between two regional indicator symbols if and only if there are an even
        // number of regional indicators preceding the position of the break.
        RI '×' RI, Treat RI RI as if it were RIRI, Treat RIRI as if it were RI,
        EB '×' EM, // LB30b Do not break between an emoji base and an emoji modifier.
        '÷' ALL, ALL '÷', // LB31 Break everywhere else.
    };

    // Synthesize all non-"safe" pairs from pair table. There are generally more safe pairs.
    let unsafe_pairs = (0..NUM_CLASSES).into_iter().flat_map(|j| {
        (0..NUM_CLASSES).into_iter().filter_map(move |i| {
            // All states that could have resulted from break class "i"
            let possible_states = pair_table
                .iter()
                .map(|row| (row[i] & !(ALLOWED_BREAK_BIT | MANDATORY_BREAK_BIT)) as usize);
            // Check if all state transitions due to "j" are the same
            if possible_states.map(|s| pair_table[s][j]).all_equal() {
                None
            } else {
                Some((i, j))
            }
        })
    });

    let out_dir = env::var("OUT_DIR")?;
    let dest_path = Path::new(&out_dir).join("tables.rs");
    let mut stream = BufWriter::new(File::create(&dest_path)?);

    stream.write_all(b"static BREAK_PROP_DATA: [[BreakClass; 256]; PAGE_COUNT] = [")?;

    let re = Regex::new(
        r"(?x)^
        (?P<start>[[:xdigit:]]{4,}) # Unicode code point
        (?:\.{2}(?P<end>[[:xdigit:]]{4,}))? # End range
        ;
        (?P<lb>\w{2,3}) # Line_Break property",
    )?;

    let mut values = BufReader::new(File::open("LineBreak.txt")?)
        .lines()
        .map(Result::unwrap)
        .filter(|l| !(l.starts_with('#') || l.is_empty()))
        .scan(0, |next, l| {
            let caps = re.captures(&l).unwrap();
            let start = u32::from_str_radix(&caps["start"], 16).unwrap();
            let end = caps
                .name("end")
                .map(|m| u32::from_str_radix(m.as_str(), 16).unwrap())
                .unwrap_or(start);
            let lb = caps["lb"].parse().unwrap();

            let iter = (*next..=end).map(move |code| {
                if code < start {
                    default_value(code)
                } else {
                    lb
                }
            });
            *next = end + 1;
            Some(iter)
        })
        .flatten();

    let mut page = Vec::with_capacity(256);
    let mut page_count = 0;
    let mut page_indices = Vec::new();
    loop {
        page.clear();
        page.extend(values.by_ref().take(256));

        if let Some(&first) = page.first() {
            page_indices.push(if page.iter().all_equal() {
                first as usize | UNIFORM_PAGE
            } else {
                writeln!(
                    stream,
                    "[{}],",
                    page.iter()
                        .copied()
                        .chain(iter::repeat(XX))
                        .take(256)
                        .map(|v| BREAK_CLASS_TABLE[v as usize])
                        .collect::<Vec<_>>()
                        .join(",")
                )?;
                let page_index = page_count;
                page_count += 1;
                page_index
            });
        } else {
            break;
        }
    }

    writeln!(
        stream,
        r"];

        const PAGE_COUNT: usize = {};
        static PAGE_INDICES: [usize; {}] = [",
        page_count,
        page_indices.len()
    )?;
    for page_idx in page_indices {
        write!(stream, "{},", page_idx)?;
    }
    write!(
        stream,
        r"];

        static PAIR_TABLE: [[u8; {}]; {}] = [",
        NUM_CLASSES_EOT, NUM_STATES
    )?;
    for row in &pair_table {
        write!(stream, "[")?;
        for x in row {
            write!(stream, "{},", x)?;
        }
        write!(stream, "],")?;
    }
    writeln!(
        stream,
        r"];

        fn is_safe_pair(a: BreakClass, b: BreakClass) -> bool {{
            !matches!((a, b), {})
        }}",
        unsafe_pairs
            .map(|(i, j)| format!("({}, {})", BREAK_CLASS_TABLE[i], BREAK_CLASS_TABLE[j]))
            .collect::<Vec<_>>()
            .join("|")
    )?;

    Ok(())
}
