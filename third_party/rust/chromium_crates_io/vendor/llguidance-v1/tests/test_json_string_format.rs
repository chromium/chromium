// Tests for JSON string format validation via regex.
//
// Format regexes are defined in parser/src/json/formats.rs.
// Test cases are sourced from:
//   - RFC 3339 (date-time, time, date) — https://www.rfc-editor.org/rfc/rfc3339
//   - ISO 8601 (duration)
//   - RFC 5321 §4.1.2 (email) — https://www.rfc-editor.org/rfc/rfc5321#section-4.1.2
//   - RFC 1123 §2.1 (hostname) — https://www.rfc-editor.org/rfc/rfc1123#section-2.1
//   - RFC 4291 (ipv6) — https://www.rfc-editor.org/rfc/rfc4291
//   - RFC 4122 (uuid) — https://www.rfc-editor.org/rfc/rfc4122
//   - RFC 3986 (uri) — https://www.rfc-editor.org/rfc/rfc3986
//   - JSON Schema Test Suite (draft2020-12) — https://github.com/json-schema-org/JSON-Schema-Test-Suite
//   - Python tests (guidance) — https://github.com/guidance-ai/guidance/blob/main/tests/unit/library/json/test_string_format.py
//
// The regex_accepts_but_invalid_* tests document values that the current regex
// incorrectly accepts. When the regex is improved, flip true → false and move
// to the corresponding bad_* function.

use rstest::*;
use serde_json::json;

use llg_test_utils::json_schema_check;

#[rstest]
#[case("1963-06-19T08:30:06.283185Z")] // Test Suite: "a valid date-time string"
#[case("1963-06-19T08:30:06Z")] // Test Suite: "a valid date-time string without second fraction"
#[case("1985-04-12T23:20:50.52Z")] // RFC 3339 §5.8 example
#[case("1996-12-19T16:39:57-08:00")] // RFC 3339 §5.8 example (minus offset)
#[case("1990-12-31T23:59:60Z")] // RFC 3339 §5.8 example (leap second)
#[case("1990-12-31T15:59:60-08:00")] // RFC 3339 §5.8 example (leap second with offset)
#[case("1937-01-01T12:00:27.87+00:20")] // RFC 3339 §5.8 example (non-standard offset)
#[case("1963-06-19t08:30:06.283185z")] // Test Suite: "case-insensitive T and Z" (RFC 3339 §5.6 ABNF note)
#[case("1990-12-31T15:59:50.123-08:00")] // Test Suite: "a valid date-time string with minus offset"
#[case("2020-01-31T23:59:59Z")] // Boundary: 31-day month max day
#[case("2020-04-30T12:00:00Z")] // Boundary: 30-day month max day
#[case("2020-02-29T00:00:00Z")] // Boundary: February max day (leap year)
#[case("2020-01-01T00:00:00Z")] // Boundary: midnight
#[case("2020-06-15T10:00:00+23:59")] // Boundary: max positive offset
pub fn valid_date_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date-time"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("1963-06-38T08:30:06.283185Z")] // Original: invalid day
#[case("1998-12-31T23:59:61Z")] // Test Suite: "an invalid date-time past leap second, UTC"
#[case("1990-02-31T15:59:59.123-08:00")] // Test Suite: "an invalid day in date-time string"
#[case("1990-12-31T15:59:59-24:00")] // Test Suite: "an invalid offset in date-time string"
#[case("1963-06-19T08:30:06.28123+01:00Z")] // Test Suite: "an invalid closing Z after time-zone offset"
#[case("1990-12-31T24:00:00Z")] // Test Suite: "an invalid hour in date-time string"
#[case("1990-12-31T15:60:00Z")] // Test Suite: "an invalid minute in date-time string"
#[case("1990-12-31T10:00:00+10:60")] // Test Suite: "an invalid offset minute in date-time string"
#[case("06/19/1963 08:30:06 PST")] // Test Suite: "an invalid date-time string"
#[case("2013-350T01:01:01")] // Test Suite: "only RFC3339 not all of ISO 8601 are valid"
#[case("1963-6-19T08:30:06.283185Z")] // Test Suite: "invalid non-padded month dates"
#[case("1963-06-1T08:30:06.283185Z")] // Test Suite: "invalid non-padded day dates"
#[case("2020-04-31T12:00:00Z")] // RFC 3339 §5.7: April has max 30 days
#[case("2020-13-01T00:00:00Z")] // RFC 3339 §5.7: month must be 01-12
#[case("1963-06-1\u{09ea}T00:00:00Z")] // Python tests: non-ASCII Bengali ৪ in date portion
#[case("1963-06-11T0\u{09ea}:00:00Z")] // Python tests: non-ASCII Bengali ৪ in time portion
pub fn bad_date_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date-time"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("08:30:06.283185Z")] // Original: valid time with fractional seconds
#[case("08:30:06Z")] // Test Suite: "a valid time string"
#[case("23:59:60Z")] // Test Suite: "a valid time string with leap second, Zulu"
#[case("23:20:50.52Z")] // Test Suite: "a valid time string with second fraction"
#[case("08:30:06+00:20")] // Test Suite: "a valid time string with plus offset"
#[case("08:30:06-08:00")] // Test Suite: "a valid time string with minus offset"
#[case("08:30:06z")] // Test Suite: "a valid time string with case-insensitive Z"
#[case("23:59:60+00:00")] // Test Suite: "valid leap second, zero time-offset"
#[case("01:29:60+01:30")] // Python tests: "valid leap second, positive time-offset"
#[case("23:29:60+23:30")] // Python tests: "valid leap second, large positive time-offset"
#[case("15:59:60-08:00")] // Python tests: "valid leap second, negative time-offset"
#[case("00:29:60-23:30")] // Python tests: "valid leap second, large negative time-offset"
#[case("00:00:00Z")] // Boundary: midnight
#[case("23:59:59Z")] // Boundary: max valid time
pub fn valid_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"time"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("28:30:06.283185Z")] // Original: invalid hour
#[case("008:030:006Z")] // Test Suite: "invalid time string with extra leading zeros"
#[case("8:3:6Z")] // Test Suite: "invalid time string with no leading zero for single digit"
#[case("8:0030:6Z")] // Test Suite: "hour, minute, second must be two digits"
#[case("24:00:00Z")] // Test Suite: "an invalid time string with invalid hour"
#[case("00:60:00Z")] // Test Suite: "an invalid time string with invalid minute"
#[case("00:00:61Z")] // Test Suite: "an invalid time string with invalid second"
#[case("01:02:03+24:00")] // Test Suite: "an invalid time string with invalid time numoffset hour"
#[case("01:02:03+00:60")] // Test Suite: "an invalid time string with invalid time numoffset minute"
#[case("01:02:03Z+00:30")] // Test Suite: "an invalid time string with invalid time with both Z and numoffset"
#[case("08:30:06 PST")] // Test Suite: "an invalid offset indicator"
#[case("01:01:01,1111")] // Test Suite: "only RFC3339 not all of ISO 8601 are valid"
#[case("12:00:00")] // Test Suite: "no time offset" (RFC 3339 §5.6: time-offset required)
#[case("12:00:00.52")] // Python tests: "no time offset with second fraction"
#[case("08:30:06#00:20")] // Python tests: "offset not starting with plus or minus"
#[case("ab:cd:ef")] // Python tests: "contains letters"
#[case("1\u{09e8}:00:00Z")] // Python tests: non-ASCII Bengali ২ in hour
#[case("08:30:06-8:000")] // Python tests: "time-offset digits must be two digits"
pub fn bad_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"time"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("1963-06-19")] // Original: valid date
#[case("2020-01-31")] // Test Suite: "a valid date string with 31 days in January"
#[case("2020-02-29")] // Test Suite: "a valid date string with 29 days in February (leap)"
#[case("2020-03-31")] // Test Suite: "a valid date string with 31 days in March"
#[case("2020-04-30")] // Test Suite: "a valid date string with 30 days in April"
#[case("2020-05-31")] // Test Suite: "a valid date string with 31 days in May"
#[case("2020-06-30")] // Test Suite: "a valid date string with 30 days in June"
#[case("2020-07-31")] // Test Suite: "a valid date string with 31 days in July"
#[case("2020-08-31")] // Test Suite: "a valid date string with 31 days in August"
#[case("2020-09-30")] // Test Suite: "a valid date string with 30 days in September"
#[case("2020-10-31")] // Test Suite: "a valid date string with 31 days in October"
#[case("2020-11-30")] // Test Suite: "a valid date string with 30 days in November"
#[case("2020-12-31")] // Test Suite: "a valid date string with 31 days in December"
pub fn valid_date(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("1963-13-19")] // Original: invalid month
#[case("2020-01-32")] // Test Suite: "a invalid date string with 32 days in January"
#[case("2020-02-30")] // Test Suite: "a invalid date string with 30 days in February (leap)"
#[case("2020-04-31")] // Test Suite: "a invalid date string with 31 days in April"
#[case("2020-06-31")] // Test Suite: "a invalid date string with 31 days in June"
#[case("2020-09-31")] // Test Suite: "a invalid date string with 31 days in September"
#[case("2020-11-31")] // Test Suite: "a invalid date string with 31 days in November"
#[case("2020-13-01")] // Test Suite: "a invalid date string with invalid month"
#[case("06/19/1963")] // Test Suite: "an invalid date string" (wrong format)
#[case("2013-350")] // Test Suite: "only RFC3339 not all of ISO 8601 are valid"
#[case("1998-1-20")] // Test Suite: "non-padded month dates are not valid"
#[case("1998-01-1")] // Test Suite: "non-padded day dates are not valid"
#[case("2020-00-01")] // RFC 3339 §5.6: month must be 01-12
#[case("1963-06-1\u{09ea}")] // Python tests: non-ASCII Bengali ৪ replacing day digit
#[case("20230328")] // Python tests: ISO 8601 / non-RFC 3339: YYYYMMDD without dashes
#[case("2023-W01")] // Python tests: ISO 8601 / non-RFC 3339: week number implicit day of week
#[case("2023-W13-2")] // Python tests: ISO 8601 / non-RFC 3339: week number with day of week
#[case("2022W527")] // Python tests: ISO 8601 / non-RFC 3339: week number rollover to next year
pub fn bad_date(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("P1M")] // Original: one month duration
#[case("P4DT12H30M5S")] // Test Suite: "a valid duration string"
#[case("P4Y")] // Test Suite: "four years duration"
#[case("PT0S")] // Test Suite: "zero time, in seconds"
#[case("P0D")] // Test Suite: "zero time, in days"
#[case("PT1M")] // Test Suite: "one minute duration"
#[case("PT36H")] // Test Suite: "one and a half days, in hours"
#[case("P1DT12H")] // Test Suite: "one and a half days, in days and hours"
#[case("P2W")] // Test Suite: "two weeks" (ISO 8601)
#[case("P1Y2M3DT4H5M6S")] // ISO 8601: all components present
#[case("PT1H")] // ISO 8601: hours only
#[case("PT1S")] // ISO 8601: seconds only
pub fn valid_duration(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"duration"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("P2D1Y")] // Original: elements out of order
#[case("PT1D")] // Test Suite: "an invalid duration string" (days after T)
#[case("4DT12H30M5S")] // Test Suite: "must start with P"
#[case("P")] // Test Suite: "no elements present"
#[case("P1YT")] // Test Suite: "no time elements present"
#[case("PT")] // Test Suite: "no date or time elements present"
#[case("P1D2H")] // Test Suite: "missing time separator"
#[case("P2S")] // Test Suite: "time element in the date position"
#[case("P1Y2W")] // Test Suite: "weeks cannot be combined with other units"
#[case("P1")] // Test Suite: "element without unit"
#[case("P\u{09e8}Y")] // Python tests: non-ASCII Bengali ২ replacing year count
pub fn bad_duration(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"duration"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("joe.bloggs@example.com")] // Original / Test Suite: "a valid e-mail address"
#[case("te~st@example.com")] // Test Suite: "tilde in local part is valid"
#[case("~test@example.com")] // Test Suite: "tilde before local part is valid"
#[case("test~@example.com")] // Test Suite: "tilde after local part is valid"
#[case("te.s.t@example.com")] // Test Suite: "two separated dots inside local part are valid"
#[case("joe.bloggs@[127.0.0.1]")] // Test Suite: "an IPv4-address-literal after the @ is valid" (RFC 5321 §4.1.3)
#[case("user+tag@example.com")] // RFC 5321 §4.1.2: '+' is valid in dot-string
#[case("a@b.com")] // Boundary: minimal valid email
pub fn valid_email(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"email"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("joe.bloggs@@example.com")] // Original: double @
#[case("2962")] // Test Suite: "an invalid e-mail address" (just a number)
#[case(".test@example.com")] // Test Suite: "dot before local part is not valid" (RFC 5321 §4.1.2)
#[case("test.@example.com")] // Test Suite: "dot after local part is not valid" (RFC 5321 §4.1.2)
#[case("te..st@example.com")] // Test Suite: "two subsequent dots inside local part are not valid"
#[case("joe.bloggs@invalid=domain.com")] // Test Suite: "an invalid domain"
#[case("joe.bloggs@[127.0.0.300]")] // Test Suite: "an invalid IPv4-address-literal"
                                    // Note: Quoted-string tests (e.g., "joe bloggs"@example.com) are NOT ported —
                                    // our regex only supports RFC 5321 §4.1.2 dot-string format, not quoted-string.
pub fn bad_email(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"email"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("hostnam3")] // Original / Test Suite: "single label ending with digit"
#[case("www.example.com")] // Test Suite: "a valid host name"
#[case("hostname")] // Test Suite: "single label"
#[case("h0stn4me")] // Test Suite: "single label with digits"
#[case("1host")] // Test Suite: "single label starting with digit" (RFC 1123 §2.1)
#[case("host-name")] // Test Suite: "single label with hyphen"
#[case("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijk.com")] // Test Suite: "maximum label length (63)"
#[case("a")] // Boundary: single character label
pub fn valid_hostname(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"hostname"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("hostnam3-")] // Original / Test Suite: "ends with hyphen"
#[case("")] // Test Suite: "empty string"
#[case(".")] // Test Suite: "single dot"
#[case(".example")] // Test Suite: "leading dot"
#[case("example.")] // Test Suite: "trailing dot"
#[case("-hostname")] // Test Suite: "starts with hyphen" (RFC 1123 §2.1)
#[case("host_name")] // Test Suite: "contains underscore" (RFC 1123 §2.1)
#[case("abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyzabcdefghijkl.com")] // Test Suite: "exceeds maximum label length (63)"
                                                                                // Note: Punycode/IDN tests from the Test Suite are NOT ported — our regex only
                                                                                // validates ASCII label structure per RFC 1123.
pub fn bad_hostname(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"hostname"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("192.168.0.1")] // Original / Test Suite: "a valid IP address"
#[case("0.0.0.0")] // Boundary: all zeros (unspecified address)
#[case("255.255.255.255")] // Boundary: max value (broadcast)
#[case("127.0.0.1")] // Boundary: loopback
#[case("1.2.3.4")] // Boundary: single-digit octets
#[case("87.10.0.1")] // Test Suite: "value without leading zero is valid"
pub fn valid_ipv4(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"ipv4"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("192.168.0.0.1")] // Original / Test Suite: "an IP address with too many components"
#[case("256.256.256.256")] // Test Suite: "an IP address with out-of-range values"
#[case("127.0")] // Test Suite: "an IP address without 4 components"
#[case("0x7f000001")] // Test Suite: "an IP address as an integer"
#[case("2130706433")] // Test Suite: "an IP address as an integer (decimal)"
#[case("087.10.0.1")] // Test Suite: "invalid leading zeroes, as they are treated as octals"
#[case("192.168.1.0/24")] // Test Suite: "netmask is not a part of ipv4 address"
#[case("1\u{09e8}7.0.0.1")] // Python tests: non-ASCII Bengali ২ in first octet
pub fn bad_ipv4(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"ipv4"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("::42:ff:1")] // Original / Test Suite: "leading colons is valid"
#[case("::1")] // Test Suite: "a valid IPv6 address" (loopback, RFC 4291 §2.5.3)
#[case("::abef")] // Test Suite: "trailing 4 hex symbols is valid"
#[case("::")] // Test Suite: "no digits is valid" (all zeros, RFC 4291 §2.5.2)
#[case("d6::")] // Test Suite: "trailing colons is valid"
#[case("1:d6::42")] // Test Suite: "single set of double colons in the middle is valid"
#[case("1:2:3:4:5:6:7:8")] // Test Suite: "8 octets" (full form, RFC 4291 §2.2)
#[case("2001:0db8:85a3:0000:0000:8a2e:0370:7334")] // RFC 4291 §2.2: full uncompressed
#[case("fe80::1")] // RFC 4291 §2.5.6: link-local
#[case("2001:db8::1")] // Boundary: common compressed form
pub fn valid_ipv6(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"ipv6"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1")] // Original / Test Suite: "an IPv6 address with too many components"
#[case("12345::")] // Test Suite: "an IPv6 address with out-of-range values" (5 hex digits)
#[case("::abcef")] // Test Suite: "trailing 5 hex symbols is invalid"
#[case("::laptop")] // Test Suite: "an IPv6 address containing illegal characters"
#[case(":2:3:4:5:6:7:8")] // Test Suite: "missing leading octet is invalid"
#[case("1:2:3:4:5:6:7:")] // Test Suite: "missing trailing octet is invalid"
#[case("1::d6::42")] // Test Suite: "two sets of double colons is invalid" (RFC 4291 §2.2)
#[case("1:2:3:4:5:::8")] // Test Suite: "triple colons is invalid"
#[case("1:2:3:4:5:6:7")] // Test Suite: "insufficient octets without double colons"
#[case("1")] // Test Suite: "no colons is invalid"
#[case("1:2:3:4:5:6:7:\u{09ea}")] // Python tests: non-ASCII Bengali ৪ in last group
                                  // Note: IPv4-mapped tests (e.g., ::ffff:192.168.0.1) are NOT ported — the
                                  // standalone ipv6 regex does not support RFC 4291 §2.5.5 mixed notation.
pub fn bad_ipv6(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"ipv6"});
    json_schema_check(&schema, &json!(s), false);
}

#[rstest]
#[case("2eb8aa08-AA98-11ea-B4Aa-73B441D16380")] // Original / Test Suite: "mixed case"
#[case("2EB8AA08-AA98-11EA-B4AA-73B441D16380")] // Test Suite: "all upper-case" (RFC 4122 §3)
#[case("2eb8aa08-aa98-11ea-b4aa-73b441d16380")] // Test Suite: "all lower-case"
#[case("00000000-0000-0000-0000-000000000000")] // Test Suite: "all zeroes is valid" (nil UUID, RFC 4122 §4.1.7)
#[case("98d80576-482e-427f-8434-7f86890ab222")] // Test Suite: "valid version 4"
#[case("99c17cbb-656f-564a-940f-1a4568f03487")] // Test Suite: "valid version 5"
#[case("99c17cbb-656f-f64a-940f-1a4568f03487")] // Test Suite: "hypothetical version 15"
pub fn valid_uuid(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"uuid"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("2eb8-aa08-aa98-11ea-b4aa73b44-1d16380")] // Original / Test Suite: "too many dashes"
#[case("2eb8aa08-aa98-11ea-b4aa-73b441d1638")] // Test Suite: "wrong length"
#[case("2eb8aa08-aa98-11ea-73b441d16380")] // Test Suite: "missing section"
#[case("2eb8aa08-aa98-11ea-b4ga-73b441d16380")] // Test Suite: "bad characters (not hex)"
#[case("2eb8aa08aa9811eab4aa73b441d16380")] // Test Suite: "no dashes" (RFC 4122 §3)
#[case("2eb8aa08aa98-11ea-b4aa73b441d16380")] // Test Suite: "too few dashes"
#[case("2eb8aa08aa9811eab4aa73b441d16380----")] // Test Suite: "dashes in the wrong spot"
#[case("2eb8aa0-8aa98-11e-ab4aa7-3b441d16380")] // Test Suite: "shifted dashes"
pub fn bad_uuid(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"uuid"});
    json_schema_check(&schema, &json!(s), false);
}

// RFC 3986 Section 1.1.2 examples
#[rstest]
#[case("ftp://ftp.is.co.za/rfc/rfc1808.txt")]
#[case("http://www.ietf.org/rfc/rfc2396.txt")]
#[case("ldap://[2001:db8::7]/c=GB?objectClass?one")]
#[case("mailto:John.Doe@example.com")]
#[case("news:comp.infosystems.www.servers.unix")]
#[case("tel:+1-816-555-1212")]
#[case("telnet://192.0.2.16:80/")]
#[case("urn:oasis:names:specification:docbook:dtd:xml:4.1.2")]
// Additional valid URIs
#[case("http://example.com")]
#[case("https://example.com/path/to/resource")]
#[case("https://example.com?query=value")]
#[case("https://example.com#section")]
#[case("http://example.com:8080/")]
#[case("ftp://user:pass@ftp.example.com/")]
#[case("http://192.168.1.1/")]
#[case("http://[::1]:8080/path")]
#[case("https://example.com/path%20with%20spaces")]
#[case("file:///path/to/file")]
#[case("tel:+1-201-555-0123;ext=456")]
// IPv6 address variations
#[case("http://[2001:0db8:85a3:0000:0000:8a2e:0370:7334]/")] // Full IPv6
#[case("http://[2001:db8:85a3::8a2e:370:7334]/")]
// Compressed IPv6
// #[case("http://[::ffff:192.0.2.1]/")]                      // IPv4-mapped IPv6 style - TODO: not yet supported
#[case("http://[::1]/")] // Loopback
#[case("http://[::]/")]
// All zeros
// #[case("http://[fe80::1%25eth0]/")]                        // Link-local with zone ID - TODO: not yet supported
#[case("http://[2001:db8::1]:8080/path")] // IPv6 with port and path
#[case("http://user:pass@[2001:db8::1]:8080/path/to/resource")] // IPv6 with userinfo and path
#[case("http://[v1.test]/")] // IPvFuture format
// Additional schemes
#[case("ssh://git@github.com:22/user/repo.git")] // SSH
#[case("git://github.com/user/repo.git")] // Git protocol
#[case("svn://svn.example.com/repo/trunk")] // Subversion
#[case("sftp://user@host.example.com/path/to/file")] // SFTP
#[case("s3://bucket-name/key/path")] // Amazon S3
#[case("data:text/plain;base64,SGVsbG8=")] // Data URI
#[case("javascript:void(0)")] // JavaScript (common in web)
#[case("magnet:?xt=urn:btih:abc123")] // Magnet link
#[case("redis://localhost:6379/0")] // Redis
#[case("postgres://user:pass@localhost:5432/db")] // PostgreSQL
#[case("mysql://user:pass@localhost:3306/db")] // MySQL
#[case("mongodb://localhost:27017/mydb")] // MongoDB
#[case("amqp://user:pass@host:5672/vhost")] // AMQP (RabbitMQ)
#[case("ws://example.com/socket")] // WebSocket
#[case("wss://example.com/socket")] // WebSocket Secure
#[case("irc://irc.example.com:6667/channel")] // IRC
#[case("xmpp:user@example.com")] // XMPP/Jabber
#[case("sip:user@example.com")] // SIP
#[case("sips:user@example.com:5061")] // SIPS (secure SIP)
#[case("rtsp://media.example.com:554/stream")] // RTSP (streaming)
#[case("spotify:track:4uLU6hMCjMI75M1A2tKUQC")] // Spotify
#[case("slack://channel?team=T123&id=C456")] // Slack
#[case("vscode://file/path/to/file.txt")] // VS Code
#[case("x-custom-scheme://anything/goes/here")] // Custom scheme with x- prefix
// Schemes with +, -, . characters
#[case("git+ssh://git@github.com/user/repo.git")] // Git over SSH
#[case("coap+tcp://example.com/sensor")] // CoAP over TCP
#[case("ms-windows-store://pdp?productid=abc123")] // Microsoft Store
// Complex URI with multiple components
#[case("https://user:pass@api.example.com:8443/v2/users/123/profile?name=John%20Doe&active=true&sort=desc#contact-info")]
// JSON Schema Test Suite cases (draft2020-12)
#[case("http://foo.bar/?baz=qux#quux")] // URL with anchor tag
#[case("http://foo.com/blah_(wikipedia)_blah#cite-1")] // URL with parentheses and anchor
#[case("http://foo.bar/?q=Test%20URL-encoded%20stuff")] // URL-encoded query
#[case("http://xn--nw2a.xn--j6w193g/")] // Puny-coded URL
#[case("http://-.~_!$&'()*+,;=:%40:80%2f::::::@example.com")] // Many special characters
#[case("http://223.255.255.254")] // IPv4-based URL
pub fn valid_uri(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"uri"});
    json_schema_check(&schema, &json!(s), true);
}

#[rstest]
#[case("//example.com/path")] // No scheme
#[case("123://example.com")] // Invalid scheme (starts with digit)
#[case("http://example.com/%GG")] // Invalid percent-encoding
#[case("/path/to/resource")] // Bare path (no scheme)
#[case("http://example.com/path with spaces")] // Unencoded spaces
// Invalid IPv6
#[case("http://[:::]/")] // Too many colons
#[case("http://2001:db8::1/")] // IPv6 without brackets
#[case("http://[2001:db8:85a3:0000:0000:8a2e:0370:7334:extra]/")] // Too many groups
// JSON Schema Test Suite invalid cases (draft2020-12)
#[case("\\\\WINDOWS\\fileshare")] // Windows path
#[case("abc")] // Just a string, no scheme
#[case("http:// shouldfail.com")] // Space after scheme
#[case(":// should fail")] // Missing scheme with spaces
#[case("bar,baz:foo")]
// Comma in scheme
#[case("https://[@example.org/test.txt")] // Invalid userinfo with [
#[case("https://example.org/foobar\\.txt")] // Backslash in path
#[case("https://example.org/foobar<>.txt")] // Invalid <> characters
#[case("https://example.org/foobar{}.txt")] // Invalid {} characters
#[case("https://example.org/foobar^.txt")] // Invalid ^ character
#[case("https://example.org/foobar`.txt")] // Invalid ` character
#[case("https://example.org/foo bar.txt")] // Invalid SPACE character
#[case("https://example.org/foobar|.txt")] // Invalid | character
// Scheme issues
#[case("a@b://example.com")] // Invalid @ in scheme
#[case("://example.com")] // Empty scheme
// Authority issues
#[case("http://example.com:abc/")] // Non-numeric port
#[case("http://user@@example.com/")] // Double @
#[case("http://[::1/path")] // Unclosed IPv6 bracket
#[case("http://exa mple.com/")] // Space in host
// Encoding issues
#[case("http://example.com/%")] // Incomplete percent encoding
#[case("http://example.com/%a")] // Incomplete percent encoding (one hex digit)
// Control characters
#[case("http://example.com/path\x00")] // Null byte
#[case("http://example.com/\t")] // Tab character
pub fn bad_uri(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"uri"});
    json_schema_check(&schema, &json!(s), false);
}

// ===== Accepted-but-invalid tests =====
//
// These values are INVALID per the relevant RFC but ACCEPTED by the current regex.
// They document known regex limitations. When the regex is improved, flip
// `true` → `false` and move the case to the corresponding `bad_*` function.

// TODO: The date-time regex allows second=60 at any hour:minute, but RFC 3339 §5.7
// restricts leap seconds to 23:59:60 UTC (shifted by offset for other time zones).
#[rstest]
#[case("1998-12-31T23:58:60Z")] // RFC 3339 §5.7: leap second only valid at minute 59
#[case("1998-12-31T22:59:60Z")] // RFC 3339 §5.7: leap second only valid at hour 23
#[case("2021-02-29T12:00:00Z")] // RFC 3339 §5.7 + Appendix C: 2021 is not a leap year
pub fn regex_accepts_but_invalid_date_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date-time"});
    json_schema_check(&schema, &json!(s), true);
}

// TODO: Same leap-second limitation as date-time — the time regex allows :60
// at any hour:minute.
#[rstest]
#[case("22:59:60Z")] // RFC 3339 §5.7: leap second only valid at hour 23
#[case("23:58:60Z")] // RFC 3339 §5.7: leap second only valid at minute 59
#[case("12:30:60Z")] // RFC 3339 §5.7: leap second at arbitrary time
pub fn regex_accepts_but_invalid_time(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"time"});
    json_schema_check(&schema, &json!(s), true);
}

// TODO: The date regex allows Feb 29 for all years. RFC 3339 §5.7 + Appendix C
// specify that Feb 29 is only valid in leap years (divisible by 4, except
// centuries not divisible by 400).
#[rstest]
#[case("2021-02-29")] // Test Suite: "2021 is not a leap year"
#[case("1900-02-29")] // RFC 3339 Appendix C: century year not divisible by 400
#[case("2100-02-29")] // RFC 3339 Appendix C: century year not divisible by 400
pub fn regex_accepts_but_invalid_date(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"date"});
    json_schema_check(&schema, &json!(s), true);
}

// TODO: The hostname regex enforces per-label length (1–63) but not overall
// hostname length. RFC 1123 §2.1 limits total hostname to 253 characters.
#[test]
pub fn regex_accepts_but_invalid_hostname() {
    let schema = json!({"type":"string", "format":"hostname"});
    // Build a hostname >253 chars with valid per-label lengths (each ≤63)
    let long_hostname = format!("{0}.{0}.{0}.{0}.{0}.com", "a".repeat(50));
    assert!(
        long_hostname.len() > 253,
        "Test hostname must exceed 253 chars"
    );
    json_schema_check(&schema, &json!(long_hostname), true);
}

#[rstest]
#[case("Some string")]
pub fn valid_unknown(#[case] s: &str) {
    let schema = json!({"type":"string", "format":"unknown"});
    json_schema_check(&schema, &json!(s), true);
}
