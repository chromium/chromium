use crate::serializer::Serializer;
use crate::FromStr;
use crate::SerializeValue;
use crate::{BareItem, Decimal, Dictionary, InnerList, Item, List, Parameters};
use std::error::Error;
use std::iter::FromIterator;

#[test]
fn serialize_value_empty_dict() -> Result<(), Box<dyn Error>> {
    let dict_field_value = Dictionary::new();
    assert_eq!(
        Err("serialize_dictionary: serializing empty field is not allowed"),
        dict_field_value.serialize_value()
    );
    Ok(())
}

#[test]
fn serialize_value_empty_list() -> Result<(), Box<dyn Error>> {
    let list_field_value = List::new();
    assert_eq!(
        Err("serialize_list: serializing empty field is not allowed"),
        list_field_value.serialize_value()
    );
    Ok(())
}

#[test]
fn serialize_value_list_mixed_members_with_params() -> Result<(), Box<dyn Error>> {
    let item1 = Item::new(Decimal::from_str("42.4568")?.into());
    let item2_param = Parameters::from_iter(vec![("itm2_p".to_owned(), BareItem::Boolean(true))]);
    let item2 = Item::with_params(17.into(), item2_param);

    let inner_list_item1_param =
        Parameters::from_iter(vec![("in1_p".to_owned(), BareItem::Boolean(false))]);
    let inner_list_item1 =
        Item::with_params(BareItem::String("str1".to_owned()), inner_list_item1_param);
    let inner_list_item2_param = Parameters::from_iter(vec![(
        "in2_p".to_owned(),
        BareItem::String("valu\\e".to_owned()),
    )]);
    let inner_list_item2 =
        Item::with_params(BareItem::Token("str2".to_owned()), inner_list_item2_param);
    let inner_list_param = Parameters::from_iter(vec![(
        "inner_list_param".to_owned(),
        BareItem::ByteSeq("weather".as_bytes().to_vec()),
    )]);
    let inner_list =
        InnerList::with_params(vec![inner_list_item1, inner_list_item2], inner_list_param);

    let list_field_value: List = vec![item1.into(), item2.into(), inner_list.into()];
    let expected = "42.457, 17;itm2_p, (\"str1\";in1_p=?0 str2;in2_p=\"valu\\\\e\");inner_list_param=:d2VhdGhlcg==:";
    assert_eq!(expected, list_field_value.serialize_value()?);
    Ok(())
}

#[test]
fn serialize_value_errors() -> Result<(), Box<dyn Error>> {
    let disallowed_item = Item::new(BareItem::String("non-ascii text 🐹".into()));
    assert_eq!(
        Err("serialize_string: non-ascii character"),
        disallowed_item.serialize_value()
    );

    let disallowed_item = Item::new(Decimal::from_str("12345678912345.123")?.into());
    assert_eq!(
        Err("serialize_decimal: integer component > 12 digits"),
        disallowed_item.serialize_value()
    );

    let param_with_disallowed_key = Parameters::from_iter(vec![("_key".to_owned(), 13.into())]);
    let disallowed_item = Item::with_params(12.into(), param_with_disallowed_key);
    assert_eq!(
        Err("serialize_key: first character is not lcalpha or '*'"),
        disallowed_item.serialize_value()
    );
    Ok(())
}

#[test]
fn serialize_item_byteseq_with_param() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let item_param = ("a".to_owned(), BareItem::Token("*ab_1".into()));
    let item_param = Parameters::from_iter(vec![item_param]);
    let item = Item::with_params(BareItem::ByteSeq("parser".as_bytes().to_vec()), item_param);
    Serializer::serialize_item(&item, &mut buf)?;
    assert_eq!(":cGFyc2Vy:;a=*ab_1", &buf);
    Ok(())
}

#[test]
fn serialize_item_without_params() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    let item = Item::new(1.into());
    Serializer::serialize_item(&item, &mut buf)?;
    assert_eq!("1", &buf);
    Ok(())
}

#[test]
fn serialize_item_with_bool_true_param() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    let param = Parameters::from_iter(vec![("a".to_owned(), BareItem::Boolean(true))]);
    let item = Item::with_params(Decimal::from_str("12.35")?.into(), param);
    Serializer::serialize_item(&item, &mut buf)?;
    assert_eq!("12.35;a", &buf);
    Ok(())
}

#[test]
fn serialize_item_with_token_param() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    let param = Parameters::from_iter(vec![("a1".to_owned(), BareItem::Token("*tok".to_owned()))]);
    let item = Item::with_params(BareItem::String("12.35".to_owned()), param);
    Serializer::serialize_item(&item, &mut buf)?;
    assert_eq!("\"12.35\";a1=*tok", &buf);
    Ok(())
}

#[test]
fn serialize_integer() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_integer(-12, &mut buf)?;
    assert_eq!("-12", &buf);

    buf.clear();
    Serializer::serialize_integer(0, &mut buf)?;
    assert_eq!("0", &buf);

    buf.clear();
    Serializer::serialize_integer(999_999_999_999_999, &mut buf)?;
    assert_eq!("999999999999999", &buf);

    buf.clear();
    Serializer::serialize_integer(-999_999_999_999_999, &mut buf)?;
    assert_eq!("-999999999999999", &buf);
    Ok(())
}

#[test]
fn serialize_integer_errors() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    assert_eq!(
        Err("serialize_integer: integer is out of range"),
        Serializer::serialize_integer(1_000_000_000_000_000, &mut buf)
    );

    buf.clear();
    assert_eq!(
        Err("serialize_integer: integer is out of range"),
        Serializer::serialize_integer(-1_000_000_000_000_000, &mut buf)
    );
    Ok(())
}

#[test]
fn serialize_decimal() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_decimal(Decimal::from_str("-99.1346897")?, &mut buf)?;
    assert_eq!("-99.135", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("-1.00")?, &mut buf)?;
    assert_eq!("-1.0", &buf);

    buf.clear();
    Serializer::serialize_decimal(
        Decimal::from_str("-00000000000000000000000099.1346897")?,
        &mut buf,
    )?;
    assert_eq!("-99.135", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("100.13")?, &mut buf)?;
    assert_eq!("100.13", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("-100.130")?, &mut buf)?;
    assert_eq!("-100.130", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("-137.0")?, &mut buf)?;
    assert_eq!("-137.0", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("137121212112.123")?, &mut buf)?;
    assert_eq!("137121212112.123", &buf);

    buf.clear();
    Serializer::serialize_decimal(Decimal::from_str("137121212112.1238")?, &mut buf)?;
    assert_eq!("137121212112.124", &buf);
    Ok(())
}

#[test]
fn serialize_decimal_errors() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    assert_eq!(
        Err("serialize_decimal: integer component > 12 digits"),
        Serializer::serialize_decimal(Decimal::from_str("1371212121121.1")?, &mut buf)
    );
    Ok(())
}

#[test]
fn serialize_string() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_string("1.1 text", &mut buf)?;
    assert_eq!("\"1.1 text\"", &buf);

    buf.clear();
    Serializer::serialize_string("hello \"name\"", &mut buf)?;
    assert_eq!("\"hello \\\"name\\\"\"", &buf);

    buf.clear();
    Serializer::serialize_string("something\\nothing", &mut buf)?;
    assert_eq!("\"something\\\\nothing\"", &buf);

    buf.clear();
    Serializer::serialize_string("", &mut buf)?;
    assert_eq!("\"\"", &buf);

    buf.clear();
    Serializer::serialize_string(" ", &mut buf)?;
    assert_eq!("\" \"", &buf);

    buf.clear();
    Serializer::serialize_string("    ", &mut buf)?;
    assert_eq!("\"    \"", &buf);
    Ok(())
}

#[test]
fn serialize_string_errors() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    assert_eq!(
        Err("serialize_string: not a visible character"),
        Serializer::serialize_string("text \x00", &mut buf)
    );

    assert_eq!(
        Err("serialize_string: not a visible character"),
        Serializer::serialize_string("text \x1f", &mut buf)
    );
    assert_eq!(
        Err("serialize_string: not a visible character"),
        Serializer::serialize_string("text \x7f", &mut buf)
    );
    assert_eq!(
        Err("serialize_string: non-ascii character"),
        Serializer::serialize_string("рядок", &mut buf)
    );
    Ok(())
}

#[test]
fn serialize_token() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_token("*", &mut buf)?;
    assert_eq!("*", &buf);

    buf.clear();
    Serializer::serialize_token("abc", &mut buf)?;
    assert_eq!("abc", &buf);

    buf.clear();
    Serializer::serialize_token("abc:de", &mut buf)?;
    assert_eq!("abc:de", &buf);

    buf.clear();
    Serializer::serialize_token("smth/#!else", &mut buf)?;
    assert_eq!("smth/#!else", &buf);
    Ok(())
}

#[test]
fn serialize_token_errors() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    assert_eq!(
        Err("serialize_token: token is empty"),
        Serializer::serialize_token("", &mut buf)
    );
    assert_eq!(
        Err("serialize_token: first character is not ALPHA or '*'"),
        Serializer::serialize_token("#some", &mut buf)
    );
    assert_eq!(
        Err("serialize_token: disallowed character"),
        Serializer::serialize_token("s ", &mut buf)
    );
    assert_eq!(
        Err("serialize_token: disallowed character"),
        Serializer::serialize_token("abc:de\t", &mut buf)
    );
    Ok(())
}

#[test]
fn serialize_byte_sequence() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_byte_sequence("hello".as_bytes(), &mut buf)?;
    assert_eq!(":aGVsbG8=:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("test_encode".as_bytes(), &mut buf)?;
    assert_eq!(":dGVzdF9lbmNvZGU=:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("".as_bytes(), &mut buf)?;
    assert_eq!("::", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("pleasure.".as_bytes(), &mut buf)?;
    assert_eq!(":cGxlYXN1cmUu:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("leasure.".as_bytes(), &mut buf)?;
    assert_eq!(":bGVhc3VyZS4=:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("easure.".as_bytes(), &mut buf)?;
    assert_eq!(":ZWFzdXJlLg==:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("asure.".as_bytes(), &mut buf)?;
    assert_eq!(":YXN1cmUu:", &buf);

    buf.clear();
    Serializer::serialize_byte_sequence("sure.".as_bytes(), &mut buf)?;
    assert_eq!(":c3VyZS4=:", &buf);

    Ok(())
}

#[test]
fn serialize_bool() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_bool(true, &mut buf)?;
    assert_eq!("?1", &buf);

    buf.clear();
    Serializer::serialize_bool(false, &mut buf)?;
    assert_eq!("?0", &buf);
    Ok(())
}

#[test]
fn serialize_params_bool() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let input = Parameters::from_iter(vec![
        ("*b".to_owned(), BareItem::Boolean(true)),
        ("a.a".to_owned(), BareItem::Boolean(true)),
    ]);

    Serializer::serialize_parameters(&input, &mut buf)?;
    assert_eq!(";*b;a.a", &buf);
    Ok(())
}

#[test]
fn serialize_params_string() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let input = Parameters::from_iter(vec![(
        "b".to_owned(),
        BareItem::String("param_val".to_owned()),
    )]);
    Serializer::serialize_parameters(&input, &mut buf)?;
    assert_eq!(";b=\"param_val\"", &buf);
    Ok(())
}

#[test]
fn serialize_params_numbers() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let input = Parameters::from_iter(vec![
        ("key1".to_owned(), Decimal::from_str("746.15")?.into()),
        ("key2".to_owned(), 11111.into()),
    ]);
    Serializer::serialize_parameters(&input, &mut buf)?;
    assert_eq!(";key1=746.15;key2=11111", &buf);
    Ok(())
}

#[test]
fn serialize_params_mixed_types() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let input = Parameters::from_iter(vec![
        ("key1".to_owned(), BareItem::Boolean(false)),
        ("key2".to_owned(), Decimal::from_str("1354.091878")?.into()),
    ]);
    Serializer::serialize_parameters(&input, &mut buf)?;
    assert_eq!(";key1=?0;key2=1354.092", &buf);
    Ok(())
}

#[test]
fn serialize_key() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();
    Serializer::serialize_key("*a_fg", &mut buf)?;
    assert_eq!("*a_fg", &buf);

    buf.clear();
    Serializer::serialize_key("*a_fg*", &mut buf)?;
    assert_eq!("*a_fg*", &buf);

    buf.clear();
    Serializer::serialize_key("key1", &mut buf)?;
    assert_eq!("key1", &buf);

    buf.clear();
    Serializer::serialize_key("ke-y.1", &mut buf)?;
    assert_eq!("ke-y.1", &buf);

    Ok(())
}

#[test]
fn serialize_key_errors() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    assert_eq!(
        Err("serialize_key: key is empty"),
        Serializer::serialize_key("", &mut buf)
    );
    assert_eq!(
        Err("serialize_key: disallowed character in input"),
        Serializer::serialize_key("aND", &mut buf)
    );
    assert_eq!(
        Err("serialize_key: first character is not lcalpha or '*'"),
        Serializer::serialize_key("_key", &mut buf)
    );
    assert_eq!(
        Err("serialize_key: first character is not lcalpha or '*'"),
        Serializer::serialize_key("7key", &mut buf)
    );
    Ok(())
}

#[test]
fn serialize_list_of_items_and_inner_list() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let item1 = Item::new(12.into());
    let item2 = Item::new(14.into());
    let item3 = Item::new(BareItem::Token("a".to_owned()));
    let item4 = Item::new(BareItem::Token("b".to_owned()));
    let inner_list_param = Parameters::from_iter(vec![(
        "param".to_owned(),
        BareItem::String("param_value_1".to_owned()),
    )]);
    let inner_list = InnerList::with_params(vec![item3, item4], inner_list_param);
    let input: List = vec![item1.into(), item2.into(), inner_list.into()];

    Serializer::serialize_list(&input, &mut buf)?;
    assert_eq!("12, 14, (a b);param=\"param_value_1\"", &buf);
    Ok(())
}

#[test]
fn serialize_list_of_lists() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let item1 = Item::new(1.into());
    let item2 = Item::new(2.into());
    let item3 = Item::new(42.into());
    let item4 = Item::new(43.into());
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let inner_list_2 = InnerList::new(vec![item3, item4]);
    let input: List = vec![inner_list_1.into(), inner_list_2.into()];

    Serializer::serialize_list(&input, &mut buf)?;
    assert_eq!("(1 2), (42 43)", &buf);
    Ok(())
}

#[test]
fn serialize_list_with_bool_item_and_bool_params() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let item1_params = Parameters::from_iter(vec![
        ("a".to_owned(), BareItem::Boolean(true)),
        ("b".to_owned(), BareItem::Boolean(false)),
    ]);
    let item1 = Item::with_params(BareItem::Boolean(false), item1_params);
    let item2 = Item::new(BareItem::Token("cde_456".to_owned()));

    let input: List = vec![item1.into(), item2.into()];
    Serializer::serialize_list(&input, &mut buf)?;
    assert_eq!("?0;a;b=?0, cde_456", &buf);
    Ok(())
}

#[test]
fn serialize_dictionary_with_params() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let item1_params = Parameters::from_iter(vec![
        ("a".to_owned(), 1.into()),
        ("b".to_owned(), BareItem::Boolean(true)),
    ]);
    let item2_params = Parameters::new();
    let item3_params = Parameters::from_iter(vec![
        ("q".to_owned(), BareItem::Boolean(false)),
        ("r".to_owned(), BareItem::String("+w".to_owned())),
    ]);

    let item1 = Item::with_params(123.into(), item1_params);
    let item2 = Item::with_params(456.into(), item2_params);
    let item3 = Item::with_params(789.into(), item3_params);

    let input = Dictionary::from_iter(vec![
        ("abc".to_owned(), item1.into()),
        ("def".to_owned(), item2.into()),
        ("ghi".to_owned(), item3.into()),
    ]);

    Serializer::serialize_dict(&input, &mut buf)?;
    assert_eq!("abc=123;a=1;b, def=456, ghi=789;q=?0;r=\"+w\"", &buf);
    Ok(())
}

#[test]
fn serialize_dict_empty_member_value() -> Result<(), Box<dyn Error>> {
    let mut buf = String::new();

    let inner_list = InnerList::new(vec![]);
    let input = Dictionary::from_iter(vec![("a".to_owned(), inner_list.into())]);
    Serializer::serialize_dict(&input, &mut buf)?;
    assert_eq!("a=()", &buf);
    Ok(())
}
