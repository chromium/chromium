// based on https://github.com/rust-lang/regex/blob/ddeb85eaa3bdf79d6306cc92a9d8bd89d839b5cd/regex-test/lib.rs

use anyhow::{bail, Context as _, Result};
use bstr::{BString, ByteSlice, ByteVec as _};
use derivre::HashSet;
use serde::Deserialize;

/*
[[test]]
name = "basic23"
regex = '''a*(^a)'''
haystack = '''aa'''
matches = [[[0, 1], [0, 1]]]
match-limit = 1
anchored = true
*/

/// A regex test describes the inputs and expected outputs of a regex match.
///
/// Each `RegexTest` represents a single `[[test]]` table in a TOML test file.
#[derive(Clone, Debug, Deserialize)]
#[serde(deny_unknown_fields)]
#[allow(dead_code)]
pub struct RegexTest {
    #[serde(skip)]
    group: String,
    #[serde(default)]
    name: String,
    #[serde(skip)]
    pub full_name: String,
    pub regex: String,
    pub haystack: BString,
    pub matches: Vec<Vec<Vec<usize>>>,
    #[serde(rename = "match-limit")]
    pub match_limit: Option<usize>,
    #[serde(default)]
    pub anchored: bool,
    #[serde(default, rename = "case-insensitive")]
    pub case_insensitive: bool,
    #[serde(default)]
    pub unescape: bool,
    #[serde(default = "default_true")]
    pub unicode: bool,
    #[serde(default = "default_true")]
    pub utf8: bool,
    #[serde(default, rename = "match-kind")]
    pub match_kind: String,
    #[serde(default, rename = "search-kind")]
    pub search_kind: String,
}

/// A collection of regex tests.
#[derive(Clone, Debug, Deserialize)]
pub struct RegexTests {
    /// 'default' permits an empty TOML file.
    #[serde(default, rename = "test")]
    pub tests: Vec<RegexTest>,
    #[serde(skip)]
    seen: HashSet<String>,
}

impl Default for RegexTests {
    fn default() -> Self {
        Self::new()
    }
}

impl RegexTests {
    /// Create a new empty collection of glob tests.
    pub fn new() -> RegexTests {
        RegexTests {
            tests: vec![],
            seen: HashSet::default(),
        }
    }

    /// Load all of the TOML encoded tests in `data` into this collection.
    /// The given group name is assigned to all loaded tests.
    pub fn load_slice(&mut self, group_name: &str, data: &[u8]) -> Result<()> {
        let data = std::str::from_utf8(data)
            .with_context(|| format!("data in {} is not valid UTF-8", group_name))?;
        let mut index = 1;
        let mut tests: RegexTests = toml::from_str(data)
            .with_context(|| format!("error decoding TOML for '{}'", group_name))?;
        for t in &mut tests.tests {
            t.group = group_name.to_string();
            if t.name.is_empty() {
                t.name = format!("{}", index);
                index += 1;
            }
            t.full_name = format!("{}/{}", t.group, t.name);

            if t.unescape {
                t.haystack = BString::from(Vec::unescape_bytes(
                    // OK because TOML requires valid UTF-8.
                    t.haystack.to_str().unwrap(),
                ));
            }

            if self.seen.contains(&t.full_name) {
                bail!("found duplicate tests for name '{}'", t.full_name);
            }
            self.seen.insert(t.full_name.to_string());
        }
        self.tests.extend(tests.tests);
        Ok(())
    }
}

/// A function to set some boolean fields to a default of 'true'. We use a
/// function so that we can hand a path to it to Serde.
fn default_true() -> bool {
    true
}
