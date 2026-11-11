use std::convert::Infallible;

#[cfg(feature = "parsed-types")]
use crate::{
    BareItem, Date, Dictionary, InnerList, Item, List, ListEntry, Parameters, Version, token_ref,
};
use crate::{
    BareItemFromInput, Error, KeyRef, Parser, error, key_ref,
    visitor::{
        DictionaryVisitor, EntryVisitor, Ignored, InnerListVisitor, ItemVisitor, ListVisitor,
        ParameterVisitor,
    },
};

#[cfg(feature = "parsed-types")]
macro_rules! item {
    ($val:expr) => {
        Item::from($val)
    };
    ($val:expr; {$($k:expr => $v:expr),* $(,)?}) => {
        Item::with_params($val, params!({$($k => $v),*}))
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! params {
    ({$($k:expr => $v:expr),* $(,)?}) => {
        Parameters::from([
            $( (key_ref($k).to_owned(), BareItem::from($v)) ),*
        ])
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! inner_list {
    ($($item:expr),* $(,)?) => {
        InnerList::new(vec![$( Item::from($item) ),*])
    };
    ([$($item:expr),* $(,)?]; {$($k:expr => $v:expr),* $(,)?}) => {
        InnerList::with_params(
            vec![$( Item::from($item) ),*],
            params!({$($k => $v),*})
        )
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! list {
    ($($v:expr),* $(,)?) => {
        vec![$( ListEntry::from($v) ),*]
    };
}

#[cfg(feature = "parsed-types")]
macro_rules! dict {
    ($($k:expr => $v:expr),* $(,)?) => {
        Dictionary::from([
            $( (key_ref($k).to_owned(), ListEntry::from($v)) ),*
        ])
    };
}

fn check_expected_errs<'de, const N: usize, T, E>(
    parse: impl Fn(Parser<'de>, Ignored) -> Result<T, E>,
    tests: [(&'de str, error::Repr); N],
) where
    T: PartialEq + std::fmt::Debug,
    E: Into<Error> + From<error::Repr> + std::fmt::Debug,
{
    for (input, expected_err) in tests {
        assert_eq!(
            Err(Error::from(expected_err)),
            parse(Parser::new(input), Ignored).map_err(Into::into),
            "{input}",
        );
    }
}

fn check_expected_errs_mut<'de, const N: usize, T>(
    parse: impl Fn(&mut Parser<'de>) -> Result<T, error::Repr>,
    tests: [(&'de str, error::Repr); N],
) where
    T: PartialEq + std::fmt::Debug,
{
    check_expected_errs(|ref mut parser, _| parse(parser), tests);
}

#[test]
fn parse_item_errors() {
    check_expected_errs(
        Parser::parse_item_with_visitor,
        [
            (r#""some_value¢""#, error::Repr::InvalidStringCharacter(11)),
            (
                r#""some_value" trailing_text""#,
                error::Repr::TrailingCharactersAfterParsedValue(13),
            ),
            ("", error::Repr::ExpectedStartOfBareItem(0)),
        ],
    );
}

#[test]
fn parse_list_errors() {
    check_expected_errs(
        Parser::parse_list_with_visitor,
        [
            (",", error::Repr::ExpectedStartOfBareItem(0)),
            ("a, b c", error::Repr::TrailingCharactersAfterMember(5)),
            ("a,", error::Repr::TrailingComma(1)),
            ("a     ,    ", error::Repr::TrailingComma(6)),
            ("a\t \t ,\t ", error::Repr::TrailingComma(5)),
            ("a\t\t,\t\t\t", error::Repr::TrailingComma(3)),
            ("(a b),", error::Repr::TrailingComma(5)),
            ("(1, 2, (a b)", error::Repr::ExpectedInnerListDelimiter(2)),
        ],
    );
}

#[test]
fn parse_inner_list_errors() {
    check_expected_errs_mut(
        |parser| parser.parse_inner_list(Ignored),
        [
            ("c b); a=1", error::Repr::ExpectedStartOfInnerList(0)),
            ("(", error::Repr::UnterminatedInnerList(1)),
        ],
    );
}

#[test]
fn parse_dict_errors() {
    check_expected_errs(
        Parser::parse_dictionary_with_visitor,
        [
            (
                "abc=123;a=1;b=2 def",
                error::Repr::TrailingCharactersAfterMember(16),
            ),
            ("abc=123;a=1,", error::Repr::TrailingComma(11)),
        ],
    );
}

#[test]
fn parse_bare_item_errors() {
    check_expected_errs_mut(
        Parser::parse_bare_item,
        [
            ("!?0", error::Repr::ExpectedStartOfBareItem(0)),
            ("_11abc", error::Repr::ExpectedStartOfBareItem(0)),
            ("   ", error::Repr::ExpectedStartOfBareItem(0)),
        ],
    );
}

#[test]
fn parse_bool_errors() {
    check_expected_errs_mut(
        Parser::parse_bool,
        [
            ("", error::Repr::ExpectedStartOfBoolean(0)),
            ("?", error::Repr::ExpectedBoolean(1)),
        ],
    );
}

#[test]
fn parse_string_errors() {
    check_expected_errs_mut(
        Parser::parse_string,
        [
            ("test", error::Repr::ExpectedStartOfString(0)),
            (r#""\"#, error::Repr::UnterminatedEscapeSequence(2)),
            (r#""\l""#, error::Repr::InvalidEscapeSequence(2)),
            ("\"\u{1f}\"", error::Repr::InvalidStringCharacter(1)),
            (r#""smth"#, error::Repr::UnterminatedString(5)),
        ],
    );
}

#[test]
fn parse_token_errors() {
    check_expected_errs_mut(
        Parser::parse_token,
        [
            ("765token", error::Repr::ExpectedStartOfToken(0)),
            ("7token", error::Repr::ExpectedStartOfToken(0)),
            ("", error::Repr::ExpectedStartOfToken(0)),
        ],
    );
}

#[test]
fn parse_byte_sequence_errors() {
    check_expected_errs_mut(
        Parser::parse_byte_sequence,
        [
            ("aGVsbG8", error::Repr::ExpectedStartOfByteSequence(0)),
            (":aGVsb G8=:", error::Repr::InvalidByteSequence(6)),
            (":aGVsbG8=", error::Repr::UnterminatedByteSequence(9)),
        ],
    );
}

#[test]
fn parse_number_errors() {
    check_expected_errs_mut(
        Parser::parse_number,
        [
            (":aGVsbG8:rest", error::Repr::ExpectedDigit(0)),
            (
                "-11.5555 test string",
                error::Repr::TooManyDigitsAfterDecimalPoint(7),
            ),
            ("--0", error::Repr::ExpectedDigit(1)),
            (
                "1999999999999.1",
                error::Repr::TooManyDigitsBeforeDecimalPoint(13),
            ),
            ("19888899999.", error::Repr::TrailingDecimalPoint(11)),
            ("1999999999999999", error::Repr::TooManyDigits(15)),
            (
                "19999999999.99991",
                error::Repr::TooManyDigitsAfterDecimalPoint(15),
            ),
            ("- 42", error::Repr::ExpectedDigit(1)),
            ("1..4", error::Repr::TrailingDecimalPoint(1)),
            ("-", error::Repr::ExpectedDigit(1)),
            ("-5. 14", error::Repr::TrailingDecimalPoint(2)),
            ("7. 1", error::Repr::TrailingDecimalPoint(1)),
            (
                "-7.3333333333",
                error::Repr::TooManyDigitsAfterDecimalPoint(6),
            ),
            (
                "-7333333333323.12",
                error::Repr::TooManyDigitsBeforeDecimalPoint(14),
            ),
        ],
    );
}

#[test]
fn parse_key_errors() {
    assert_eq!(
        Err(error::Repr::ExpectedStartOfKey(0)),
        Parser::new("[*f=10").parse_key()
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_list() -> Result<(), Error> {
    let expected_list = list![inner_list![1, 2], 42];

    let mut parsed_header: List = Parser::new("(1 2)").parse()?;
    parsed_header = Parser::new("42").parse_list_with_visitor(parsed_header)?;
    assert_eq!(expected_list, parsed_header);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_dict() -> Result<(), Error> {
    let expected_dict = dict! {
        "a" => 1,
        "b" => item!(token_ref("a"); {"bar" => true}),
        "c" => 3
    };

    let mut parsed_header: Dictionary = Parser::new("a=1, b;foo=*\t\t").parse()?;
    parsed_header = Parser::new(" c=3, b=a;bar").parse_dictionary_with_visitor(parsed_header)?;
    assert_eq!(expected_dict, parsed_header);
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_more_errors() -> Result<(), Error> {
    let parsed_dict_header: Dictionary = Parser::new("a=1, b;foo=*").parse()?;
    assert!(
        Parser::new(",a")
            .parse_dictionary_with_visitor(parsed_dict_header)
            .is_err()
    );

    let parsed_list_header: List = Parser::new("a, b;foo=*").parse()?;
    assert!(
        Parser::new("(a, 2)")
            .parse_list_with_visitor(parsed_list_header)
            .is_err()
    );
    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_date() -> Result<(), Error> {
    let input = "@0";

    assert!(
        Parser::new(input)
            .with_version(Version::Rfc8941)
            .parse::<Item>()
            .is_err()
    );

    assert_eq!(Parser::new(input).parse::<Item>()?, item!(Date::UNIX_EPOCH));

    Ok(())
}

#[test]
#[cfg(feature = "parsed-types")]
fn parse_display_string() -> Result<(), Error> {
    let input = r#"%"This is intended for display to %c3%bcsers.""#;

    assert!(
        Parser::new(input)
            .with_version(Version::Rfc8941)
            .parse::<Item>()
            .is_err()
    );

    assert_eq!(
        Parser::new(input).parse::<Item>()?,
        item!(BareItem::DisplayString(
            "This is intended for display to üsers.".to_owned()
        ))
    );

    Ok(())
}

#[test]
fn parse_display_string_errors() {
    check_expected_errs(
        Parser::parse_item_with_visitor,
        [
            (" %", error::Repr::ExpectedQuote(2)),
            (r#" %""#, error::Repr::UnterminatedDisplayString(3)),
            (r#" %"%"#, error::Repr::UnterminatedEscapeSequence(4)),
            (r#" %"%a"#, error::Repr::UnterminatedEscapeSequence(5)),
            (r#" %"%A"#, error::Repr::InvalidEscapeSequence(4)),
            (r#" %"%aA"#, error::Repr::InvalidEscapeSequence(5)),
            (r#" %"x%aa""#, error::Repr::InvalidUtf8InDisplayString(4)),
            (
                r#" %"%c3%bc%aa""#,
                error::Repr::InvalidUtf8InDisplayString(9),
            ),
        ],
    );
}

/// A simple struct used for the complex tests.
#[derive(Default, Debug, PartialEq)]
struct Point {
    x: i64,
    y: i64,
}

impl Point {
    fn new(x: i64, y: i64) -> Self {
        Self { x, y }
    }
}

// For when a `Point` is a parameter somewhere.
impl<'de> ParameterVisitor<'de> for &mut Point {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        let Some(v) = value.as_integer() else {
            return Ok(());
        };
        let ptr = match key.as_str() {
            "x" => &mut self.x,
            "y" => &mut self.y,
            _ => return Ok(()),
        };
        *ptr = i64::from(v);
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> DictionaryVisitor<'de> for &mut Point {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
        let coord = match key.as_str() {
            "x" => &mut self.x,
            "y" => &mut self.y,
            _ => return Ok(None),
        };
        Ok(Some(CoordVisitor { coord }))
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

struct CoordVisitor<'a> {
    coord: &'a mut i64,
}

impl<'de> ItemVisitor<'de> for CoordVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        if let Some(v) = bare_item.as_integer() {
            *self.coord = i64::from(v);
        }
        Ok(Ignored)
    }
}

impl<'de> EntryVisitor<'de> for CoordVisitor<'_> {
    type Error = Infallible;

    fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
        Ok(self)
    }

    fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
        Ok(Ignored)
    }
}

#[test]
fn complex_dict_visitor() {
    let mut point = Point::default();
    Parser::new("x=10, y=3")
        .parse_dictionary_with_visitor(&mut point)
        .expect("successful parse");
    assert_eq!(point, Point::new(10, 3));
}

// An item that is an integer, with optional `x` and `y` parameters.
#[derive(Default, Debug, PartialEq)]
struct Holder {
    v: i64,
    point: Point,
}

impl<'de> ItemVisitor<'de> for &mut Holder {
    type Out = Option<()>;
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        Ok(if let Some(v) = bare_item.as_integer() {
            self.v = i64::from(v);
            Some(&mut self.point)
        } else {
            None
        })
    }
}

#[test]
fn complex_item_visitor() {
    let mut holder = Holder::default();
    Parser::new("12;x=7;y=-5")
        .parse_item_with_visitor(&mut holder)
        .expect("successful parse");
    assert_eq!(holder.point, Point::new(7, -5));
}

#[test]
fn complex_list_visitor() {
    #[derive(Default, Debug, PartialEq)]
    struct ListHolder {
        list: Vec<Holder>,
        point: Point,
    }

    impl<'de> ListVisitor<'de> for Vec<ListHolder> {
        type Out = Self;
        type Error = Infallible;

        fn entry(&mut self) -> Result<impl EntryVisitor<'de>, Self::Error> {
            Ok(self.push_mut(ListHolder::default()))
        }

        fn finish(self) -> Result<Self::Out, Self::Error> {
            Ok(self)
        }
    }

    impl<'de> EntryVisitor<'de> for &mut ListHolder {
        type Error = Infallible;

        fn item(self) -> Result<impl ItemVisitor<'de>, Self::Error> {
            Ok(Ignored)
        }

        fn inner_list(self) -> Result<impl InnerListVisitor<'de>, Self::Error> {
            Ok(self)
        }
    }

    impl<'de> InnerListVisitor<'de> for &mut ListHolder {
        type Error = Infallible;

        fn item(&mut self) -> Result<impl ItemVisitor<'de>, Self::Error> {
            Ok(self.list.push_mut(Holder::default()))
        }

        fn finish(self) -> Result<impl ParameterVisitor<'de>, Self::Error> {
            Ok(&mut self.point)
        }
    }

    let list: Vec<ListHolder> = Parser::new("(1;x=4 2;y=5 3);x=1;y=2,(4;x=12;y=33), ()")
        .parse_list()
        .expect("successful parse");

    let expected = vec![
        ListHolder {
            list: vec![
                Holder {
                    v: 1,
                    point: Point::new(4, 0),
                },
                Holder {
                    v: 2,
                    point: Point::new(0, 5),
                },
                Holder {
                    v: 3,
                    point: Point::default(),
                },
            ],
            point: Point::new(1, 2),
        },
        ListHolder {
            list: vec![Holder {
                v: 4,
                point: Point::new(12, 33),
            }],
            point: Point::default(),
        },
        ListHolder {
            list: Vec::new(),
            point: Point::default(),
        },
    ];

    assert_eq!(list, expected);
}

// Regression test for https://github.com/undef1nd/sfv/issues/194.
// This test does not compile without the associated fix.
#[test]
fn parse_dictionary_lifetime() -> Result<(), Error> {
    struct Visitor<'de>(Option<&'de KeyRef>);

    impl<'de> DictionaryVisitor<'de> for Visitor<'de> {
        type Out = Option<&'de KeyRef>;
        type Error = Infallible;

        fn entry(&mut self, key: &'de KeyRef) -> Result<impl EntryVisitor<'de>, Self::Error> {
            self.0 = Some(key);
            Ok(Ignored)
        }

        fn finish(self) -> Result<Self::Out, Self::Error> {
            Ok(self.0)
        }
    }

    assert_eq!(
        Parser::new("a=1").parse_dictionary_with_visitor(Visitor(None))?,
        Some(key_ref("a"))
    );
    Ok(())
}
