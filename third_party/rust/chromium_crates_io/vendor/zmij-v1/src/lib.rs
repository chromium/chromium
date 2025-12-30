//! [![github]](https://github.com/dtolnay/zmij)&ensp;[![crates-io]](https://crates.io/crates/zmij)&ensp;[![docs-rs]](https://docs.rs/zmij)
//!
//! [github]: https://img.shields.io/badge/github-8da0cb?style=for-the-badge&labelColor=555555&logo=github
//! [crates-io]: https://img.shields.io/badge/crates.io-fc8d62?style=for-the-badge&labelColor=555555&logo=rust
//! [docs-rs]: https://img.shields.io/badge/docs.rs-66c2a5?style=for-the-badge&labelColor=555555&logo=docs.rs
//!
//! <br>
//!
//! A double-to-string conversion algorithm based on [Schubfach] and [yy].
//!
//! This Rust implementation is a line-by-line port of Victor Zverovich's
//! implementation in C++, <https://github.com/vitaut/zmij>.
//!
//! [Schubfach]: https://fmt.dev/papers/Schubfach4.pdf
//! [yy]: https://github.com/ibireme/c_numconv_benchmark/blob/master/vendor/yy_double/yy_double.c
//!
//! <br>
//!
//! # Example
//!
//! ```
//! fn main() {
//!     let mut buffer = zmij::Buffer::new();
//!     let printed = buffer.format(1.234);
//!     assert_eq!(printed, "1.234");
//! }
//! ```
//!
//! <br>
//!
//! ## Performance
//!
//! The [dtoa-benchmark] compares this library and other Rust floating point
//! formatting implementations across a range of precisions. The vertical axis
//! in this chart shows nanoseconds taken by a single execution of
//! `zmij::Buffer::new().format_finite(value)` so a lower result indicates a
//! faster library.
//!
//! [dtoa-benchmark]: https://github.com/dtolnay/dtoa-benchmark
//!
//! ![performance](https://raw.githubusercontent.com/dtolnay/zmij/master/dtoa-benchmark.png)

#![no_std]
#![doc(html_root_url = "https://docs.rs/zmij/1.0.2")]
#![deny(unsafe_op_in_unsafe_fn)]
#![allow(
    clippy::blocks_in_conditions,
    clippy::cast_possible_truncation,
    clippy::cast_possible_wrap,
    clippy::cast_sign_loss,
    clippy::doc_markdown,
    clippy::incompatible_msrv,
    clippy::items_after_statements,
    clippy::must_use_candidate,
    clippy::needless_doctest_main,
    clippy::redundant_else,
    clippy::similar_names,
    clippy::too_many_lines,
    clippy::unreadable_literal,
    clippy::wildcard_imports
)]

#[cfg(test)]
mod tests;
mod traits;

#[cfg(not(zmij_no_select_unpredictable))]
use core::hint;
use core::mem::{self, MaybeUninit};
use core::ptr;
use core::slice;
use core::str;
#[cfg(feature = "no-panic")]
use no_panic::no_panic;

const BUFFER_SIZE: usize = 24;
const NAN: &str = "NaN";
const INFINITY: &str = "inf";
const NEG_INFINITY: &str = "-inf";

#[allow(non_camel_case_types)]
struct uint128 {
    hi: u64,
    lo: u64,
}

// 128-bit significands of powers of 10 rounded down.
// Generated with gen-pow10/main.rs.
const DEC_EXP_MIN: i32 = -292;
#[rustfmt::skip]
static POW10_SIGNIFICANDS: [(u64, u64); 617] = [
    (0xff77b1fcbebcdc4f, 0x25e8e89c13bb0f7a), // -292
    (0x9faacf3df73609b1, 0x77b191618c54e9ac), // -291
    (0xc795830d75038c1d, 0xd59df5b9ef6a2417), // -290
    (0xf97ae3d0d2446f25, 0x4b0573286b44ad1d), // -289
    (0x9becce62836ac577, 0x4ee367f9430aec32), // -288
    (0xc2e801fb244576d5, 0x229c41f793cda73f), // -287
    (0xf3a20279ed56d48a, 0x6b43527578c1110f), // -286
    (0x9845418c345644d6, 0x830a13896b78aaa9), // -285
    (0xbe5691ef416bd60c, 0x23cc986bc656d553), // -284
    (0xedec366b11c6cb8f, 0x2cbfbe86b7ec8aa8), // -283
    (0x94b3a202eb1c3f39, 0x7bf7d71432f3d6a9), // -282
    (0xb9e08a83a5e34f07, 0xdaf5ccd93fb0cc53), // -281
    (0xe858ad248f5c22c9, 0xd1b3400f8f9cff68), // -280
    (0x91376c36d99995be, 0x23100809b9c21fa1), // -279
    (0xb58547448ffffb2d, 0xabd40a0c2832a78a), // -278
    (0xe2e69915b3fff9f9, 0x16c90c8f323f516c), // -277
    (0x8dd01fad907ffc3b, 0xae3da7d97f6792e3), // -276
    (0xb1442798f49ffb4a, 0x99cd11cfdf41779c), // -275
    (0xdd95317f31c7fa1d, 0x40405643d711d583), // -274
    (0x8a7d3eef7f1cfc52, 0x482835ea666b2572), // -273
    (0xad1c8eab5ee43b66, 0xda3243650005eecf), // -272
    (0xd863b256369d4a40, 0x90bed43e40076a82), // -271
    (0x873e4f75e2224e68, 0x5a7744a6e804a291), // -270
    (0xa90de3535aaae202, 0x711515d0a205cb36), // -269
    (0xd3515c2831559a83, 0x0d5a5b44ca873e03), // -268
    (0x8412d9991ed58091, 0xe858790afe9486c2), // -267
    (0xa5178fff668ae0b6, 0x626e974dbe39a872), // -266
    (0xce5d73ff402d98e3, 0xfb0a3d212dc8128f), // -265
    (0x80fa687f881c7f8e, 0x7ce66634bc9d0b99), // -264
    (0xa139029f6a239f72, 0x1c1fffc1ebc44e80), // -263
    (0xc987434744ac874e, 0xa327ffb266b56220), // -262
    (0xfbe9141915d7a922, 0x4bf1ff9f0062baa8), // -261
    (0x9d71ac8fada6c9b5, 0x6f773fc3603db4a9), // -260
    (0xc4ce17b399107c22, 0xcb550fb4384d21d3), // -259
    (0xf6019da07f549b2b, 0x7e2a53a146606a48), // -258
    (0x99c102844f94e0fb, 0x2eda7444cbfc426d), // -257
    (0xc0314325637a1939, 0xfa911155fefb5308), // -256
    (0xf03d93eebc589f88, 0x793555ab7eba27ca), // -255
    (0x96267c7535b763b5, 0x4bc1558b2f3458de), // -254
    (0xbbb01b9283253ca2, 0x9eb1aaedfb016f16), // -253
    (0xea9c227723ee8bcb, 0x465e15a979c1cadc), // -252
    (0x92a1958a7675175f, 0x0bfacd89ec191ec9), // -251
    (0xb749faed14125d36, 0xcef980ec671f667b), // -250
    (0xe51c79a85916f484, 0x82b7e12780e7401a), // -249
    (0x8f31cc0937ae58d2, 0xd1b2ecb8b0908810), // -248
    (0xb2fe3f0b8599ef07, 0x861fa7e6dcb4aa15), // -247
    (0xdfbdcece67006ac9, 0x67a791e093e1d49a), // -246
    (0x8bd6a141006042bd, 0xe0c8bb2c5c6d24e0), // -245
    (0xaecc49914078536d, 0x58fae9f773886e18), // -244
    (0xda7f5bf590966848, 0xaf39a475506a899e), // -243
    (0x888f99797a5e012d, 0x6d8406c952429603), // -242
    (0xaab37fd7d8f58178, 0xc8e5087ba6d33b83), // -241
    (0xd5605fcdcf32e1d6, 0xfb1e4a9a90880a64), // -240
    (0x855c3be0a17fcd26, 0x5cf2eea09a55067f), // -239
    (0xa6b34ad8c9dfc06f, 0xf42faa48c0ea481e), // -238
    (0xd0601d8efc57b08b, 0xf13b94daf124da26), // -237
    (0x823c12795db6ce57, 0x76c53d08d6b70858), // -236
    (0xa2cb1717b52481ed, 0x54768c4b0c64ca6e), // -235
    (0xcb7ddcdda26da268, 0xa9942f5dcf7dfd09), // -234
    (0xfe5d54150b090b02, 0xd3f93b35435d7c4c), // -233
    (0x9efa548d26e5a6e1, 0xc47bc5014a1a6daf), // -232
    (0xc6b8e9b0709f109a, 0x359ab6419ca1091b), // -231
    (0xf867241c8cc6d4c0, 0xc30163d203c94b62), // -230
    (0x9b407691d7fc44f8, 0x79e0de63425dcf1d), // -229
    (0xc21094364dfb5636, 0x985915fc12f542e4), // -228
    (0xf294b943e17a2bc4, 0x3e6f5b7b17b2939d), // -227
    (0x979cf3ca6cec5b5a, 0xa705992ceecf9c42), // -226
    (0xbd8430bd08277231, 0x50c6ff782a838353), // -225
    (0xece53cec4a314ebd, 0xa4f8bf5635246428), // -224
    (0x940f4613ae5ed136, 0x871b7795e136be99), // -223
    (0xb913179899f68584, 0x28e2557b59846e3f), // -222
    (0xe757dd7ec07426e5, 0x331aeada2fe589cf), // -221
    (0x9096ea6f3848984f, 0x3ff0d2c85def7621), // -220
    (0xb4bca50b065abe63, 0x0fed077a756b53a9), // -219
    (0xe1ebce4dc7f16dfb, 0xd3e8495912c62894), // -218
    (0x8d3360f09cf6e4bd, 0x64712dd7abbbd95c), // -217
    (0xb080392cc4349dec, 0xbd8d794d96aacfb3), // -216
    (0xdca04777f541c567, 0xecf0d7a0fc5583a0), // -215
    (0x89e42caaf9491b60, 0xf41686c49db57244), // -214
    (0xac5d37d5b79b6239, 0x311c2875c522ced5), // -213
    (0xd77485cb25823ac7, 0x7d633293366b828b), // -212
    (0x86a8d39ef77164bc, 0xae5dff9c02033197), // -211
    (0xa8530886b54dbdeb, 0xd9f57f830283fdfc), // -210
    (0xd267caa862a12d66, 0xd072df63c324fd7b), // -209
    (0x8380dea93da4bc60, 0x4247cb9e59f71e6d), // -208
    (0xa46116538d0deb78, 0x52d9be85f074e608), // -207
    (0xcd795be870516656, 0x67902e276c921f8b), // -206
    (0x806bd9714632dff6, 0x00ba1cd8a3db53b6), // -205
    (0xa086cfcd97bf97f3, 0x80e8a40eccd228a4), // -204
    (0xc8a883c0fdaf7df0, 0x6122cd128006b2cd), // -203
    (0xfad2a4b13d1b5d6c, 0x796b805720085f81), // -202
    (0x9cc3a6eec6311a63, 0xcbe3303674053bb0), // -201
    (0xc3f490aa77bd60fc, 0xbedbfc4411068a9c), // -200
    (0xf4f1b4d515acb93b, 0xee92fb5515482d44), // -199
    (0x991711052d8bf3c5, 0x751bdd152d4d1c4a), // -198
    (0xbf5cd54678eef0b6, 0xd262d45a78a0635d), // -197
    (0xef340a98172aace4, 0x86fb897116c87c34), // -196
    (0x9580869f0e7aac0e, 0xd45d35e6ae3d4da0), // -195
    (0xbae0a846d2195712, 0x8974836059cca109), // -194
    (0xe998d258869facd7, 0x2bd1a438703fc94b), // -193
    (0x91ff83775423cc06, 0x7b6306a34627ddcf), // -192
    (0xb67f6455292cbf08, 0x1a3bc84c17b1d542), // -191
    (0xe41f3d6a7377eeca, 0x20caba5f1d9e4a93), // -190
    (0x8e938662882af53e, 0x547eb47b7282ee9c), // -189
    (0xb23867fb2a35b28d, 0xe99e619a4f23aa43), // -188
    (0xdec681f9f4c31f31, 0x6405fa00e2ec94d4), // -187
    (0x8b3c113c38f9f37e, 0xde83bc408dd3dd04), // -186
    (0xae0b158b4738705e, 0x9624ab50b148d445), // -185
    (0xd98ddaee19068c76, 0x3badd624dd9b0957), // -184
    (0x87f8a8d4cfa417c9, 0xe54ca5d70a80e5d6), // -183
    (0xa9f6d30a038d1dbc, 0x5e9fcf4ccd211f4c), // -182
    (0xd47487cc8470652b, 0x7647c3200069671f), // -181
    (0x84c8d4dfd2c63f3b, 0x29ecd9f40041e073), // -180
    (0xa5fb0a17c777cf09, 0xf468107100525890), // -179
    (0xcf79cc9db955c2cc, 0x7182148d4066eeb4), // -178
    (0x81ac1fe293d599bf, 0xc6f14cd848405530), // -177
    (0xa21727db38cb002f, 0xb8ada00e5a506a7c), // -176
    (0xca9cf1d206fdc03b, 0xa6d90811f0e4851c), // -175
    (0xfd442e4688bd304a, 0x908f4a166d1da663), // -174
    (0x9e4a9cec15763e2e, 0x9a598e4e043287fe), // -173
    (0xc5dd44271ad3cdba, 0x40eff1e1853f29fd), // -172
    (0xf7549530e188c128, 0xd12bee59e68ef47c), // -171
    (0x9a94dd3e8cf578b9, 0x82bb74f8301958ce), // -170
    (0xc13a148e3032d6e7, 0xe36a52363c1faf01), // -169
    (0xf18899b1bc3f8ca1, 0xdc44e6c3cb279ac1), // -168
    (0x96f5600f15a7b7e5, 0x29ab103a5ef8c0b9), // -167
    (0xbcb2b812db11a5de, 0x7415d448f6b6f0e7), // -166
    (0xebdf661791d60f56, 0x111b495b3464ad21), // -165
    (0x936b9fcebb25c995, 0xcab10dd900beec34), // -164
    (0xb84687c269ef3bfb, 0x3d5d514f40eea742), // -163
    (0xe65829b3046b0afa, 0x0cb4a5a3112a5112), // -162
    (0x8ff71a0fe2c2e6dc, 0x47f0e785eaba72ab), // -161
    (0xb3f4e093db73a093, 0x59ed216765690f56), // -160
    (0xe0f218b8d25088b8, 0x306869c13ec3532c), // -159
    (0x8c974f7383725573, 0x1e414218c73a13fb), // -158
    (0xafbd2350644eeacf, 0xe5d1929ef90898fa), // -157
    (0xdbac6c247d62a583, 0xdf45f746b74abf39), // -156
    (0x894bc396ce5da772, 0x6b8bba8c328eb783), // -155
    (0xab9eb47c81f5114f, 0x066ea92f3f326564), // -154
    (0xd686619ba27255a2, 0xc80a537b0efefebd), // -153
    (0x8613fd0145877585, 0xbd06742ce95f5f36), // -152
    (0xa798fc4196e952e7, 0x2c48113823b73704), // -151
    (0xd17f3b51fca3a7a0, 0xf75a15862ca504c5), // -150
    (0x82ef85133de648c4, 0x9a984d73dbe722fb), // -149
    (0xa3ab66580d5fdaf5, 0xc13e60d0d2e0ebba), // -148
    (0xcc963fee10b7d1b3, 0x318df905079926a8), // -147
    (0xffbbcfe994e5c61f, 0xfdf17746497f7052), // -146
    (0x9fd561f1fd0f9bd3, 0xfeb6ea8bedefa633), // -145
    (0xc7caba6e7c5382c8, 0xfe64a52ee96b8fc0), // -144
    (0xf9bd690a1b68637b, 0x3dfdce7aa3c673b0), // -143
    (0x9c1661a651213e2d, 0x06bea10ca65c084e), // -142
    (0xc31bfa0fe5698db8, 0x486e494fcff30a62), // -141
    (0xf3e2f893dec3f126, 0x5a89dba3c3efccfa), // -140
    (0x986ddb5c6b3a76b7, 0xf89629465a75e01c), // -139
    (0xbe89523386091465, 0xf6bbb397f1135823), // -138
    (0xee2ba6c0678b597f, 0x746aa07ded582e2c), // -137
    (0x94db483840b717ef, 0xa8c2a44eb4571cdc), // -136
    (0xba121a4650e4ddeb, 0x92f34d62616ce413), // -135
    (0xe896a0d7e51e1566, 0x77b020baf9c81d17), // -134
    (0x915e2486ef32cd60, 0x0ace1474dc1d122e), // -133
    (0xb5b5ada8aaff80b8, 0x0d819992132456ba), // -132
    (0xe3231912d5bf60e6, 0x10e1fff697ed6c69), // -131
    (0x8df5efabc5979c8f, 0xca8d3ffa1ef463c1), // -130
    (0xb1736b96b6fd83b3, 0xbd308ff8a6b17cb2), // -129
    (0xddd0467c64bce4a0, 0xac7cb3f6d05ddbde), // -128
    (0x8aa22c0dbef60ee4, 0x6bcdf07a423aa96b), // -127
    (0xad4ab7112eb3929d, 0x86c16c98d2c953c6), // -126
    (0xd89d64d57a607744, 0xe871c7bf077ba8b7), // -125
    (0x87625f056c7c4a8b, 0x11471cd764ad4972), // -124
    (0xa93af6c6c79b5d2d, 0xd598e40d3dd89bcf), // -123
    (0xd389b47879823479, 0x4aff1d108d4ec2c3), // -122
    (0x843610cb4bf160cb, 0xcedf722a585139ba), // -121
    (0xa54394fe1eedb8fe, 0xc2974eb4ee658828), // -120
    (0xce947a3da6a9273e, 0x733d226229feea32), // -119
    (0x811ccc668829b887, 0x0806357d5a3f525f), // -118
    (0xa163ff802a3426a8, 0xca07c2dcb0cf26f7), // -117
    (0xc9bcff6034c13052, 0xfc89b393dd02f0b5), // -116
    (0xfc2c3f3841f17c67, 0xbbac2078d443ace2), // -115
    (0x9d9ba7832936edc0, 0xd54b944b84aa4c0d), // -114
    (0xc5029163f384a931, 0x0a9e795e65d4df11), // -113
    (0xf64335bcf065d37d, 0x4d4617b5ff4a16d5), // -112
    (0x99ea0196163fa42e, 0x504bced1bf8e4e45), // -111
    (0xc06481fb9bcf8d39, 0xe45ec2862f71e1d6), // -110
    (0xf07da27a82c37088, 0x5d767327bb4e5a4c), // -109
    (0x964e858c91ba2655, 0x3a6a07f8d510f86f), // -108
    (0xbbe226efb628afea, 0x890489f70a55368b), // -107
    (0xeadab0aba3b2dbe5, 0x2b45ac74ccea842e), // -106
    (0x92c8ae6b464fc96f, 0x3b0b8bc90012929d), // -105
    (0xb77ada0617e3bbcb, 0x09ce6ebb40173744), // -104
    (0xe55990879ddcaabd, 0xcc420a6a101d0515), // -103
    (0x8f57fa54c2a9eab6, 0x9fa946824a12232d), // -102
    (0xb32df8e9f3546564, 0x47939822dc96abf9), // -101
    (0xdff9772470297ebd, 0x59787e2b93bc56f7), // -100
    (0x8bfbea76c619ef36, 0x57eb4edb3c55b65a), //  -99
    (0xaefae51477a06b03, 0xede622920b6b23f1), //  -98
    (0xdab99e59958885c4, 0xe95fab368e45eced), //  -97
    (0x88b402f7fd75539b, 0x11dbcb0218ebb414), //  -96
    (0xaae103b5fcd2a881, 0xd652bdc29f26a119), //  -95
    (0xd59944a37c0752a2, 0x4be76d3346f0495f), //  -94
    (0x857fcae62d8493a5, 0x6f70a4400c562ddb), //  -93
    (0xa6dfbd9fb8e5b88e, 0xcb4ccd500f6bb952), //  -92
    (0xd097ad07a71f26b2, 0x7e2000a41346a7a7), //  -91
    (0x825ecc24c873782f, 0x8ed400668c0c28c8), //  -90
    (0xa2f67f2dfa90563b, 0x728900802f0f32fa), //  -89
    (0xcbb41ef979346bca, 0x4f2b40a03ad2ffb9), //  -88
    (0xfea126b7d78186bc, 0xe2f610c84987bfa8), //  -87
    (0x9f24b832e6b0f436, 0x0dd9ca7d2df4d7c9), //  -86
    (0xc6ede63fa05d3143, 0x91503d1c79720dbb), //  -85
    (0xf8a95fcf88747d94, 0x75a44c6397ce912a), //  -84
    (0x9b69dbe1b548ce7c, 0xc986afbe3ee11aba), //  -83
    (0xc24452da229b021b, 0xfbe85badce996168), //  -82
    (0xf2d56790ab41c2a2, 0xfae27299423fb9c3), //  -81
    (0x97c560ba6b0919a5, 0xdccd879fc967d41a), //  -80
    (0xbdb6b8e905cb600f, 0x5400e987bbc1c920), //  -79
    (0xed246723473e3813, 0x290123e9aab23b68), //  -78
    (0x9436c0760c86e30b, 0xf9a0b6720aaf6521), //  -77
    (0xb94470938fa89bce, 0xf808e40e8d5b3e69), //  -76
    (0xe7958cb87392c2c2, 0xb60b1d1230b20e04), //  -75
    (0x90bd77f3483bb9b9, 0xb1c6f22b5e6f48c2), //  -74
    (0xb4ecd5f01a4aa828, 0x1e38aeb6360b1af3), //  -73
    (0xe2280b6c20dd5232, 0x25c6da63c38de1b0), //  -72
    (0x8d590723948a535f, 0x579c487e5a38ad0e), //  -71
    (0xb0af48ec79ace837, 0x2d835a9df0c6d851), //  -70
    (0xdcdb1b2798182244, 0xf8e431456cf88e65), //  -69
    (0x8a08f0f8bf0f156b, 0x1b8e9ecb641b58ff), //  -68
    (0xac8b2d36eed2dac5, 0xe272467e3d222f3f), //  -67
    (0xd7adf884aa879177, 0x5b0ed81dcc6abb0f), //  -66
    (0x86ccbb52ea94baea, 0x98e947129fc2b4e9), //  -65
    (0xa87fea27a539e9a5, 0x3f2398d747b36224), //  -64
    (0xd29fe4b18e88640e, 0x8eec7f0d19a03aad), //  -63
    (0x83a3eeeef9153e89, 0x1953cf68300424ac), //  -62
    (0xa48ceaaab75a8e2b, 0x5fa8c3423c052dd7), //  -61
    (0xcdb02555653131b6, 0x3792f412cb06794d), //  -60
    (0x808e17555f3ebf11, 0xe2bbd88bbee40bd0), //  -59
    (0xa0b19d2ab70e6ed6, 0x5b6aceaeae9d0ec4), //  -58
    (0xc8de047564d20a8b, 0xf245825a5a445275), //  -57
    (0xfb158592be068d2e, 0xeed6e2f0f0d56712), //  -56
    (0x9ced737bb6c4183d, 0x55464dd69685606b), //  -55
    (0xc428d05aa4751e4c, 0xaa97e14c3c26b886), //  -54
    (0xf53304714d9265df, 0xd53dd99f4b3066a8), //  -53
    (0x993fe2c6d07b7fab, 0xe546a8038efe4029), //  -52
    (0xbf8fdb78849a5f96, 0xde98520472bdd033), //  -51
    (0xef73d256a5c0f77c, 0x963e66858f6d4440), //  -50
    (0x95a8637627989aad, 0xdde7001379a44aa8), //  -49
    (0xbb127c53b17ec159, 0x5560c018580d5d52), //  -48
    (0xe9d71b689dde71af, 0xaab8f01e6e10b4a6), //  -47
    (0x9226712162ab070d, 0xcab3961304ca70e8), //  -46
    (0xb6b00d69bb55c8d1, 0x3d607b97c5fd0d22), //  -45
    (0xe45c10c42a2b3b05, 0x8cb89a7db77c506a), //  -44
    (0x8eb98a7a9a5b04e3, 0x77f3608e92adb242), //  -43
    (0xb267ed1940f1c61c, 0x55f038b237591ed3), //  -42
    (0xdf01e85f912e37a3, 0x6b6c46dec52f6688), //  -41
    (0x8b61313bbabce2c6, 0x2323ac4b3b3da015), //  -40
    (0xae397d8aa96c1b77, 0xabec975e0a0d081a), //  -39
    (0xd9c7dced53c72255, 0x96e7bd358c904a21), //  -38
    (0x881cea14545c7575, 0x7e50d64177da2e54), //  -37
    (0xaa242499697392d2, 0xdde50bd1d5d0b9e9), //  -36
    (0xd4ad2dbfc3d07787, 0x955e4ec64b44e864), //  -35
    (0x84ec3c97da624ab4, 0xbd5af13bef0b113e), //  -34
    (0xa6274bbdd0fadd61, 0xecb1ad8aeacdd58e), //  -33
    (0xcfb11ead453994ba, 0x67de18eda5814af2), //  -32
    (0x81ceb32c4b43fcf4, 0x80eacf948770ced7), //  -31
    (0xa2425ff75e14fc31, 0xa1258379a94d028d), //  -30
    (0xcad2f7f5359a3b3e, 0x096ee45813a04330), //  -29
    (0xfd87b5f28300ca0d, 0x8bca9d6e188853fc), //  -28
    (0x9e74d1b791e07e48, 0x775ea264cf55347d), //  -27
    (0xc612062576589dda, 0x95364afe032a819d), //  -26
    (0xf79687aed3eec551, 0x3a83ddbd83f52204), //  -25
    (0x9abe14cd44753b52, 0xc4926a9672793542), //  -24
    (0xc16d9a0095928a27, 0x75b7053c0f178293), //  -23
    (0xf1c90080baf72cb1, 0x5324c68b12dd6338), //  -22
    (0x971da05074da7bee, 0xd3f6fc16ebca5e03), //  -21
    (0xbce5086492111aea, 0x88f4bb1ca6bcf584), //  -20
    (0xec1e4a7db69561a5, 0x2b31e9e3d06c32e5), //  -19
    (0x9392ee8e921d5d07, 0x3aff322e62439fcf), //  -18
    (0xb877aa3236a4b449, 0x09befeb9fad487c2), //  -17
    (0xe69594bec44de15b, 0x4c2ebe687989a9b3), //  -16
    (0x901d7cf73ab0acd9, 0x0f9d37014bf60a10), //  -15
    (0xb424dc35095cd80f, 0x538484c19ef38c94), //  -14
    (0xe12e13424bb40e13, 0x2865a5f206b06fb9), //  -13
    (0x8cbccc096f5088cb, 0xf93f87b7442e45d3), //  -12
    (0xafebff0bcb24aafe, 0xf78f69a51539d748), //  -11
    (0xdbe6fecebdedd5be, 0xb573440e5a884d1b), //  -10
    (0x89705f4136b4a597, 0x31680a88f8953030), //   -9
    (0xabcc77118461cefc, 0xfdc20d2b36ba7c3d), //   -8
    (0xd6bf94d5e57a42bc, 0x3d32907604691b4c), //   -7
    (0x8637bd05af6c69b5, 0xa63f9a49c2c1b10f), //   -6
    (0xa7c5ac471b478423, 0x0fcf80dc33721d53), //   -5
    (0xd1b71758e219652b, 0xd3c36113404ea4a8), //   -4
    (0x83126e978d4fdf3b, 0x645a1cac083126e9), //   -3
    (0xa3d70a3d70a3d70a, 0x3d70a3d70a3d70a3), //   -2
    (0xcccccccccccccccc, 0xcccccccccccccccc), //   -1
    (0x8000000000000000, 0x0000000000000000), //    0
    (0xa000000000000000, 0x0000000000000000), //    1
    (0xc800000000000000, 0x0000000000000000), //    2
    (0xfa00000000000000, 0x0000000000000000), //    3
    (0x9c40000000000000, 0x0000000000000000), //    4
    (0xc350000000000000, 0x0000000000000000), //    5
    (0xf424000000000000, 0x0000000000000000), //    6
    (0x9896800000000000, 0x0000000000000000), //    7
    (0xbebc200000000000, 0x0000000000000000), //    8
    (0xee6b280000000000, 0x0000000000000000), //    9
    (0x9502f90000000000, 0x0000000000000000), //   10
    (0xba43b74000000000, 0x0000000000000000), //   11
    (0xe8d4a51000000000, 0x0000000000000000), //   12
    (0x9184e72a00000000, 0x0000000000000000), //   13
    (0xb5e620f480000000, 0x0000000000000000), //   14
    (0xe35fa931a0000000, 0x0000000000000000), //   15
    (0x8e1bc9bf04000000, 0x0000000000000000), //   16
    (0xb1a2bc2ec5000000, 0x0000000000000000), //   17
    (0xde0b6b3a76400000, 0x0000000000000000), //   18
    (0x8ac7230489e80000, 0x0000000000000000), //   19
    (0xad78ebc5ac620000, 0x0000000000000000), //   20
    (0xd8d726b7177a8000, 0x0000000000000000), //   21
    (0x878678326eac9000, 0x0000000000000000), //   22
    (0xa968163f0a57b400, 0x0000000000000000), //   23
    (0xd3c21bcecceda100, 0x0000000000000000), //   24
    (0x84595161401484a0, 0x0000000000000000), //   25
    (0xa56fa5b99019a5c8, 0x0000000000000000), //   26
    (0xcecb8f27f4200f3a, 0x0000000000000000), //   27
    (0x813f3978f8940984, 0x4000000000000000), //   28
    (0xa18f07d736b90be5, 0x5000000000000000), //   29
    (0xc9f2c9cd04674ede, 0xa400000000000000), //   30
    (0xfc6f7c4045812296, 0x4d00000000000000), //   31
    (0x9dc5ada82b70b59d, 0xf020000000000000), //   32
    (0xc5371912364ce305, 0x6c28000000000000), //   33
    (0xf684df56c3e01bc6, 0xc732000000000000), //   34
    (0x9a130b963a6c115c, 0x3c7f400000000000), //   35
    (0xc097ce7bc90715b3, 0x4b9f100000000000), //   36
    (0xf0bdc21abb48db20, 0x1e86d40000000000), //   37
    (0x96769950b50d88f4, 0x1314448000000000), //   38
    (0xbc143fa4e250eb31, 0x17d955a000000000), //   39
    (0xeb194f8e1ae525fd, 0x5dcfab0800000000), //   40
    (0x92efd1b8d0cf37be, 0x5aa1cae500000000), //   41
    (0xb7abc627050305ad, 0xf14a3d9e40000000), //   42
    (0xe596b7b0c643c719, 0x6d9ccd05d0000000), //   43
    (0x8f7e32ce7bea5c6f, 0xe4820023a2000000), //   44
    (0xb35dbf821ae4f38b, 0xdda2802c8a800000), //   45
    (0xe0352f62a19e306e, 0xd50b2037ad200000), //   46
    (0x8c213d9da502de45, 0x4526f422cc340000), //   47
    (0xaf298d050e4395d6, 0x9670b12b7f410000), //   48
    (0xdaf3f04651d47b4c, 0x3c0cdd765f114000), //   49
    (0x88d8762bf324cd0f, 0xa5880a69fb6ac800), //   50
    (0xab0e93b6efee0053, 0x8eea0d047a457a00), //   51
    (0xd5d238a4abe98068, 0x72a4904598d6d880), //   52
    (0x85a36366eb71f041, 0x47a6da2b7f864750), //   53
    (0xa70c3c40a64e6c51, 0x999090b65f67d924), //   54
    (0xd0cf4b50cfe20765, 0xfff4b4e3f741cf6d), //   55
    (0x82818f1281ed449f, 0xbff8f10e7a8921a4), //   56
    (0xa321f2d7226895c7, 0xaff72d52192b6a0d), //   57
    (0xcbea6f8ceb02bb39, 0x9bf4f8a69f764490), //   58
    (0xfee50b7025c36a08, 0x02f236d04753d5b4), //   59
    (0x9f4f2726179a2245, 0x01d762422c946590), //   60
    (0xc722f0ef9d80aad6, 0x424d3ad2b7b97ef5), //   61
    (0xf8ebad2b84e0d58b, 0xd2e0898765a7deb2), //   62
    (0x9b934c3b330c8577, 0x63cc55f49f88eb2f), //   63
    (0xc2781f49ffcfa6d5, 0x3cbf6b71c76b25fb), //   64
    (0xf316271c7fc3908a, 0x8bef464e3945ef7a), //   65
    (0x97edd871cfda3a56, 0x97758bf0e3cbb5ac), //   66
    (0xbde94e8e43d0c8ec, 0x3d52eeed1cbea317), //   67
    (0xed63a231d4c4fb27, 0x4ca7aaa863ee4bdd), //   68
    (0x945e455f24fb1cf8, 0x8fe8caa93e74ef6a), //   69
    (0xb975d6b6ee39e436, 0xb3e2fd538e122b44), //   70
    (0xe7d34c64a9c85d44, 0x60dbbca87196b616), //   71
    (0x90e40fbeea1d3a4a, 0xbc8955e946fe31cd), //   72
    (0xb51d13aea4a488dd, 0x6babab6398bdbe41), //   73
    (0xe264589a4dcdab14, 0xc696963c7eed2dd1), //   74
    (0x8d7eb76070a08aec, 0xfc1e1de5cf543ca2), //   75
    (0xb0de65388cc8ada8, 0x3b25a55f43294bcb), //   76
    (0xdd15fe86affad912, 0x49ef0eb713f39ebe), //   77
    (0x8a2dbf142dfcc7ab, 0x6e3569326c784337), //   78
    (0xacb92ed9397bf996, 0x49c2c37f07965404), //   79
    (0xd7e77a8f87daf7fb, 0xdc33745ec97be906), //   80
    (0x86f0ac99b4e8dafd, 0x69a028bb3ded71a3), //   81
    (0xa8acd7c0222311bc, 0xc40832ea0d68ce0c), //   82
    (0xd2d80db02aabd62b, 0xf50a3fa490c30190), //   83
    (0x83c7088e1aab65db, 0x792667c6da79e0fa), //   84
    (0xa4b8cab1a1563f52, 0x577001b891185938), //   85
    (0xcde6fd5e09abcf26, 0xed4c0226b55e6f86), //   86
    (0x80b05e5ac60b6178, 0x544f8158315b05b4), //   87
    (0xa0dc75f1778e39d6, 0x696361ae3db1c721), //   88
    (0xc913936dd571c84c, 0x03bc3a19cd1e38e9), //   89
    (0xfb5878494ace3a5f, 0x04ab48a04065c723), //   90
    (0x9d174b2dcec0e47b, 0x62eb0d64283f9c76), //   91
    (0xc45d1df942711d9a, 0x3ba5d0bd324f8394), //   92
    (0xf5746577930d6500, 0xca8f44ec7ee36479), //   93
    (0x9968bf6abbe85f20, 0x7e998b13cf4e1ecb), //   94
    (0xbfc2ef456ae276e8, 0x9e3fedd8c321a67e), //   95
    (0xefb3ab16c59b14a2, 0xc5cfe94ef3ea101e), //   96
    (0x95d04aee3b80ece5, 0xbba1f1d158724a12), //   97
    (0xbb445da9ca61281f, 0x2a8a6e45ae8edc97), //   98
    (0xea1575143cf97226, 0xf52d09d71a3293bd), //   99
    (0x924d692ca61be758, 0x593c2626705f9c56), //  100
    (0xb6e0c377cfa2e12e, 0x6f8b2fb00c77836c), //  101
    (0xe498f455c38b997a, 0x0b6dfb9c0f956447), //  102
    (0x8edf98b59a373fec, 0x4724bd4189bd5eac), //  103
    (0xb2977ee300c50fe7, 0x58edec91ec2cb657), //  104
    (0xdf3d5e9bc0f653e1, 0x2f2967b66737e3ed), //  105
    (0x8b865b215899f46c, 0xbd79e0d20082ee74), //  106
    (0xae67f1e9aec07187, 0xecd8590680a3aa11), //  107
    (0xda01ee641a708de9, 0xe80e6f4820cc9495), //  108
    (0x884134fe908658b2, 0x3109058d147fdcdd), //  109
    (0xaa51823e34a7eede, 0xbd4b46f0599fd415), //  110
    (0xd4e5e2cdc1d1ea96, 0x6c9e18ac7007c91a), //  111
    (0x850fadc09923329e, 0x03e2cf6bc604ddb0), //  112
    (0xa6539930bf6bff45, 0x84db8346b786151c), //  113
    (0xcfe87f7cef46ff16, 0xe612641865679a63), //  114
    (0x81f14fae158c5f6e, 0x4fcb7e8f3f60c07e), //  115
    (0xa26da3999aef7749, 0xe3be5e330f38f09d), //  116
    (0xcb090c8001ab551c, 0x5cadf5bfd3072cc5), //  117
    (0xfdcb4fa002162a63, 0x73d9732fc7c8f7f6), //  118
    (0x9e9f11c4014dda7e, 0x2867e7fddcdd9afa), //  119
    (0xc646d63501a1511d, 0xb281e1fd541501b8), //  120
    (0xf7d88bc24209a565, 0x1f225a7ca91a4226), //  121
    (0x9ae757596946075f, 0x3375788de9b06958), //  122
    (0xc1a12d2fc3978937, 0x0052d6b1641c83ae), //  123
    (0xf209787bb47d6b84, 0xc0678c5dbd23a49a), //  124
    (0x9745eb4d50ce6332, 0xf840b7ba963646e0), //  125
    (0xbd176620a501fbff, 0xb650e5a93bc3d898), //  126
    (0xec5d3fa8ce427aff, 0xa3e51f138ab4cebe), //  127
    (0x93ba47c980e98cdf, 0xc66f336c36b10137), //  128
    (0xb8a8d9bbe123f017, 0xb80b0047445d4184), //  129
    (0xe6d3102ad96cec1d, 0xa60dc059157491e5), //  130
    (0x9043ea1ac7e41392, 0x87c89837ad68db2f), //  131
    (0xb454e4a179dd1877, 0x29babe4598c311fb), //  132
    (0xe16a1dc9d8545e94, 0xf4296dd6fef3d67a), //  133
    (0x8ce2529e2734bb1d, 0x1899e4a65f58660c), //  134
    (0xb01ae745b101e9e4, 0x5ec05dcff72e7f8f), //  135
    (0xdc21a1171d42645d, 0x76707543f4fa1f73), //  136
    (0x899504ae72497eba, 0x6a06494a791c53a8), //  137
    (0xabfa45da0edbde69, 0x0487db9d17636892), //  138
    (0xd6f8d7509292d603, 0x45a9d2845d3c42b6), //  139
    (0x865b86925b9bc5c2, 0x0b8a2392ba45a9b2), //  140
    (0xa7f26836f282b732, 0x8e6cac7768d7141e), //  141
    (0xd1ef0244af2364ff, 0x3207d795430cd926), //  142
    (0x8335616aed761f1f, 0x7f44e6bd49e807b8), //  143
    (0xa402b9c5a8d3a6e7, 0x5f16206c9c6209a6), //  144
    (0xcd036837130890a1, 0x36dba887c37a8c0f), //  145
    (0x802221226be55a64, 0xc2494954da2c9789), //  146
    (0xa02aa96b06deb0fd, 0xf2db9baa10b7bd6c), //  147
    (0xc83553c5c8965d3d, 0x6f92829494e5acc7), //  148
    (0xfa42a8b73abbf48c, 0xcb772339ba1f17f9), //  149
    (0x9c69a97284b578d7, 0xff2a760414536efb), //  150
    (0xc38413cf25e2d70d, 0xfef5138519684aba), //  151
    (0xf46518c2ef5b8cd1, 0x7eb258665fc25d69), //  152
    (0x98bf2f79d5993802, 0xef2f773ffbd97a61), //  153
    (0xbeeefb584aff8603, 0xaafb550ffacfd8fa), //  154
    (0xeeaaba2e5dbf6784, 0x95ba2a53f983cf38), //  155
    (0x952ab45cfa97a0b2, 0xdd945a747bf26183), //  156
    (0xba756174393d88df, 0x94f971119aeef9e4), //  157
    (0xe912b9d1478ceb17, 0x7a37cd5601aab85d), //  158
    (0x91abb422ccb812ee, 0xac62e055c10ab33a), //  159
    (0xb616a12b7fe617aa, 0x577b986b314d6009), //  160
    (0xe39c49765fdf9d94, 0xed5a7e85fda0b80b), //  161
    (0x8e41ade9fbebc27d, 0x14588f13be847307), //  162
    (0xb1d219647ae6b31c, 0x596eb2d8ae258fc8), //  163
    (0xde469fbd99a05fe3, 0x6fca5f8ed9aef3bb), //  164
    (0x8aec23d680043bee, 0x25de7bb9480d5854), //  165
    (0xada72ccc20054ae9, 0xaf561aa79a10ae6a), //  166
    (0xd910f7ff28069da4, 0x1b2ba1518094da04), //  167
    (0x87aa9aff79042286, 0x90fb44d2f05d0842), //  168
    (0xa99541bf57452b28, 0x353a1607ac744a53), //  169
    (0xd3fa922f2d1675f2, 0x42889b8997915ce8), //  170
    (0x847c9b5d7c2e09b7, 0x69956135febada11), //  171
    (0xa59bc234db398c25, 0x43fab9837e699095), //  172
    (0xcf02b2c21207ef2e, 0x94f967e45e03f4bb), //  173
    (0x8161afb94b44f57d, 0x1d1be0eebac278f5), //  174
    (0xa1ba1ba79e1632dc, 0x6462d92a69731732), //  175
    (0xca28a291859bbf93, 0x7d7b8f7503cfdcfe), //  176
    (0xfcb2cb35e702af78, 0x5cda735244c3d43e), //  177
    (0x9defbf01b061adab, 0x3a0888136afa64a7), //  178
    (0xc56baec21c7a1916, 0x088aaa1845b8fdd0), //  179
    (0xf6c69a72a3989f5b, 0x8aad549e57273d45), //  180
    (0x9a3c2087a63f6399, 0x36ac54e2f678864b), //  181
    (0xc0cb28a98fcf3c7f, 0x84576a1bb416a7dd), //  182
    (0xf0fdf2d3f3c30b9f, 0x656d44a2a11c51d5), //  183
    (0x969eb7c47859e743, 0x9f644ae5a4b1b325), //  184
    (0xbc4665b596706114, 0x873d5d9f0dde1fee), //  185
    (0xeb57ff22fc0c7959, 0xa90cb506d155a7ea), //  186
    (0x9316ff75dd87cbd8, 0x09a7f12442d588f2), //  187
    (0xb7dcbf5354e9bece, 0x0c11ed6d538aeb2f), //  188
    (0xe5d3ef282a242e81, 0x8f1668c8a86da5fa), //  189
    (0x8fa475791a569d10, 0xf96e017d694487bc), //  190
    (0xb38d92d760ec4455, 0x37c981dcc395a9ac), //  191
    (0xe070f78d3927556a, 0x85bbe253f47b1417), //  192
    (0x8c469ab843b89562, 0x93956d7478ccec8e), //  193
    (0xaf58416654a6babb, 0x387ac8d1970027b2), //  194
    (0xdb2e51bfe9d0696a, 0x06997b05fcc0319e), //  195
    (0x88fcf317f22241e2, 0x441fece3bdf81f03), //  196
    (0xab3c2fddeeaad25a, 0xd527e81cad7626c3), //  197
    (0xd60b3bd56a5586f1, 0x8a71e223d8d3b074), //  198
    (0x85c7056562757456, 0xf6872d5667844e49), //  199
    (0xa738c6bebb12d16c, 0xb428f8ac016561db), //  200
    (0xd106f86e69d785c7, 0xe13336d701beba52), //  201
    (0x82a45b450226b39c, 0xecc0024661173473), //  202
    (0xa34d721642b06084, 0x27f002d7f95d0190), //  203
    (0xcc20ce9bd35c78a5, 0x31ec038df7b441f4), //  204
    (0xff290242c83396ce, 0x7e67047175a15271), //  205
    (0x9f79a169bd203e41, 0x0f0062c6e984d386), //  206
    (0xc75809c42c684dd1, 0x52c07b78a3e60868), //  207
    (0xf92e0c3537826145, 0xa7709a56ccdf8a82), //  208
    (0x9bbcc7a142b17ccb, 0x88a66076400bb691), //  209
    (0xc2abf989935ddbfe, 0x6acff893d00ea435), //  210
    (0xf356f7ebf83552fe, 0x0583f6b8c4124d43), //  211
    (0x98165af37b2153de, 0xc3727a337a8b704a), //  212
    (0xbe1bf1b059e9a8d6, 0x744f18c0592e4c5c), //  213
    (0xeda2ee1c7064130c, 0x1162def06f79df73), //  214
    (0x9485d4d1c63e8be7, 0x8addcb5645ac2ba8), //  215
    (0xb9a74a0637ce2ee1, 0x6d953e2bd7173692), //  216
    (0xe8111c87c5c1ba99, 0xc8fa8db6ccdd0437), //  217
    (0x910ab1d4db9914a0, 0x1d9c9892400a22a2), //  218
    (0xb54d5e4a127f59c8, 0x2503beb6d00cab4b), //  219
    (0xe2a0b5dc971f303a, 0x2e44ae64840fd61d), //  220
    (0x8da471a9de737e24, 0x5ceaecfed289e5d2), //  221
    (0xb10d8e1456105dad, 0x7425a83e872c5f47), //  222
    (0xdd50f1996b947518, 0xd12f124e28f77719), //  223
    (0x8a5296ffe33cc92f, 0x82bd6b70d99aaa6f), //  224
    (0xace73cbfdc0bfb7b, 0x636cc64d1001550b), //  225
    (0xd8210befd30efa5a, 0x3c47f7e05401aa4e), //  226
    (0x8714a775e3e95c78, 0x65acfaec34810a71), //  227
    (0xa8d9d1535ce3b396, 0x7f1839a741a14d0d), //  228
    (0xd31045a8341ca07c, 0x1ede48111209a050), //  229
    (0x83ea2b892091e44d, 0x934aed0aab460432), //  230
    (0xa4e4b66b68b65d60, 0xf81da84d5617853f), //  231
    (0xce1de40642e3f4b9, 0x36251260ab9d668e), //  232
    (0x80d2ae83e9ce78f3, 0xc1d72b7c6b426019), //  233
    (0xa1075a24e4421730, 0xb24cf65b8612f81f), //  234
    (0xc94930ae1d529cfc, 0xdee033f26797b627), //  235
    (0xfb9b7cd9a4a7443c, 0x169840ef017da3b1), //  236
    (0x9d412e0806e88aa5, 0x8e1f289560ee864e), //  237
    (0xc491798a08a2ad4e, 0xf1a6f2bab92a27e2), //  238
    (0xf5b5d7ec8acb58a2, 0xae10af696774b1db), //  239
    (0x9991a6f3d6bf1765, 0xacca6da1e0a8ef29), //  240
    (0xbff610b0cc6edd3f, 0x17fd090a58d32af3), //  241
    (0xeff394dcff8a948e, 0xddfc4b4cef07f5b0), //  242
    (0x95f83d0a1fb69cd9, 0x4abdaf101564f98e), //  243
    (0xbb764c4ca7a4440f, 0x9d6d1ad41abe37f1), //  244
    (0xea53df5fd18d5513, 0x84c86189216dc5ed), //  245
    (0x92746b9be2f8552c, 0x32fd3cf5b4e49bb4), //  246
    (0xb7118682dbb66a77, 0x3fbc8c33221dc2a1), //  247
    (0xe4d5e82392a40515, 0x0fabaf3feaa5334a), //  248
    (0x8f05b1163ba6832d, 0x29cb4d87f2a7400e), //  249
    (0xb2c71d5bca9023f8, 0x743e20e9ef511012), //  250
    (0xdf78e4b2bd342cf6, 0x914da9246b255416), //  251
    (0x8bab8eefb6409c1a, 0x1ad089b6c2f7548e), //  252
    (0xae9672aba3d0c320, 0xa184ac2473b529b1), //  253
    (0xda3c0f568cc4f3e8, 0xc9e5d72d90a2741e), //  254
    (0x8865899617fb1871, 0x7e2fa67c7a658892), //  255
    (0xaa7eebfb9df9de8d, 0xddbb901b98feeab7), //  256
    (0xd51ea6fa85785631, 0x552a74227f3ea565), //  257
    (0x8533285c936b35de, 0xd53a88958f87275f), //  258
    (0xa67ff273b8460356, 0x8a892abaf368f137), //  259
    (0xd01fef10a657842c, 0x2d2b7569b0432d85), //  260
    (0x8213f56a67f6b29b, 0x9c3b29620e29fc73), //  261
    (0xa298f2c501f45f42, 0x8349f3ba91b47b8f), //  262
    (0xcb3f2f7642717713, 0x241c70a936219a73), //  263
    (0xfe0efb53d30dd4d7, 0xed238cd383aa0110), //  264
    (0x9ec95d1463e8a506, 0xf4363804324a40aa), //  265
    (0xc67bb4597ce2ce48, 0xb143c6053edcd0d5), //  266
    (0xf81aa16fdc1b81da, 0xdd94b7868e94050a), //  267
    (0x9b10a4e5e9913128, 0xca7cf2b4191c8326), //  268
    (0xc1d4ce1f63f57d72, 0xfd1c2f611f63a3f0), //  269
    (0xf24a01a73cf2dccf, 0xbc633b39673c8cec), //  270
    (0x976e41088617ca01, 0xd5be0503e085d813), //  271
    (0xbd49d14aa79dbc82, 0x4b2d8644d8a74e18), //  272
    (0xec9c459d51852ba2, 0xddf8e7d60ed1219e), //  273
    (0x93e1ab8252f33b45, 0xcabb90e5c942b503), //  274
    (0xb8da1662e7b00a17, 0x3d6a751f3b936243), //  275
    (0xe7109bfba19c0c9d, 0x0cc512670a783ad4), //  276
    (0x906a617d450187e2, 0x27fb2b80668b24c5), //  277
    (0xb484f9dc9641e9da, 0xb1f9f660802dedf6), //  278
    (0xe1a63853bbd26451, 0x5e7873f8a0396973), //  279
    (0x8d07e33455637eb2, 0xdb0b487b6423e1e8), //  280
    (0xb049dc016abc5e5f, 0x91ce1a9a3d2cda62), //  281
    (0xdc5c5301c56b75f7, 0x7641a140cc7810fb), //  282
    (0x89b9b3e11b6329ba, 0xa9e904c87fcb0a9d), //  283
    (0xac2820d9623bf429, 0x546345fa9fbdcd44), //  284
    (0xd732290fbacaf133, 0xa97c177947ad4095), //  285
    (0x867f59a9d4bed6c0, 0x49ed8eabcccc485d), //  286
    (0xa81f301449ee8c70, 0x5c68f256bfff5a74), //  287
    (0xd226fc195c6a2f8c, 0x73832eec6fff3111), //  288
    (0x83585d8fd9c25db7, 0xc831fd53c5ff7eab), //  289
    (0xa42e74f3d032f525, 0xba3e7ca8b77f5e55), //  290
    (0xcd3a1230c43fb26f, 0x28ce1bd2e55f35eb), //  291
    (0x80444b5e7aa7cf85, 0x7980d163cf5b81b3), //  292
    (0xa0555e361951c366, 0xd7e105bcc332621f), //  293
    (0xc86ab5c39fa63440, 0x8dd9472bf3fefaa7), //  294
    (0xfa856334878fc150, 0xb14f98f6f0feb951), //  295
    (0x9c935e00d4b9d8d2, 0x6ed1bf9a569f33d3), //  296
    (0xc3b8358109e84f07, 0x0a862f80ec4700c8), //  297
    (0xf4a642e14c6262c8, 0xcd27bb612758c0fa), //  298
    (0x98e7e9cccfbd7dbd, 0x8038d51cb897789c), //  299
    (0xbf21e44003acdd2c, 0xe0470a63e6bd56c3), //  300
    (0xeeea5d5004981478, 0x1858ccfce06cac74), //  301
    (0x95527a5202df0ccb, 0x0f37801e0c43ebc8), //  302
    (0xbaa718e68396cffd, 0xd30560258f54e6ba), //  303
    (0xe950df20247c83fd, 0x47c6b82ef32a2069), //  304
    (0x91d28b7416cdd27e, 0x4cdc331d57fa5441), //  305
    (0xb6472e511c81471d, 0xe0133fe4adf8e952), //  306
    (0xe3d8f9e563a198e5, 0x58180fddd97723a6), //  307
    (0x8e679c2f5e44ff8f, 0x570f09eaa7ea7648), //  308
    (0xb201833b35d63f73, 0x2cd2cc6551e513da), //  309
    (0xde81e40a034bcf4f, 0xf8077f7ea65e58d1), //  310
    (0x8b112e86420f6191, 0xfb04afaf27faf782), //  311
    (0xadd57a27d29339f6, 0x79c5db9af1f9b563), //  312
    (0xd94ad8b1c7380874, 0x18375281ae7822bc), //  313
    (0x87cec76f1c830548, 0x8f2293910d0b15b5), //  314
    (0xa9c2794ae3a3c69a, 0xb2eb3875504ddb22), //  315
    (0xd433179d9c8cb841, 0x5fa60692a46151eb), //  316
    (0x849feec281d7f328, 0xdbc7c41ba6bcd333), //  317
    (0xa5c7ea73224deff3, 0x12b9b522906c0800), //  318
    (0xcf39e50feae16bef, 0xd768226b34870a00), //  319
    (0x81842f29f2cce375, 0xe6a1158300d46640), //  320
    (0xa1e53af46f801c53, 0x60495ae3c1097fd0), //  321
    (0xca5e89b18b602368, 0x385bb19cb14bdfc4), //  322
    (0xfcf62c1dee382c42, 0x46729e03dd9ed7b5), //  323
    (0x9e19db92b4e31ba9, 0x6c07a2c26a8346d1), //  324
];

// Computes 128-bit result of multiplication of two 64-bit unsigned integers.
#[cfg_attr(feature = "no-panic", no_panic)]
fn umul128(x: u64, y: u64) -> u128 {
    u128::from(x) * u128::from(y)
}

#[cfg_attr(feature = "no-panic", no_panic)]
fn umul192_upper128(x_hi: u64, x_lo: u64, y: u64) -> uint128 {
    let p = umul128(x_hi, y);
    let lo = (p as u64).wrapping_add((umul128(x_lo, y) >> 64) as u64);
    uint128 {
        hi: (p >> 64) as u64 + u64::from(lo < p as u64),
        lo,
    }
}

// Computes upper 64 bits of multiplication of x and y, discards the least
// significant bit and rounds to odd, where x = uint128_t(x_hi << 64) | x_lo.
#[cfg_attr(feature = "no-panic", no_panic)]
fn umul_upper_inexact_to_odd<UInt>(x_hi: u64, x_lo: u64, y: UInt) -> UInt
where
    UInt: traits::UInt,
{
    let num_bits = mem::size_of::<UInt>() * 8;
    if num_bits == 64 {
        let uint128 { hi, lo } = umul192_upper128(x_hi, x_lo, y.into());
        UInt::truncate(hi | u64::from((lo >> 1) != 0))
    } else {
        let result = (umul128(x_hi, y.into()) >> 32) as u64;
        UInt::enlarge((result >> 32) as u32 | u32::from((result as u32 >> 1) != 0))
    }
}

// Returns {value / 100, value % 100} correct for values of up to 4 digits.
fn divmod100(value: u32) -> (u32, u32) {
    debug_assert!(value < 10_000);
    const EXP: u32 = 19; // 19 is faster or equal to 12 even for 3 digits.
    const SIG: u32 = (1 << EXP) / 100 + 1;
    let div = (value * SIG) >> EXP; // value / 100
    (div, value - div * 100)
}

#[cfg_attr(feature = "no-panic", no_panic)]
fn count_trailing_nonzeros(x: u64) -> usize {
    // We count the number of bytes until there are only zeros left.
    // The code is equivalent to
    //    8 - x.leading_zeros() / 8
    // but if the BSR instruction is emitted (as gcc on x64 does with default
    // settings), subtracting the constant before dividing allows the compiler
    // to combine it with the subtraction which it inserts due to BSR counting
    // in the opposite direction.
    //
    // Additionally, the BSR instruction requires a zero check. Since the high
    // bit is unused we can avoid the zero check by shifting the datum left by
    // one and inserting a sentinel bit at the end. This can be faster than the
    // automatically inserted range check.
    (70 - ((x.to_le() << 1) | 1).leading_zeros()) as usize / 8
}

// Align data since unaligned access may be slower when crossing a
// hardware-specific boundary.
#[repr(C, align(2))]
struct Digits2([u8; 200]);

static DIGITS2: Digits2 = Digits2(
    *b"0001020304050607080910111213141516171819\
       2021222324252627282930313233343536373839\
       4041424344454647484950515253545556575859\
       6061626364656667686970717273747576777879\
       8081828384858687888990919293949596979899",
);

// Converts value in the range [0, 100) to a string. GCC generates a bit better
// code when value is pointer-size (https://www.godbolt.org/z/5fEPMT1cc).
#[cfg_attr(feature = "no-panic", no_panic)]
unsafe fn digits2(value: usize) -> &'static u16 {
    debug_assert!(value < 100);

    #[allow(clippy::cast_ptr_alignment)]
    unsafe {
        &*DIGITS2.0.as_ptr().cast::<u16>().add(value)
    }
}

#[cfg_attr(feature = "no-panic", no_panic)]
fn to_bcd8(abcdefgh: u64) -> u64 {
    // An optimization from Xiang JunBo.
    // Three steps BCD. Base 10000 -> base 100 -> base 10.
    // div and mod are evaluated simultaneously as, e.g.
    //   (abcdefgh / 10000) << 32 + (abcdefgh % 10000)
    //      == abcdefgh + (2^32 - 10000) * (abcdefgh / 10000)))
    // where the division on the RHS is implemented by the usual multiply + shift
    // trick and the fractional bits are masked away.
    let abcd_efgh = abcdefgh + (0x100000000 - 10000) * ((abcdefgh * 0x68db8bb) >> 40);
    let ab_cd_ef_gh = abcd_efgh + (0x10000 - 100) * (((abcd_efgh * 0x147b) >> 19) & 0x7f0000007f);
    let a_b_c_d_e_f_g_h =
        ab_cd_ef_gh + (0x100 - 10) * (((ab_cd_ef_gh * 0x67) >> 10) & 0xf000f000f000f);
    a_b_c_d_e_f_g_h.to_be()
}

unsafe fn write_if_nonzero(buffer: *mut u8, digit: u32) -> *mut u8 {
    unsafe {
        *buffer = b'0' + digit as u8;
        buffer.add(usize::from(digit != 0))
    }
}

unsafe fn write8(buffer: *mut u8, value: u64) {
    unsafe {
        buffer.cast::<u64>().write_unaligned(value);
    }
}

const ZEROS: u64 = 0x30303030_30303030; // 0x30 == '0'

// Writes a significand consisting of up to 17 decimal digits (16-17 for
// normals) and removes trailing zeros.
#[cfg_attr(feature = "no-panic", no_panic)]
unsafe fn write_significand17(mut buffer: *mut u8, value: u64) -> *mut u8 {
    #[cfg(not(all(target_arch = "aarch64", target_feature = "neon", not(miri))))]
    {
        // Each digits is denoted by a letter so value is abbccddeeffgghhii where
        // digit a can be zero.
        let abbccddee = (value / 100_000_000) as u32;
        let ffgghhii = (value % 100_000_000) as u32;
        unsafe {
            buffer = write_if_nonzero(buffer, abbccddee / 100_000_000);
        }
        let bcd = to_bcd8(u64::from(abbccddee % 100_000_000));
        unsafe {
            write8(buffer, bcd | ZEROS);
        }
        if ffgghhii == 0 {
            return unsafe { buffer.add(count_trailing_nonzeros(bcd)) };
        }
        let bcd = to_bcd8(u64::from(ffgghhii));
        unsafe {
            write8(buffer.add(8), bcd | ZEROS);
            buffer.add(8).add(count_trailing_nonzeros(bcd))
        }
    }

    #[cfg(all(target_arch = "aarch64", target_feature = "neon", not(miri)))]
    {
        use core::arch::aarch64::*;
        use core::arch::asm;

        // An optimized version for NEON by Dougall Johnson.
        struct ToStringConstants {
            mul_const: u64,
            hundred_million: u64,
            multipliers32: int32x4_t,
            multipliers16: int16x8_t,
        }

        static CONSTANTS: ToStringConstants = ToStringConstants {
            mul_const: 0xabcc77118461cefd,
            hundred_million: 100000000,
            multipliers32: unsafe {
                mem::transmute::<[i32; 4], int32x4_t>([
                    0x68db8bb,
                    -10000 + 0x10000,
                    0x147b000,
                    -100 + 0x10000,
                ])
            },
            multipliers16: unsafe {
                mem::transmute::<[i16; 8], int16x8_t>([0xce0, -10 + 0x100, 0, 0, 0, 0, 0, 0])
            },
        };

        let mut c = ptr::addr_of!(CONSTANTS);

        // Compiler barrier, or clang doesn't load from memory and generates 15
        // more instructions
        let c = unsafe {
            asm!("/*{0}*/", inout(reg) c);
            &*c
        };

        let mut hundred_million = c.hundred_million;

        // Compiler barrier, or clang narrows the load to 32-bit and unpairs it.
        unsafe {
            asm!("/*{0}*/", inout(reg) hundred_million);
        }

        // Equivalent to abbccddee = value / 100000000, ffgghhii = value % 100000000.
        let mut abbccddee = ((u128::from(value) * u128::from(c.mul_const)) >> 90) as u64;
        let ffgghhii = value - abbccddee * hundred_million;

        // We could probably make this bit faster, but we're preferring to
        // reuse the constants for now.
        let a = ((u128::from(abbccddee) * u128::from(c.mul_const)) >> 90) as u64;
        abbccddee -= a * hundred_million;

        unsafe {
            buffer = write_if_nonzero(buffer, a as u32);

            let hundredmillions: uint64x1_t =
                mem::transmute::<u64, uint64x1_t>(abbccddee | (ffgghhii << 32));

            let high_10000: int32x2_t = mem::transmute::<uint32x2_t, int32x2_t>(vshr_n_u32(
                mem::transmute::<int32x2_t, uint32x2_t>(vqdmulh_n_s32(
                    mem::transmute::<uint64x1_t, int32x2_t>(hundredmillions),
                    mem::transmute::<int32x4_t, [i32; 4]>(c.multipliers32)[0],
                )),
                9,
            ));
            let tenthousands: int32x2_t = vmla_n_s32(
                mem::transmute::<uint64x1_t, int32x2_t>(hundredmillions),
                high_10000,
                mem::transmute::<int32x4_t, [i32; 4]>(c.multipliers32)[1],
            );

            let mut extended: int32x4_t = mem::transmute::<uint32x4_t, int32x4_t>(vshll_n_u16(
                mem::transmute::<int32x2_t, uint16x4_t>(tenthousands),
                0,
            ));

            // Compiler barrier, or clang breaks the subsequent MLA into UADDW +
            // MUL.
            asm!("/*{:v}*/", inout(vreg) extended);

            let high_100: int32x4_t = vqdmulhq_n_s32(
                extended,
                mem::transmute::<int32x4_t, [i32; 4]>(c.multipliers32)[2],
            );
            let hundreds: int32x4_t = vmlaq_n_s32(
                extended,
                high_100,
                mem::transmute::<int32x4_t, [i32; 4]>(c.multipliers32)[3],
            );
            let high_10: int16x8_t = vqdmulhq_n_s16(
                mem::transmute::<int32x4_t, int16x8_t>(hundreds),
                mem::transmute::<int16x8_t, [i16; 8]>(c.multipliers16)[0],
            );
            let digits: int16x8_t =
                mem::transmute::<uint8x16_t, int16x8_t>(vrev64q_u8(mem::transmute::<
                    int16x8_t,
                    uint8x16_t,
                >(vmlaq_n_s16(
                    mem::transmute::<int32x4_t, int16x8_t>(hundreds),
                    high_10,
                    mem::transmute::<int16x8_t, [i16; 8]>(c.multipliers16)[1],
                ))));
            let ascii: int16x8_t = mem::transmute::<uint16x8_t, int16x8_t>(vaddq_u16(
                mem::transmute::<int16x8_t, uint16x8_t>(digits),
                mem::transmute::<int8x16_t, uint16x8_t>(vdupq_n_s8(b'0' as i8)),
            ));

            buffer.cast::<int16x8_t>().write_unaligned(ascii);

            let is_zero: uint16x8_t =
                mem::transmute::<uint8x16_t, uint16x8_t>(vceqzq_u8(mem::transmute::<
                    int16x8_t,
                    uint8x16_t,
                >(digits)));
            let zeros: u64 = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(is_zero, 4)), 0);

            buffer.add(16 - ((!zeros).leading_zeros() as usize >> 2))
        }
    }
}

// Writes a significand consisting of up to 9 decimal digits (8-9 for normals)
// and removes trailing zeros.
#[cfg_attr(feature = "no-panic", no_panic)]
unsafe fn write_significand9(mut buffer: *mut u8, value: u32) -> *mut u8 {
    unsafe {
        buffer = write_if_nonzero(buffer, value / 100_000_000);
    }
    let bcd = to_bcd8(u64::from(value % 100_000_000));
    unsafe {
        write8(buffer, bcd | ZEROS);
        buffer.add(count_trailing_nonzeros(bcd))
    }
}

#[allow(non_camel_case_types)]
struct fp {
    sig: u64,
    exp: i32,
}

fn normalize<UInt>(mut dec: fp, subnormal: bool) -> fp
where
    UInt: traits::UInt,
{
    if !subnormal {
        return dec;
    }
    let num_bits = mem::size_of::<UInt>() * 8;
    while dec.sig
        < if num_bits == 64 {
            10_000_000_000_000_000
        } else {
            100_000_000
        }
    {
        dec.sig *= 10;
        dec.exp -= 1;
    }
    dec
}

// Computes the decimal exponent as floor(log10(2**bin_exp)) if regular or
// floor(log10(3/4 * 2**bin_exp)) otherwise, without branching.
const fn compute_dec_exp(bin_exp: i32, regular: bool) -> i32 {
    debug_assert!(bin_exp >= -1334 && bin_exp <= 2620);
    // log10_3_over_4_sig = round(log10(3/4) * 2**log10_2_exp)
    const LOG10_3_OVER_4_SIG: i32 = -131_008;
    // log10_2_sig = round(log10(2) * 2**log10_2_exp)
    const LOG10_2_SIG: i32 = 315_653;
    const LOG10_2_EXP: i32 = 20;
    (bin_exp * LOG10_2_SIG + !regular as i32 * LOG10_3_OVER_4_SIG) >> LOG10_2_EXP
}

// Computes a shift so that, after scaling by a power of 10, the intermediate
// result always has a fixed 128-bit fractional part (for double).
//
// Different binary exponents can map to the same decimal exponent, but place
// the decimal point at different bit positions. The shift compensates for this.
//
// For example, both 3 * 2**59 and 3 * 2**60 have dec_exp = 2, but dividing by
// 10^dec_exp puts the decimal point in different bit positions:
//   3 * 2**59 / 100 = 1.72...e+16  (needs shift = 1 + 1)
//   3 * 2**60 / 100 = 3.45...e+16  (needs shift = 2 + 1)
const fn compute_exp_shift(bin_exp: i32, dec_exp: i32) -> i32 {
    // log2_pow10_sig = round(log2(10) * 2**log2_pow10_exp) + 1
    const LOG2_POW10_SIG: i32 = 217_707;
    const LOG2_POW10_EXP: i32 = 16;
    debug_assert!(dec_exp >= -350 && dec_exp <= 350);
    // pow10_bin_exp = floor(log2(10**-dec_exp))
    let pow10_bin_exp = (-dec_exp * LOG2_POW10_SIG) >> LOG2_POW10_EXP;
    // pow10 = ((pow10_hi << 64) | pow10_lo) * 2**(pow10_bin_exp - 127)

    // Shift to ensure the intermediate result of multiplying by a power of 10
    // has a fixed 128-bit fractional part. For example, 3 * 2**59 and 3 * 2**60
    // both have dec_exp = 2 and dividing them by 10**dec_exp would have the
    // decimal point in different (bit) positions without the shift:
    //   3 * 2**59 / 100 = 1.72...e+16 (exp_shift = 1 + 1)
    //   3 * 2**60 / 100 = 3.45...e+16 (exp_shift = 2 + 1)
    bin_exp + pow10_bin_exp + 1
}

// Converts a binary FP number bin_sig * 2**bin_exp to the shortest decimal
// representation.
#[cfg_attr(feature = "no-panic", no_panic)]
fn to_decimal<UInt>(bin_sig: UInt, bin_exp: i32, regular: bool, subnormal: bool) -> fp
where
    UInt: traits::UInt,
{
    let dec_exp = compute_dec_exp(bin_exp, regular);
    let exp_shift = compute_exp_shift(bin_exp, dec_exp);
    let (mut pow10_hi, mut pow10_lo) =
        *unsafe { POW10_SIGNIFICANDS.get_unchecked((-dec_exp - DEC_EXP_MIN) as usize) };

    let num_bits = mem::size_of::<UInt>() as i32 * 8;
    if regular && !subnormal {
        let integral; // integral part of bin_sig * pow10
        let fractional; // fractional part of bin_sig * pow10
        if num_bits == 64 {
            let result = umul192_upper128(pow10_hi, pow10_lo, (bin_sig << exp_shift).into());
            integral = UInt::truncate(result.hi);
            fractional = result.lo;
        } else {
            let result = umul128(pow10_hi, (bin_sig << exp_shift).into());
            integral = UInt::truncate((result >> 64) as u64);
            fractional = result as u64;
        }
        let digit = integral.into() % 10;

        // Switch to a fixed-point representation with the least significant
        // integral digit in the upper bits and fractional digits in the lower
        // bits.
        let num_integral_bits = if num_bits == 64 { 4 } else { 32 };
        let num_fractional_bits = 64 - num_integral_bits;
        let ten = 10u64 << num_fractional_bits;
        // Fixed-point remainder of the scaled significand modulo 10.
        let scaled_sig_mod10 = (digit << num_fractional_bits) | (fractional >> num_integral_bits);

        // scaled_half_ulp = 0.5 * pow10 in the fixed-point format.
        // dec_exp is chosen so that 10**dec_exp <= 2**bin_exp < 10**(dec_exp + 1).
        // Since 1ulp == 2**bin_exp it will be in the range [1, 10) after scaling
        // by 10**dec_exp. Add 1 to combine the shift with division by two.
        let scaled_half_ulp = pow10_hi >> (num_integral_bits - exp_shift + 1);
        let upper = scaled_sig_mod10 + scaled_half_ulp;
        const HALF_ULP: u64 = 1 << 63;

        // An optimization from yy by Yaoyuan Guo:
        if {
            // Exact half-ulp tie when rounding to nearest integer.
            fractional != HALF_ULP &&
            // Exact half-ulp tie when rounding to nearest 10.
            scaled_sig_mod10 != scaled_half_ulp &&
            // Near-boundary case for rounding to nearest 10.
            ten.wrapping_sub(upper) > 1
        } {
            let round_up = upper >= ten;
            let shorter = integral.into() - digit + u64::from(round_up) * 10;
            let longer = integral.into() + u64::from(fractional >= HALF_ULP);
            let use_shorter = scaled_sig_mod10 <= scaled_half_ulp || round_up;
            return fp {
                #[cfg(zmij_no_select_unpredictable)]
                sig: if use_shorter { shorter } else { longer },
                #[cfg(not(zmij_no_select_unpredictable))]
                sig: hint::select_unpredictable(use_shorter, shorter, longer),
                exp: dec_exp,
            };
        }
    }

    // Fallback to Schubfach to guarantee correctness in boundary cases and
    // switch to strict overestimates of powers of 10.
    if num_bits == 64 {
        pow10_lo += 1;
    } else {
        pow10_hi += 1;
    }

    // Shift the significand so that boundaries are integer.
    const BOUND_SHIFT: u32 = 2;
    let bin_sig_shifted = bin_sig << BOUND_SHIFT;

    // Compute the estimates of lower and upper bounds of the rounding interval
    // by multiplying them by the power of 10 and applying modified rounding.
    let lsb = bin_sig & UInt::from(1);
    let lower = (bin_sig_shifted - (UInt::from(regular) + UInt::from(1))) << exp_shift;
    let lower = umul_upper_inexact_to_odd(pow10_hi, pow10_lo, lower) + lsb;
    let upper = (bin_sig_shifted + UInt::from(2)) << exp_shift;
    let upper = umul_upper_inexact_to_odd(pow10_hi, pow10_lo, upper) - lsb;

    // The idea of using a single shorter candidate is by Cassio Neri.
    // It is less or equal to the upper bound by construction.
    let shorter = UInt::from(10) * ((upper >> BOUND_SHIFT) / UInt::from(10));
    if (shorter << BOUND_SHIFT) >= lower {
        return normalize::<UInt>(
            fp {
                sig: shorter.into(),
                exp: dec_exp,
            },
            subnormal,
        );
    }

    let scaled_sig = umul_upper_inexact_to_odd(pow10_hi, pow10_lo, bin_sig_shifted << exp_shift);
    let dec_sig_below = scaled_sig >> BOUND_SHIFT;
    let dec_sig_above = dec_sig_below + UInt::from(1);

    // Pick the closest of dec_sig_below and dec_sig_above and check if it's in
    // the rounding interval.
    let cmp = scaled_sig
        .wrapping_sub((dec_sig_below + dec_sig_above) << 1)
        .to_signed();
    let below_closer = cmp < UInt::from(0).to_signed()
        || (cmp == UInt::from(0).to_signed() && (dec_sig_below & UInt::from(1)) == UInt::from(0));
    let below_in = (dec_sig_below << BOUND_SHIFT) >= lower;
    let dec_sig = if below_closer & below_in {
        dec_sig_below
    } else {
        dec_sig_above
    };
    normalize::<UInt>(
        fp {
            sig: dec_sig.into(),
            exp: dec_exp,
        },
        subnormal,
    )
}

/// Writes the shortest correctly rounded decimal representation of `value` to
/// `buffer`. `buffer` should point to a buffer of size `buffer_size` or larger.
#[cfg_attr(feature = "no-panic", no_panic)]
unsafe fn write<Float>(value: Float, mut buffer: *mut u8) -> *mut u8
where
    Float: traits::Float,
{
    let num_bits = mem::size_of::<Float>() as i32 * 8;
    let bits = value.to_bits();

    unsafe {
        *buffer = b'-';
        buffer = buffer.add((bits >> (num_bits - 1)).into() as usize);
    }

    let num_sig_bits = Float::MANTISSA_DIGITS as i32 - 1;
    let implicit_bit = Float::UInt::from(1) << num_sig_bits;
    let mut bin_sig = bits & (implicit_bit - Float::UInt::from(1)); // binary significand
    let mut regular = bin_sig != Float::UInt::from(0);

    let num_exp_bits = num_bits - num_sig_bits - 1;
    let exp_mask = (1 << num_exp_bits) - 1;
    let exp_bias = (1 << (num_exp_bits - 1)) - 1;
    let mut bin_exp = (bits >> num_sig_bits).into() as i32 & exp_mask; // binary exponent

    let mut subnormal = false;
    if bin_exp == 0 {
        if bin_sig == Float::UInt::from(0) {
            return unsafe {
                *buffer = b'0';
                *buffer.add(1) = b'.';
                *buffer.add(2) = b'0';
                buffer.add(3)
            };
        }
        // Handle subnormals.
        // Setting regular is not redundant: it avoids extra data dependencies
        // and register pressure on the hot path (measurable perf impact).
        regular = true;
        bin_sig |= implicit_bit;
        bin_exp = 1;
        subnormal = true;
    }
    bin_sig ^= implicit_bit;
    bin_exp -= num_sig_bits + exp_bias;

    let fp {
        sig: mut dec_sig,
        exp: mut dec_exp,
    } = to_decimal(bin_sig, bin_exp, regular, subnormal);
    let num_digits = Float::MAX_DIGITS10 as i32 - 2;
    let end = if num_bits == 64 {
        dec_exp += num_digits + i32::from(dec_sig >= 10_000_000_000_000_000);
        unsafe { write_significand17(buffer.add(1), dec_sig) }
    } else {
        if dec_sig < 10_000_000 {
            dec_sig *= 10;
            dec_exp -= 1;
        }
        dec_exp += num_digits + i32::from(dec_sig >= 100_000_000);
        unsafe { write_significand9(buffer.add(1), dec_sig as u32) }
    };

    let length = unsafe { end.offset_from(buffer.add(1)) } as usize;

    if num_bits == 32 && (-6..=12).contains(&dec_exp)
        || num_bits == 64 && (-5..=15).contains(&dec_exp)
    {
        if length as i32 - 1 <= dec_exp {
            // 1234e7 -> 12340000000.0
            return unsafe {
                ptr::copy(buffer.add(1), buffer, length);
                ptr::write_bytes(buffer.add(length), b'0', dec_exp as usize + 3 - length);
                *buffer.add(dec_exp as usize + 1) = b'.';
                buffer.add(dec_exp as usize + 3)
            };
        } else if 0 <= dec_exp {
            // 1234e-2 -> 12.34
            return unsafe {
                ptr::copy(buffer.add(1), buffer, dec_exp as usize + 1);
                *buffer.add(dec_exp as usize + 1) = b'.';
                buffer.add(length + 1)
            };
        } else {
            // 1234e-6 -> 0.001234
            return unsafe {
                ptr::copy(buffer.add(1), buffer.add((1 - dec_exp) as usize), length);
                ptr::write_bytes(buffer, b'0', (1 - dec_exp) as usize);
                *buffer.add(1) = b'.';
                buffer.add((1 - dec_exp) as usize + length)
            };
        }
    }

    unsafe {
        // 1234e30 -> 1.234e33
        *buffer = *buffer.add(1);
        *buffer.add(1) = b'.';
        buffer = buffer.add(length + usize::from(length > 1));
        *buffer = b'e';
        buffer = buffer.add(1);
    }
    let sign_ptr = buffer;
    let sign = b'-'.wrapping_add(u8::from(dec_exp >= 0) * b'+'.wrapping_sub(b'-'));
    let mask = i32::from(dec_exp >= 0) - 1;
    dec_exp = (dec_exp + mask) ^ mask; // absolute value
    unsafe {
        buffer = buffer.add(usize::from(dec_exp >= 10));
    }
    let (a, bb) = divmod100(dec_exp as u32);
    unsafe {
        *buffer = b'0' + a as u8;
        buffer = buffer.add(usize::from(dec_exp >= 100));
        buffer.cast::<u16>().write_unaligned(*digits2(bb as usize));
        *sign_ptr = sign;
        buffer.add(2)
    }
}

/// Safe API for formatting floating point numbers to text.
///
/// ## Example
///
/// ```
/// let mut buffer = zmij::Buffer::new();
/// let printed = buffer.format_finite(1.234);
/// assert_eq!(printed, "1.234");
/// ```
pub struct Buffer {
    bytes: [MaybeUninit<u8>; BUFFER_SIZE],
}

impl Buffer {
    /// This is a cheap operation; you don't need to worry about reusing buffers
    /// for efficiency.
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    pub fn new() -> Self {
        let bytes = [MaybeUninit::<u8>::uninit(); BUFFER_SIZE];
        Buffer { bytes }
    }

    /// Print a floating point number into this buffer and return a reference to
    /// its string representation within the buffer.
    ///
    /// # Special cases
    ///
    /// This function formats NaN as the string "NaN", positive infinity as
    /// "inf", and negative infinity as "-inf" to match std::fmt.
    ///
    /// If your input is known to be finite, you may get better performance by
    /// calling the `format_finite` method instead of `format` to avoid the
    /// checks for special cases.
    #[cfg_attr(feature = "no-panic", no_panic)]
    pub fn format<F: Float>(&mut self, f: F) -> &str {
        if f.is_nonfinite() {
            f.format_nonfinite()
        } else {
            self.format_finite(f)
        }
    }

    /// Print a floating point number into this buffer and return a reference to
    /// its string representation within the buffer.
    ///
    /// # Special cases
    ///
    /// This function **does not** check for NaN or infinity. If the input
    /// number is not a finite float, the printed representation will be some
    /// correctly formatted but unspecified numerical value.
    ///
    /// Please check [`is_finite`] yourself before calling this function, or
    /// check [`is_nan`] and [`is_infinite`] and handle those cases yourself.
    ///
    /// [`is_finite`]: f64::is_finite
    /// [`is_nan`]: f64::is_nan
    /// [`is_infinite`]: f64::is_infinite
    #[cfg_attr(feature = "no-panic", no_panic)]
    pub fn format_finite<F: Float>(&mut self, f: F) -> &str {
        unsafe {
            let end = f.write_to_zmij_buffer(self.bytes.as_mut_ptr().cast::<u8>());
            let len = end.offset_from(self.bytes.as_ptr().cast::<u8>()) as usize;
            let slice = slice::from_raw_parts(self.bytes.as_ptr().cast::<u8>(), len);
            str::from_utf8_unchecked(slice)
        }
    }
}

/// A floating point number, f32 or f64, that can be written into a
/// [`zmij::Buffer`][Buffer].
///
/// This trait is sealed and cannot be implemented for types outside of the
/// `zmij` crate.
#[allow(unknown_lints)] // rustc older than 1.74
#[allow(private_bounds)]
pub trait Float: private::Sealed {}
impl Float for f32 {}
impl Float for f64 {}

mod private {
    pub trait Sealed: crate::traits::Float {
        fn is_nonfinite(self) -> bool;
        fn format_nonfinite(self) -> &'static str;
        unsafe fn write_to_zmij_buffer(self, buffer: *mut u8) -> *mut u8;
    }

    impl Sealed for f32 {
        #[inline]
        fn is_nonfinite(self) -> bool {
            const EXP_MASK: u32 = 0x7f800000;
            let bits = self.to_bits();
            bits & EXP_MASK == EXP_MASK
        }

        #[cold]
        #[cfg_attr(feature = "no-panic", inline)]
        fn format_nonfinite(self) -> &'static str {
            const MANTISSA_MASK: u32 = 0x007fffff;
            const SIGN_MASK: u32 = 0x80000000;
            let bits = self.to_bits();
            if bits & MANTISSA_MASK != 0 {
                crate::NAN
            } else if bits & SIGN_MASK != 0 {
                crate::NEG_INFINITY
            } else {
                crate::INFINITY
            }
        }

        #[cfg_attr(feature = "no-panic", inline)]
        unsafe fn write_to_zmij_buffer(self, buffer: *mut u8) -> *mut u8 {
            unsafe { crate::write(self, buffer) }
        }
    }

    impl Sealed for f64 {
        #[inline]
        fn is_nonfinite(self) -> bool {
            const EXP_MASK: u64 = 0x7ff0000000000000;
            let bits = self.to_bits();
            bits & EXP_MASK == EXP_MASK
        }

        #[cold]
        #[cfg_attr(feature = "no-panic", inline)]
        fn format_nonfinite(self) -> &'static str {
            const MANTISSA_MASK: u64 = 0x000fffffffffffff;
            const SIGN_MASK: u64 = 0x8000000000000000;
            let bits = self.to_bits();
            if bits & MANTISSA_MASK != 0 {
                crate::NAN
            } else if bits & SIGN_MASK != 0 {
                crate::NEG_INFINITY
            } else {
                crate::INFINITY
            }
        }

        #[cfg_attr(feature = "no-panic", inline)]
        unsafe fn write_to_zmij_buffer(self, buffer: *mut u8) -> *mut u8 {
            unsafe { crate::write(self, buffer) }
        }
    }
}

impl Default for Buffer {
    #[inline]
    #[cfg_attr(feature = "no-panic", no_panic)]
    fn default() -> Self {
        Buffer::new()
    }
}
