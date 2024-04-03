/// The method is not meant to be used by other crates! It may change
/// or be removed in the future, with no regard for backwards compatibility.
#[allow(clippy::missing_panics_doc)]
pub fn escape_string(input: &str, out: &mut String) {
	for ch in input.chars() {
		match ch {
			'\\' => out.push_str("\\\\"),
			'"' => out.push_str("\\\""),
			'\n' => out.push_str("\\n"),
			'\r' => out.push_str("\\r"),
			'\t' => out.push_str("\\t"),
			'\x20'..='\x7e' => out.push(ch),
			_ => {
				let mut buf = [0; 2];
				for &mut code_unit in ch.encode_utf16(&mut buf) {
					out.push_str("\\u");
					out.push(char::from_digit(u32::from(code_unit) / 0x1000, 16).unwrap());
					out.push(char::from_digit(u32::from(code_unit) % 0x1000 / 0x100, 16).unwrap());
					out.push(char::from_digit(u32::from(code_unit) % 0x100 / 0x10, 16).unwrap());
					out.push(char::from_digit(u32::from(code_unit) % 0x10, 16).unwrap());
				}
			}
		}
	}
}

#[cfg(test)]
mod tests {
	use super::*;

	#[track_caller]
	fn test_json_str(input: &str, expected: &str) {
		let mut out = String::new();
		escape_string(input, &mut out);
		assert_eq!(out, expected);
	}

	#[test]
	fn json_string_encoding() {
		test_json_str("abc", "abc");
		test_json_str("fancy string\n", "fancy string\\n");
		test_json_str("\n\t\r\0\\\'\"", "\\n\\t\\r\\u0000\\\\'\\\"");
		test_json_str("\u{1d54a}", "\\ud835\\udd4a");
	}
}
