use crate::HashMap;
use anyhow::{anyhow, bail, Result};
use serde::{Deserialize, Serialize};
use serde_json::Value;
use toktrie::TokTrie;

#[derive(Debug, Deserialize, Serialize)]
struct AddedToken {
    id: usize,
    content: String,
    special: bool,
}

fn add_bytes(tokens: &mut Vec<Vec<u8>>, idx: usize, bytes: Vec<u8>) {
    if tokens.len() <= idx {
        tokens.resize(idx + 1, vec![]);
    }
    tokens[idx] = bytes;
}

// useful when debugging this: https://www.cogsci.ed.ac.uk/~richard/utf-8.cgi

fn is_self_mapped(c: char) -> bool {
    matches!(c, '!'..='~' | '\u{00A1}'..='\u{00AC}' | '\u{00AE}'..='\u{00FF}')
}

fn build_char_map() -> HashMap<char, u8> {
    let mut res = HashMap::default();
    let mut k = 0x100u32;
    for byte in 0..=255u8 {
        let c = byte as char;
        if is_self_mapped(c) {
            res.insert(c, byte);
        } else {
            res.insert(char::from_u32(k).unwrap(), byte);
            k += 1;
        }
    }
    res
}

/// Parse HF tokenizer.json file and return bytes for every token
pub fn token_bytes_from_tokenizer_json(tokenizer_json: &Value) -> Result<Vec<Vec<u8>>> {
    let mut is_byte_level = false;
    let mut is_byte_fallback = false;
    let mut space_ch = ' ';

    let decoder = &tokenizer_json["decoder"];
    if decoder["type"].as_str() == Some("ByteLevel") {
        is_byte_level = true;
    } else if decoder["type"].as_str() == Some("Sequence") {
        if let Some(decoders) = decoder["decoders"].as_array() {
            for decoder in decoders {
                if decoder["type"].as_str() == Some("ByteFallback") {
                    is_byte_fallback = true;
                } else if decoder["type"].as_str() == Some("Replace")
                    && decoder["content"].as_str() == Some(" ")
                {
                    if let Some(s) = decoder["pattern"]["String"].as_str() {
                        let s: Vec<char> = s.chars().collect();
                        if s.len() == 1 {
                            space_ch = s[0];
                        }
                    }
                }
            }
        }
    }

    if !is_byte_fallback && !is_byte_level {
        bail!("can't determine decoder type: {:?}", decoder["type"]);
    }

    let mut token_bytes = vec![];
    let added_tokens: Vec<AddedToken> =
        serde_json::from_value(tokenizer_json["added_tokens"].clone())
            .map_err(|e| anyhow!("error parsing added_tokens: {}", e))?;

    for info in added_tokens.iter() {
        let mut bytes = info.content.as_bytes().to_vec();
        if info.special {
            bytes.insert(0, TokTrie::SPECIAL_TOKEN_MARKER);
        }
        add_bytes(&mut token_bytes, info.id, bytes);
    }

    let char_map = build_char_map();

    let vocab: HashMap<String, usize> =
        serde_json::from_value(tokenizer_json["model"]["vocab"].clone())
            .map_err(|e| anyhow!("error parsing vocab: {}", e))?;

    for (tok_name, &tok_id) in vocab.iter() {
        if tok_id < token_bytes.len() && !token_bytes[tok_id].is_empty() {
            continue; // skip specials already added
        }

        let bytes = if is_byte_fallback {
            if tok_name.len() == 6 && tok_name.starts_with("<0x") && tok_name.ends_with(">") {
                // parse hex number from tok_name
                let hex_str = &tok_name[3..5];
                let byte = u8::from_str_radix(hex_str, 16).unwrap();
                vec![byte]
            } else {
                assert!(!tok_name.starts_with("<0x"));
                let tok_name = tok_name.replace(space_ch, " ");
                tok_name.as_bytes().to_vec()
            }
        } else if is_byte_level {
            let bytes: Result<Vec<u8>> = tok_name
                .chars()
                .map(|c| {
                    char_map
                        .get(&c)
                        .copied()
                        .ok_or_else(|| anyhow!("missing char: {}", c))
                })
                .collect();
            match bytes {
                Ok(b) => b,
                Err(e) => {
                    bail!("error: {} decoding {:?}", e, tok_name);
                }
            }
        } else {
            panic!();
        };
        add_bytes(&mut token_bytes, tok_id, bytes);
    }

    Ok(token_bytes)
}
