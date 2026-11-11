#[cfg(feature = "parsed-types")]
use crate::{BareItem, Dictionary, FieldType, InnerList, Item, List, Parameters};
use crate::{Date, Decimal, integer, key_ref, serializer, string_ref, token_ref};

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_value_empty_dict() {
    let dict_field_value = Dictionary::new();
    assert_eq!(None, dict_field_value.serialize());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_value_empty_list() {
    let list_field_value = List::new();
    assert_eq!(None, list_field_value.serialize());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_value_list_mixed_members_with_params() {
    let item1 = Item::new(Decimal::from_integer_scaled_1000(integer(42_457)));
    let item2_param = Parameters::from_iter(vec![(
        key_ref("itm2_p").to_owned(),
        BareItem::Boolean(true),
    )]);
    let item2 = Item::with_params(17, item2_param);

    let inner_list_item1_param = Parameters::from_iter(vec![(
        key_ref("in1_p").to_owned(),
        BareItem::Boolean(false),
    )]);
    let inner_list_item1 = Item::with_params(string_ref("str1"), inner_list_item1_param);
    let inner_list_item2_param = Parameters::from_iter(vec![(
        key_ref("in2_p").to_owned(),
        BareItem::String(string_ref("valu\\e").to_owned()),
    )]);
    let inner_list_item2 = Item::with_params(token_ref("str2"), inner_list_item2_param);
    let inner_list_param = Parameters::from_iter(vec![(
        key_ref("inner_list_param").to_owned(),
        BareItem::ByteSequence(b"weather".to_vec()),
    )]);
    let inner_list =
        InnerList::with_params(vec![inner_list_item1, inner_list_item2], inner_list_param);

    let list_field_value: List = vec![item1.into(), item2.into(), inner_list.into()];
    assert_eq!(
        Some(
            r#"42.457, 17;itm2_p, ("str1";in1_p=?0 str2;in2_p="valu\\e");inner_list_param=:d2VhdGhlcg==:"#
        ),
        list_field_value.serialize().as_deref(),
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_item_byteseq_with_param() {
    let item_param = (
        key_ref("a").to_owned(),
        BareItem::Token(token_ref("*ab_1").to_owned()),
    );
    let item_param = Parameters::from_iter(vec![item_param]);
    let item = Item::with_params(b"parser".to_vec(), item_param);
    assert_eq!(":cGFyc2Vy:;a=*ab_1", item.serialize());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_item_without_params() {
    let item = Item::new(1);
    assert_eq!("1", item.serialize());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_item_with_bool_true_param() {
    let param = Parameters::from_iter(vec![(key_ref("a").to_owned(), BareItem::Boolean(true))]);
    let item = Item::with_params(Decimal::from_integer_scaled_1000(integer(12_350)), param);
    assert_eq!("12.35;a", item.serialize());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_item_with_token_param() {
    let param = Parameters::from_iter(vec![(
        key_ref("a1").to_owned(),
        BareItem::Token(token_ref("*tok").to_owned()),
    )]);
    let item = Item::with_params(string_ref("12.35"), param);
    assert_eq!(r#""12.35";a1=*tok"#, item.serialize());
}

#[test]
fn serialize_integer() {
    let mut buf = String::new();
    serializer::serialize_integer(integer(-12), &mut buf);
    assert_eq!("-12", &buf);

    buf.clear();
    serializer::serialize_integer(integer(0), &mut buf);
    assert_eq!("0", &buf);

    buf.clear();
    serializer::serialize_integer(integer(999_999_999_999_999), &mut buf);
    assert_eq!("999999999999999", &buf);

    buf.clear();
    serializer::serialize_integer(integer(-999_999_999_999_999), &mut buf);
    assert_eq!("-999999999999999", &buf);
}

#[test]
fn serialize_decimal() {
    let mut buf = String::new();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(-99_135)),
        &mut buf,
    );
    assert_eq!("-99.135", &buf);

    buf.clear();
    serializer::serialize_decimal(Decimal::from_integer_scaled_1000(integer(-1_000)), &mut buf);
    assert_eq!("-1.0", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(-99_135)),
        &mut buf,
    );
    assert_eq!("-99.135", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(100_130)),
        &mut buf,
    );
    assert_eq!("100.13", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(-100_130)),
        &mut buf,
    );
    assert_eq!("-100.13", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(-100_100)),
        &mut buf,
    );
    assert_eq!("-100.1", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(-137_000)),
        &mut buf,
    );
    assert_eq!("-137.0", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(137_121_212_112_123)),
        &mut buf,
    );
    assert_eq!("137121212112.123", &buf);

    buf.clear();
    serializer::serialize_decimal(
        Decimal::from_integer_scaled_1000(integer(137_121_212_112_124)),
        &mut buf,
    );
    assert_eq!("137121212112.124", &buf);
}

#[test]
fn serialize_string() {
    let mut buf = String::new();
    serializer::serialize_string(string_ref("1.1 text"), &mut buf);
    assert_eq!(r#""1.1 text""#, &buf);

    buf.clear();
    serializer::serialize_string(string_ref(r#"hello "name""#), &mut buf);
    assert_eq!(r#""hello \"name\"""#, &buf);

    buf.clear();
    serializer::serialize_string(string_ref("something\\nothing"), &mut buf);
    assert_eq!(r#""something\\nothing""#, &buf);

    buf.clear();
    serializer::serialize_string(string_ref(""), &mut buf);
    assert_eq!(r#""""#, &buf);

    buf.clear();
    serializer::serialize_string(string_ref(" "), &mut buf);
    assert_eq!(r#"" ""#, &buf);

    buf.clear();
    serializer::serialize_string(string_ref("    "), &mut buf);
    assert_eq!(r#""    ""#, &buf);
}

#[test]
fn serialize_token() {
    let mut buf = String::new();
    serializer::serialize_token(token_ref("*"), &mut buf);
    assert_eq!("*", &buf);

    buf.clear();
    serializer::serialize_token(token_ref("abc"), &mut buf);
    assert_eq!("abc", &buf);

    buf.clear();
    serializer::serialize_token(token_ref("abc:de"), &mut buf);
    assert_eq!("abc:de", &buf);

    buf.clear();
    serializer::serialize_token(token_ref("smth/#!else"), &mut buf);
    assert_eq!("smth/#!else", &buf);
}

#[test]
fn serialize_byte_sequence() {
    let mut buf = String::new();
    serializer::serialize_byte_sequence("hello".as_bytes(), &mut buf);
    assert_eq!(":aGVsbG8=:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("test_encode".as_bytes(), &mut buf);
    assert_eq!(":dGVzdF9lbmNvZGU=:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("".as_bytes(), &mut buf);
    assert_eq!("::", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("pleasure.".as_bytes(), &mut buf);
    assert_eq!(":cGxlYXN1cmUu:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("leasure.".as_bytes(), &mut buf);
    assert_eq!(":bGVhc3VyZS4=:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("easure.".as_bytes(), &mut buf);
    assert_eq!(":ZWFzdXJlLg==:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("asure.".as_bytes(), &mut buf);
    assert_eq!(":YXN1cmUu:", &buf);

    buf.clear();
    serializer::serialize_byte_sequence("sure.".as_bytes(), &mut buf);
    assert_eq!(":c3VyZS4=:", &buf);
}

#[test]
fn serialize_bool() {
    let mut buf = String::new();
    serializer::serialize_bool(true, &mut buf);
    assert_eq!("?1", &buf);

    buf.clear();
    serializer::serialize_bool(false, &mut buf);
    assert_eq!("?0", &buf);
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_params_bool() {
    let mut buf = String::new();
    serializer::serialize_parameter(key_ref("*b"), true, &mut buf);
    assert_eq!(";*b", buf);

    buf.clear();
    serializer::serialize_parameter(key_ref("a.a"), false, &mut buf);
    assert_eq!(";a.a=?0", buf);
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_params_string() {
    let mut buf = String::new();

    serializer::serialize_parameter(key_ref("b"), string_ref("param_val"), &mut buf);
    assert_eq!(r#";b="param_val""#, buf);
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_params_numbers() {
    let mut buf = String::new();

    serializer::serialize_parameter(
        key_ref("key1"),
        Decimal::from_integer_scaled_1000(integer(746_150)),
        &mut buf,
    );
    assert_eq!(";key1=746.15", buf);

    buf.clear();
    serializer::serialize_parameter(key_ref("key2"), 11111, &mut buf);
    assert_eq!(";key2=11111", buf);
}

#[test]
fn serialize_key() {
    let mut buf = String::new();
    serializer::serialize_key(key_ref("*a_fg"), &mut buf);
    assert_eq!("*a_fg", &buf);

    buf.clear();
    serializer::serialize_key(key_ref("*a_fg*"), &mut buf);
    assert_eq!("*a_fg*", &buf);

    buf.clear();
    serializer::serialize_key(key_ref("key1"), &mut buf);
    assert_eq!("key1", &buf);

    buf.clear();
    serializer::serialize_key(key_ref("ke-y.1"), &mut buf);
    assert_eq!("ke-y.1", &buf);
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_list_of_items_and_inner_list() {
    let item1 = Item::new(12);
    let item2 = Item::new(14);
    let item3 = Item::new(token_ref("a"));
    let item4 = Item::new(token_ref("b"));
    let inner_list_param = Parameters::from_iter(vec![(
        key_ref("param").to_owned(),
        BareItem::String(string_ref("param_value_1").to_owned()),
    )]);
    let inner_list = InnerList::with_params(vec![item3, item4], inner_list_param);
    let input: List = vec![item1.into(), item2.into(), inner_list.into()];

    assert_eq!(
        Some(r#"12, 14, (a b);param="param_value_1""#),
        input.serialize().as_deref(),
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_list_of_lists() {
    let item1 = Item::new(1);
    let item2 = Item::new(2);
    let item3 = Item::new(42);
    let item4 = Item::new(43);
    let inner_list_1 = InnerList::new(vec![item1, item2]);
    let inner_list_2 = InnerList::new(vec![item3, item4]);
    let input: List = vec![inner_list_1.into(), inner_list_2.into()];

    assert_eq!(Some("(1 2), (42 43)"), input.serialize().as_deref());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_list_with_bool_item_and_bool_params() {
    let item1_params = Parameters::from_iter(vec![
        (key_ref("a").to_owned(), BareItem::Boolean(true)),
        (key_ref("b").to_owned(), BareItem::Boolean(false)),
    ]);
    let item1 = Item::with_params(false, item1_params);
    let item2 = Item::new(token_ref("cde_456"));

    let input: List = vec![item1.into(), item2.into()];
    assert_eq!(Some("?0;a;b=?0, cde_456"), input.serialize().as_deref());
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_dictionary_with_params() {
    let item1_params = Parameters::from_iter(vec![
        (key_ref("a").to_owned(), 1.into()),
        (key_ref("b").to_owned(), BareItem::Boolean(true)),
    ]);
    let item2_params = Parameters::new();
    let item3_params = Parameters::from_iter(vec![
        (key_ref("q").to_owned(), BareItem::Boolean(false)),
        (
            key_ref("r").to_owned(),
            BareItem::String(string_ref("+w").to_owned()),
        ),
    ]);

    let item1 = Item::with_params(123, item1_params);
    let item2 = Item::with_params(456, item2_params);
    let item3 = Item::with_params(789, item3_params);

    let input = Dictionary::from_iter(vec![
        (key_ref("abc").to_owned(), item1.into()),
        (key_ref("def").to_owned(), item2.into()),
        (key_ref("ghi").to_owned(), item3.into()),
    ]);

    assert_eq!(
        Some(r#"abc=123;a=1;b, def=456, ghi=789;q=?0;r="+w""#),
        input.serialize().as_deref(),
    );
}

#[test]
#[cfg(feature = "parsed-types")]
fn serialize_dict_empty_member_value() {
    let inner_list = InnerList::new(vec![]);
    let input = Dictionary::from_iter(vec![(key_ref("a").to_owned(), inner_list.into())]);
    assert_eq!(Some("a=()"), input.serialize().as_deref());
}

#[test]
fn serialize_date_with() {
    let mut buf = String::new();
    serializer::serialize_date(Date::UNIX_EPOCH, &mut buf);
    assert_eq!(buf, "@0");
}

#[test]
fn serialize_display_string() {
    let mut buf = String::new();
    serializer::serialize_display_string("üsers", &mut buf);
    assert_eq!(buf, r#"%"%c3%bcsers""#);
}
