use crate::FromStr;
use crate::{BareItem, Decimal, Dictionary, InnerList, Item, List, Num, Parameters};
use crate::{ParseMore, ParseValue, Parser};
use std::error::Error;
use std::iter::FromIterator;

#[test]
fn parse() -> Result<(), Box<dyn Error>> {
    let input = "\"some_value\"".as_bytes();
    let parsed_item = Item::new(BareItem::String("some_value".to_owned()));
    let expected = parsed_item;
    assert_eq!(expected, Parser::parse_item(input)?);

    let input = "12.35;a ".as_bytes();
    let params = Parameters::from_iter(vec![("a".to_owned(), BareItem::Boolean(true))]);
    let expected = Item::with_params(Decimal::from_str("12.35")?.into(), params);

    assert_eq!(expected, Parser::parse_item(input)?);
    Ok(())
}

#[test]
fn parse_errors() -> Result<(), Box<dyn Error>> {
    let input = "\"some_value¢\"".as_bytes();
    assert_eq!(
        Err("parse: non-ascii characters in input"),
        Parser::parse_item(input)
    );
    let input = "\"some_value\" trailing_text".as_bytes();
    assert_eq!(
        Err("parse: trailing characters after parsed value"),
        Parser::parse_item(input)
    );
    assert_eq!(
        Err("parse_bare_item: empty item"),
        Parser::parse_item("".as_bytes())
    );
    Ok(())
}

#[test]
fn parse_list_of_numbers() -> Result<(), Box<dyn Error>> {
    let mut input = "1,42".chars().peekable();
    let item1 = Item::new(1.into());
    let item2 = Item::new(42.into());
    let expected_list: List = vec![item1.into(), item2.into()];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_with_multiple_spaces() -> Result<(), Box<dyn Error>> {
    let mut input = "1  ,  42".chars().peekable();
    let item1 = Item::new(1.into());
    let item2 = Item::new(42.into());
    let expected_list: List = vec![item1.into(), item2.into()];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_of_lists() -> Result<(), Box<dyn Error>> {
    let mut input = "(1 2), (42 43)".chars().peekable();
    let item1 = Item::new(1.into());
    let item2 = Item::new(2.into());
    let item3 = Item::new(42.into());
    let item4 = Item::new(43.into());
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let inner_list_2 = InnerList::new(vec![item3, item4]);
    let expected_list: List = vec![inner_list_1.into(), inner_list_2.into()];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_empty_inner_list() -> Result<(), Box<dyn Error>> {
    let mut input = "()".chars().peekable();
    let inner_list = InnerList::new(vec![]);
    let expected_list: List = vec![inner_list.into()];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_empty() -> Result<(), Box<dyn Error>> {
    let mut input = "".chars().peekable();
    let expected_list: List = vec![];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_of_lists_with_param_and_spaces() -> Result<(), Box<dyn Error>> {
    let mut input = "(  1  42  ); k=*".chars().peekable();
    let item1 = Item::new(1.into());
    let item2 = Item::new(42.into());
    let inner_list_param =
        Parameters::from_iter(vec![("k".to_owned(), BareItem::Token("*".to_owned()))]);
    let inner_list = InnerList::with_params(vec![item1, item2], inner_list_param);
    let expected_list: List = vec![inner_list.into()];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_of_items_and_lists_with_param() -> Result<(), Box<dyn Error>> {
    let mut input = "12, 14, (a  b); param=\"param_value_1\", ()"
        .chars()
        .peekable();
    let item1 = Item::new(12.into());
    let item2 = Item::new(14.into());
    let item3 = Item::new(BareItem::Token("a".to_owned()));
    let item4 = Item::new(BareItem::Token("b".to_owned()));
    let inner_list_param = Parameters::from_iter(vec![(
        "param".to_owned(),
        BareItem::String("param_value_1".to_owned()),
    )]);
    let inner_list = InnerList::with_params(vec![item3, item4], inner_list_param);
    let empty_inner_list = InnerList::new(vec![]);
    let expected_list: List = vec![
        item1.into(),
        item2.into(),
        inner_list.into(),
        empty_inner_list.into(),
    ];
    assert_eq!(expected_list, List::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_list_errors() -> Result<(), Box<dyn Error>> {
    let mut input = ",".chars().peekable();
    assert_eq!(
        Err("parse_bare_item: item type can't be identified"),
        List::parse(&mut input)
    );

    let mut input = "a, b c".chars().peekable();
    assert_eq!(
        Err("parse_list: trailing characters after list member"),
        List::parse(&mut input)
    );

    let mut input = "a,".chars().peekable();
    assert_eq!(Err("parse_list: trailing comma"), List::parse(&mut input));

    let mut input = "a     ,    ".chars().peekable();
    assert_eq!(Err("parse_list: trailing comma"), List::parse(&mut input));

    let mut input = "a\t \t ,\t ".chars().peekable();
    assert_eq!(Err("parse_list: trailing comma"), List::parse(&mut input));

    let mut input = "a\t\t,\t\t\t".chars().peekable();
    assert_eq!(Err("parse_list: trailing comma"), List::parse(&mut input));

    let mut input = "(a b),".chars().peekable();
    assert_eq!(Err("parse_list: trailing comma"), List::parse(&mut input));

    let mut input = "(1, 2, (a b)".chars().peekable();
    assert_eq!(
        Err("parse_inner_list: bad delimitation"),
        List::parse(&mut input)
    );

    Ok(())
}

#[test]
fn parse_inner_list_errors() -> Result<(), Box<dyn Error>> {
    let mut input = "c b); a=1".chars().peekable();
    assert_eq!(
        Err("parse_inner_list: input does not start with '('"),
        Parser::parse_inner_list(&mut input)
    );
    Ok(())
}

#[test]
fn parse_inner_list_with_param_and_spaces() -> Result<(), Box<dyn Error>> {
    let mut input = "(c b); a=1".chars().peekable();
    let inner_list_param = Parameters::from_iter(vec![("a".to_owned(), 1.into())]);

    let item1 = Item::new(BareItem::Token("c".to_owned()));
    let item2 = Item::new(BareItem::Token("b".to_owned()));
    let expected = InnerList::with_params(vec![item1, item2], inner_list_param);
    assert_eq!(expected, Parser::parse_inner_list(&mut input)?);
    Ok(())
}

#[test]
fn parse_item_int_with_space() -> Result<(), Box<dyn Error>> {
    let mut input = "12 ".chars().peekable();
    assert_eq!(Item::new(12.into()), Item::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_item_decimal_with_bool_param_and_space() -> Result<(), Box<dyn Error>> {
    let mut input = "12.35;a ".chars().peekable();
    let param = Parameters::from_iter(vec![("a".to_owned(), BareItem::Boolean(true))]);
    assert_eq!(
        Item::with_params(Decimal::from_str("12.35")?.into(), param),
        Item::parse(&mut input)?
    );
    Ok(())
}

#[test]
fn parse_item_number_with_param() -> Result<(), Box<dyn Error>> {
    let param = Parameters::from_iter(vec![("a1".to_owned(), BareItem::Token("*".to_owned()))]);
    assert_eq!(
        Item::with_params(BareItem::String("12.35".to_owned()), param),
        Item::parse(&mut "\"12.35\";a1=*".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_item_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_bare_item: empty item"),
        Item::parse(&mut "".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_dict_empty() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Dictionary::new(),
        Dictionary::parse(&mut "".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_dict_errors() -> Result<(), Box<dyn Error>> {
    let mut input = "abc=123;a=1;b=2 def".chars().peekable();
    assert_eq!(
        Err("parse_dict: trailing characters after dictionary member"),
        Dictionary::parse(&mut input)
    );
    let mut input = "abc=123;a=1,".chars().peekable();
    assert_eq!(
        Err("parse_dict: trailing comma"),
        Dictionary::parse(&mut input)
    );
    Ok(())
}

#[test]
fn parse_dict_with_spaces_and_params() -> Result<(), Box<dyn Error>> {
    let mut input = "abc=123;a=1;b=2, def=456, ghi=789;q=9;r=\"+w\""
        .chars()
        .peekable();
    let item1_params =
        Parameters::from_iter(vec![("a".to_owned(), 1.into()), ("b".to_owned(), 2.into())]);
    let item3_params = Parameters::from_iter(vec![
        ("q".to_owned(), 9.into()),
        ("r".to_owned(), BareItem::String("+w".to_owned())),
    ]);

    let item1 = Item::with_params(123.into(), item1_params);
    let item2 = Item::new(456.into());
    let item3 = Item::with_params(789.into(), item3_params);

    let expected_dict = Dictionary::from_iter(vec![
        ("abc".to_owned(), item1.into()),
        ("def".to_owned(), item2.into()),
        ("ghi".to_owned(), item3.into()),
    ]);
    assert_eq!(expected_dict, Dictionary::parse(&mut input)?);

    Ok(())
}

#[test]
fn parse_dict_empty_value() -> Result<(), Box<dyn Error>> {
    let mut input = "a=()".chars().peekable();
    let inner_list = InnerList::new(vec![]);
    let expected_dict = Dictionary::from_iter(vec![("a".to_owned(), inner_list.into())]);
    assert_eq!(expected_dict, Dictionary::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_dict_with_token_param() -> Result<(), Box<dyn Error>> {
    let mut input = "a=1, b;foo=*, c=3".chars().peekable();
    let item2_params =
        Parameters::from_iter(vec![("foo".to_owned(), BareItem::Token("*".to_owned()))]);
    let item1 = Item::new(1.into());
    let item2 = Item::with_params(BareItem::Boolean(true), item2_params);
    let item3 = Item::new(3.into());
    let expected_dict = Dictionary::from_iter(vec![
        ("a".to_owned(), item1.into()),
        ("b".to_owned(), item2.into()),
        ("c".to_owned(), item3.into()),
    ]);
    assert_eq!(expected_dict, Dictionary::parse(&mut input)?);
    Ok(())
}

#[test]
fn parse_dict_multiple_spaces() -> Result<(), Box<dyn Error>> {
    // input1, input2, input3 must be parsed into the same structure
    let item1 = Item::new(1.into());
    let item2 = Item::new(2.into());
    let expected_dict = Dictionary::from_iter(vec![
        ("a".to_owned(), item1.into()),
        ("b".to_owned(), item2.into()),
    ]);

    let mut input1 = "a=1 ,  b=2".chars().peekable();
    let mut input2 = "a=1\t,\tb=2".chars().peekable();
    let mut input3 = "a=1, b=2".chars().peekable();
    assert_eq!(expected_dict, Dictionary::parse(&mut input1)?);
    assert_eq!(expected_dict, Dictionary::parse(&mut input2)?);
    assert_eq!(expected_dict, Dictionary::parse(&mut input3)?);

    Ok(())
}

#[test]
fn parse_bare_item() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        BareItem::Boolean(false),
        Parser::parse_bare_item(&mut "?0".chars().peekable())?
    );
    assert_eq!(
        BareItem::String("test string".to_owned()),
        Parser::parse_bare_item(&mut "\"test string\"".chars().peekable())?
    );
    assert_eq!(
        BareItem::Token("*token".to_owned()),
        Parser::parse_bare_item(&mut "*token".chars().peekable())?
    );
    assert_eq!(
        BareItem::ByteSeq("base_64 encoding test".to_owned().into_bytes()),
        Parser::parse_bare_item(&mut ":YmFzZV82NCBlbmNvZGluZyB0ZXN0:".chars().peekable())?
    );
    assert_eq!(
        BareItem::Decimal(Decimal::from_str("-3.55")?),
        Parser::parse_bare_item(&mut "-3.55".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_bare_item_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_bare_item: item type can't be identified"),
        Parser::parse_bare_item(&mut "!?0".chars().peekable())
    );
    assert_eq!(
        Err("parse_bare_item: item type can't be identified"),
        Parser::parse_bare_item(&mut "_11abc".chars().peekable())
    );
    assert_eq!(
        Err("parse_bare_item: item type can't be identified"),
        Parser::parse_bare_item(&mut "   ".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_bool() -> Result<(), Box<dyn Error>> {
    let mut input = "?0gk".chars().peekable();
    assert_eq!(false, Parser::parse_bool(&mut input)?);
    assert_eq!(input.collect::<String>(), "gk");

    assert_eq!(false, Parser::parse_bool(&mut "?0".chars().peekable())?);
    assert_eq!(true, Parser::parse_bool(&mut "?1".chars().peekable())?);
    Ok(())
}

#[test]
fn parse_bool_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_bool: first character is not '?'"),
        Parser::parse_bool(&mut "".chars().peekable())
    );
    assert_eq!(
        Err("parse_bool: invalid variant"),
        Parser::parse_bool(&mut "?".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_string() -> Result<(), Box<dyn Error>> {
    let mut input = "\"some string\" ;not string".chars().peekable();
    assert_eq!("some string".to_owned(), Parser::parse_string(&mut input)?);
    assert_eq!(input.collect::<String>(), " ;not string");

    assert_eq!(
        "test".to_owned(),
        Parser::parse_string(&mut "\"test\"".chars().peekable())?
    );
    assert_eq!(
        r#"te\st"#.to_owned(),
        Parser::parse_string(&mut "\"te\\\\st\"".chars().peekable())?
    );
    assert_eq!(
        "".to_owned(),
        Parser::parse_string(&mut "\"\"".chars().peekable())?
    );
    assert_eq!(
        "some string".to_owned(),
        Parser::parse_string(&mut "\"some string\"".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_string_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_string: first character is not '\"'"),
        Parser::parse_string(&mut "test".chars().peekable())
    );
    assert_eq!(
        Err("parse_string: last input character is '\\'"),
        Parser::parse_string(&mut "\"\\".chars().peekable())
    );
    assert_eq!(
        Err("parse_string: disallowed character after '\\'"),
        Parser::parse_string(&mut "\"\\l\"".chars().peekable())
    );
    assert_eq!(
        Err("parse_string: not a visible character"),
        Parser::parse_string(&mut "\"\u{1f}\"".chars().peekable())
    );
    assert_eq!(
        Err("parse_string: no closing '\"'"),
        Parser::parse_string(&mut "\"smth".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_token() -> Result<(), Box<dyn Error>> {
    let mut input = "*some:token}not token".chars().peekable();
    assert_eq!("*some:token".to_owned(), Parser::parse_token(&mut input)?);
    assert_eq!(input.collect::<String>(), "}not token");

    assert_eq!(
        "token".to_owned(),
        Parser::parse_token(&mut "token".chars().peekable())?
    );
    assert_eq!(
        "a_b-c.d3:f%00/*".to_owned(),
        Parser::parse_token(&mut "a_b-c.d3:f%00/*".chars().peekable())?
    );
    assert_eq!(
        "TestToken".to_owned(),
        Parser::parse_token(&mut "TestToken".chars().peekable())?
    );
    assert_eq!(
        "some".to_owned(),
        Parser::parse_token(&mut "some@token".chars().peekable())?
    );
    assert_eq!(
        "*TestToken*".to_owned(),
        Parser::parse_token(&mut "*TestToken*".chars().peekable())?
    );
    assert_eq!(
        "*".to_owned(),
        Parser::parse_token(&mut "*[@:token".chars().peekable())?
    );
    assert_eq!(
        "test".to_owned(),
        Parser::parse_token(&mut "test token".chars().peekable())?
    );

    Ok(())
}

#[test]
fn parse_token_errors() -> Result<(), Box<dyn Error>> {
    let mut input = "765token".chars().peekable();
    assert_eq!(
        Err("parse_token: first character is not ALPHA or '*'"),
        Parser::parse_token(&mut input)
    );
    assert_eq!(input.collect::<String>(), "765token");

    assert_eq!(
        Err("parse_token: first character is not ALPHA or '*'"),
        Parser::parse_token(&mut "7token".chars().peekable())
    );
    assert_eq!(
        Err("parse_token: empty input string"),
        Parser::parse_token(&mut "".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_byte_sequence() -> Result<(), Box<dyn Error>> {
    let mut input = ":aGVsbG8:rest_of_str".chars().peekable();
    assert_eq!(
        "hello".to_owned().into_bytes(),
        Parser::parse_byte_sequence(&mut input)?
    );
    assert_eq!("rest_of_str", input.collect::<String>());

    assert_eq!(
        "hello".to_owned().into_bytes(),
        Parser::parse_byte_sequence(&mut ":aGVsbG8:".chars().peekable())?
    );
    assert_eq!(
        "test_encode".to_owned().into_bytes(),
        Parser::parse_byte_sequence(&mut ":dGVzdF9lbmNvZGU:".chars().peekable())?
    );
    assert_eq!(
        "new:year tree".to_owned().into_bytes(),
        Parser::parse_byte_sequence(&mut ":bmV3OnllYXIgdHJlZQ==:".chars().peekable())?
    );
    assert_eq!(
        "".to_owned().into_bytes(),
        Parser::parse_byte_sequence(&mut "::".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_byte_sequence_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_byte_seq: first char is not ':'"),
        Parser::parse_byte_sequence(&mut "aGVsbG8".chars().peekable())
    );
    assert_eq!(
        Err("parse_byte_seq: invalid char in byte sequence"),
        Parser::parse_byte_sequence(&mut ":aGVsb G8=:".chars().peekable())
    );
    assert_eq!(
        Err("parse_byte_seq: no closing ':'"),
        Parser::parse_byte_sequence(&mut ":aGVsbG8=".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_number_int() -> Result<(), Box<dyn Error>> {
    let mut input = "-733333333332d.14".chars().peekable();
    assert_eq!(
        Num::Integer(-733333333332),
        Parser::parse_number(&mut input)?
    );
    assert_eq!("d.14", input.collect::<String>());

    assert_eq!(
        Num::Integer(42),
        Parser::parse_number(&mut "42".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(-42),
        Parser::parse_number(&mut "-42".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(-42),
        Parser::parse_number(&mut "-042".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(0),
        Parser::parse_number(&mut "0".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(0),
        Parser::parse_number(&mut "00".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(123456789012345),
        Parser::parse_number(&mut "123456789012345".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(-123456789012345),
        Parser::parse_number(&mut "-123456789012345".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(2),
        Parser::parse_number(&mut "2,3".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(4),
        Parser::parse_number(&mut "4-2".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(-999999999999999),
        Parser::parse_number(&mut "-999999999999999".chars().peekable())?
    );
    assert_eq!(
        Num::Integer(999999999999999),
        Parser::parse_number(&mut "999999999999999".chars().peekable())?
    );

    Ok(())
}

#[test]
fn parse_number_decimal() -> Result<(), Box<dyn Error>> {
    let mut input = "00.42 test string".chars().peekable();
    assert_eq!(
        Num::Decimal(Decimal::from_str("0.42")?),
        Parser::parse_number(&mut input)?
    );
    assert_eq!(" test string", input.collect::<String>());

    assert_eq!(
        Num::Decimal(Decimal::from_str("1.5")?),
        Parser::parse_number(&mut "1.5.4.".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("1.8")?),
        Parser::parse_number(&mut "1.8.".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("1.7")?),
        Parser::parse_number(&mut "1.7.0".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("3.14")?),
        Parser::parse_number(&mut "3.14".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("-3.14")?),
        Parser::parse_number(&mut "-3.14".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("123456789012.1")?),
        Parser::parse_number(&mut "123456789012.1".chars().peekable())?
    );
    assert_eq!(
        Num::Decimal(Decimal::from_str("1234567890.112")?),
        Parser::parse_number(&mut "1234567890.112".chars().peekable())?
    );

    Ok(())
}

#[test]
fn parse_number_errors() -> Result<(), Box<dyn Error>> {
    let mut input = ":aGVsbG8:rest".chars().peekable();
    assert_eq!(
        Err("parse_number: input number does not start with a digit"),
        Parser::parse_number(&mut input)
    );
    assert_eq!(":aGVsbG8:rest", input.collect::<String>());

    let mut input = "-11.5555 test string".chars().peekable();
    assert_eq!(
        Err("parse_number: invalid decimal fraction length"),
        Parser::parse_number(&mut input)
    );
    assert_eq!(" test string", input.collect::<String>());

    assert_eq!(
        Err("parse_number: input number does not start with a digit"),
        Parser::parse_number(&mut "--0".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal too long, illegal position for decimal point"),
        Parser::parse_number(&mut "1999999999999.1".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal ends with '.'"),
        Parser::parse_number(&mut "19888899999.".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: integer too long, length > 15"),
        Parser::parse_number(&mut "1999999999999999".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal too long, length > 16"),
        Parser::parse_number(&mut "19999999999.99991".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: input number does not start with a digit"),
        Parser::parse_number(&mut "- 42".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: input number does not start with a digit"),
        Parser::parse_number(&mut "- 42".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal ends with '.'"),
        Parser::parse_number(&mut "1..4".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: input number lacks a digit"),
        Parser::parse_number(&mut "-".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal ends with '.'"),
        Parser::parse_number(&mut "-5. 14".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal ends with '.'"),
        Parser::parse_number(&mut "7. 1".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: invalid decimal fraction length"),
        Parser::parse_number(&mut "-7.3333333333".chars().peekable())
    );
    assert_eq!(
        Err("parse_number: decimal too long, illegal position for decimal point"),
        Parser::parse_number(&mut "-7333333333323.12".chars().peekable())
    );

    Ok(())
}

#[test]
fn parse_params_string() -> Result<(), Box<dyn Error>> {
    let mut input = ";b=\"param_val\"".chars().peekable();
    let expected = Parameters::from_iter(vec![(
        "b".to_owned(),
        BareItem::String("param_val".to_owned()),
    )]);
    assert_eq!(expected, Parser::parse_parameters(&mut input)?);
    Ok(())
}

#[test]
fn parse_params_bool() -> Result<(), Box<dyn Error>> {
    let mut input = ";b;a".chars().peekable();
    let expected = Parameters::from_iter(vec![
        ("b".to_owned(), BareItem::Boolean(true)),
        ("a".to_owned(), BareItem::Boolean(true)),
    ]);
    assert_eq!(expected, Parser::parse_parameters(&mut input)?);
    Ok(())
}

#[test]
fn parse_params_mixed_types() -> Result<(), Box<dyn Error>> {
    let mut input = ";key1=?0;key2=746.15".chars().peekable();
    let expected = Parameters::from_iter(vec![
        ("key1".to_owned(), BareItem::Boolean(false)),
        ("key2".to_owned(), Decimal::from_str("746.15")?.into()),
    ]);
    assert_eq!(expected, Parser::parse_parameters(&mut input)?);
    Ok(())
}

#[test]
fn parse_params_with_spaces() -> Result<(), Box<dyn Error>> {
    let mut input = "; key1=?0; key2=11111".chars().peekable();
    let expected = Parameters::from_iter(vec![
        ("key1".to_owned(), BareItem::Boolean(false)),
        ("key2".to_owned(), 11111.into()),
    ]);
    assert_eq!(expected, Parser::parse_parameters(&mut input)?);
    Ok(())
}

#[test]
fn parse_params_empty() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Parameters::new(),
        Parser::parse_parameters(&mut " key1=?0; key2=11111".chars().peekable())?
    );
    assert_eq!(
        Parameters::new(),
        Parser::parse_parameters(&mut "".chars().peekable())?
    );
    assert_eq!(
        Parameters::new(),
        Parser::parse_parameters(&mut "[;a=1".chars().peekable())?
    );
    assert_eq!(
        Parameters::new(),
        Parser::parse_parameters(&mut String::new().chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_key() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        "a".to_owned(),
        Parser::parse_key(&mut "a=1".chars().peekable())?
    );
    assert_eq!(
        "a1".to_owned(),
        Parser::parse_key(&mut "a1=10".chars().peekable())?
    );
    assert_eq!(
        "*1".to_owned(),
        Parser::parse_key(&mut "*1=10".chars().peekable())?
    );
    assert_eq!(
        "f".to_owned(),
        Parser::parse_key(&mut "f[f=10".chars().peekable())?
    );
    Ok(())
}

#[test]
fn parse_key_errors() -> Result<(), Box<dyn Error>> {
    assert_eq!(
        Err("parse_key: first character is not lcalpha or '*'"),
        Parser::parse_key(&mut "[*f=10".chars().peekable())
    );
    Ok(())
}

#[test]
fn parse_more_list() -> Result<(), Box<dyn Error>> {
    let item1 = Item::new(1.into());
    let item2 = Item::new(2.into());
    let item3 = Item::new(42.into());
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let expected_list: List = vec![inner_list_1.into(), item3.into()];

    let mut parsed_header = Parser::parse_list("(1 2)".as_bytes())?;
    let _ = parsed_header.parse_more("42".as_bytes())?;
    assert_eq!(expected_list, parsed_header);
    Ok(())
}

#[test]
fn parse_more_dict() -> Result<(), Box<dyn Error>> {
    let item2_params =
        Parameters::from_iter(vec![("foo".to_owned(), BareItem::Token("*".to_owned()))]);
    let item1 = Item::new(1.into());
    let item2 = Item::with_params(BareItem::Boolean(true), item2_params);
    let item3 = Item::new(3.into());
    let expected_dict = Dictionary::from_iter(vec![
        ("a".to_owned(), item1.into()),
        ("b".to_owned(), item2.into()),
        ("c".to_owned(), item3.into()),
    ]);

    let mut parsed_header = Parser::parse_dictionary("a=1, b;foo=*\t\t".as_bytes())?;
    let _ = parsed_header.parse_more(" c=3".as_bytes())?;
    assert_eq!(expected_dict, parsed_header);
    Ok(())
}

#[test]
fn parse_more_errors() -> Result<(), Box<dyn Error>> {
    let parsed_dict_header =
        Parser::parse_dictionary("a=1, b;foo=*".as_bytes())?.parse_more(",a".as_bytes());
    assert!(parsed_dict_header.is_err());

    let parsed_list_header =
        Parser::parse_list("a, b;foo=*".as_bytes())?.parse_more("(a, 2)".as_bytes());
    assert!(parsed_list_header.is_err());
    Ok(())
}
