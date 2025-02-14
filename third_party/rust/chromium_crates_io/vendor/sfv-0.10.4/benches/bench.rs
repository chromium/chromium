#[macro_use]
extern crate criterion;

use criterion::{BenchmarkId, Criterion};
use rust_decimal::prelude::FromPrimitive;
use sfv::{Decimal, Parser, SerializeValue};
use sfv::{RefBareItem, RefDictSerializer, RefItemSerializer, RefListSerializer};

criterion_main!(parsing, serializing, ref_serializing);

criterion_group!(parsing, parsing_item, parsing_list, parsing_dict);

fn parsing_item(c: &mut Criterion) {
    let fixture =
        "c29tZXZlcnlsb25nc3RyaW5ndmFsdWVyZXByZXNlbnRlZGFzYnl0ZXNhbnNvbWVvdGhlcmxvbmdsaW5l";
    c.bench_with_input(
        BenchmarkId::new("parsing_item", fixture),
        &fixture,
        move |bench, &input| {
            bench.iter(|| Parser::parse_item(input.as_bytes()).unwrap());
        },
    );
}

fn parsing_list(c: &mut Criterion) {
    let fixture = "a, abcdefghigklmnoprst, 123456785686457, 99999999999.999, (), (\"somelongstringvalue\" \"anotherlongstringvalue\";key=:c29tZXZlciBsb25nc3RyaW5ndmFsdWVyZXByZXNlbnRlZGFzYnl0ZXM: 145)";
    c.bench_with_input(
        BenchmarkId::new("parsing_list", fixture),
        &fixture,
        move |bench, &input| {
            bench.iter(|| Parser::parse_list(input.as_bytes()).unwrap());
        },
    );
}

fn parsing_dict(c: &mut Criterion) {
    let fixture = "a, dict_key2=abcdefghigklmnoprst, dict_key3=123456785686457, dict_key4=(\"inner-list-member\" :aW5uZXItbGlzdC1tZW1iZXI=:);key=aW5uZXItbGlzdC1wYXJhbWV0ZXJz";
    c.bench_with_input(
        BenchmarkId::new("parsing_dict", fixture),
        &fixture,
        move |bench, &input| {
            bench.iter(|| Parser::parse_dictionary(input.as_bytes()).unwrap());
        },
    );
}

criterion_group!(
    serializing,
    serializing_item,
    serializing_list,
    serializing_dict
);

fn serializing_item(c: &mut Criterion) {
    let fixture =
        "c29tZXZlcnlsb25nc3RyaW5ndmFsdWVyZXByZXNlbnRlZGFzYnl0ZXNhbnNvbWVvdGhlcmxvbmdsaW5l";
    c.bench_with_input(
        BenchmarkId::new("serializing_item", fixture),
        &fixture,
        move |bench, &input| {
            let parsed_item = Parser::parse_item(input.as_bytes()).unwrap();
            bench.iter(|| parsed_item.serialize_value().unwrap());
        },
    );
}

fn serializing_list(c: &mut Criterion) {
    let fixture = "a, abcdefghigklmnoprst, 123456785686457, 99999999999.999, (), (\"somelongstringvalue\" \"anotherlongstringvalue\";key=:c29tZXZlciBsb25nc3RyaW5ndmFsdWVyZXByZXNlbnRlZGFzYnl0ZXM: 145)";
    c.bench_with_input(
        BenchmarkId::new("serializing_list", fixture),
        &fixture,
        move |bench, &input| {
            let parsed_list = Parser::parse_list(input.as_bytes()).unwrap();
            bench.iter(|| parsed_list.serialize_value().unwrap());
        },
    );
}

fn serializing_dict(c: &mut Criterion) {
    let fixture = "a, dict_key2=abcdefghigklmnoprst, dict_key3=123456785686457, dict_key4=(\"inner-list-member\" :aW5uZXItbGlzdC1tZW1iZXI=:);key=aW5uZXItbGlzdC1wYXJhbWV0ZXJz";
    c.bench_with_input(
        BenchmarkId::new("serializing_dict", fixture),
        &fixture,
        move |bench, &input| {
            let parsed_dict = Parser::parse_dictionary(input.as_bytes()).unwrap();
            bench.iter(|| parsed_dict.serialize_value().unwrap());
        },
    );
}

criterion_group!(
    ref_serializing,
    serializing_ref_item,
    serializing_ref_list,
    serializing_ref_dict
);

fn serializing_ref_item(c: &mut Criterion) {
    let fixture =
        "c29tZXZlcnlsb25nc3RyaW5ndmFsdWVyZXByZXNlbnRlZGFzYnl0ZXNhbnNvbWVvdGhlcmxvbmdsaW5l";
    c.bench_with_input(
        BenchmarkId::new("serializing_ref_item", fixture),
        &fixture,
        move |bench, &input| {
            bench.iter(|| {
                let mut output = String::new();
                let ser = RefItemSerializer::new(&mut output);
                ser.bare_item(&RefBareItem::ByteSeq(input.as_bytes()))
                    .unwrap();
            });
        },
    );
}

fn serializing_ref_list(c: &mut Criterion) {
    c.bench_function("serializing_ref_list", move |bench| {
        bench.iter(|| {
            let mut output = String::new();
            let ser = RefListSerializer::new(&mut output);
            ser.bare_item(&RefBareItem::Token("a"))
                .unwrap()
                .bare_item(&RefBareItem::Token("abcdefghigklmnoprst"))
                .unwrap()
                .bare_item(&RefBareItem::Integer(123456785686457))
                .unwrap()
                .bare_item(&RefBareItem::Decimal(
                    Decimal::from_f64(99999999999.999).unwrap(),
                ))
                .unwrap()
                .open_inner_list()
                .close_inner_list()
                .open_inner_list()
                .inner_list_bare_item(&RefBareItem::String("somelongstringvalue"))
                .unwrap()
                .inner_list_bare_item(&RefBareItem::String("anotherlongstringvalue"))
                .unwrap()
                .inner_list_parameter(
                    "key",
                    &RefBareItem::ByteSeq("somever longstringvaluerepresentedasbytes".as_bytes()),
                )
                .unwrap()
                .inner_list_bare_item(&RefBareItem::Integer(145))
                .unwrap()
                .close_inner_list();
        });
    });
}

fn serializing_ref_dict(c: &mut Criterion) {
    c.bench_function("serializing_ref_dict", move |bench| {
        bench.iter(|| {
            let mut output = String::new();
            RefDictSerializer::new(&mut output)
                .bare_item_member("a", &RefBareItem::Boolean(true))
                .unwrap()
                .bare_item_member("dict_key2", &RefBareItem::Token("abcdefghigklmnoprst"))
                .unwrap()
                .bare_item_member("dict_key3", &RefBareItem::Integer(123456785686457))
                .unwrap()
                .open_inner_list("dict_key4")
                .unwrap()
                .inner_list_bare_item(&RefBareItem::String("inner-list-member"))
                .unwrap()
                .inner_list_bare_item(&RefBareItem::ByteSeq("inner-list-member".as_bytes()))
                .unwrap()
                .close_inner_list()
                .parameter("key", &RefBareItem::Token("aW5uZXItbGlzdC1wYXJhbWV0ZXJz"))
                .unwrap();
        });
    });
}
