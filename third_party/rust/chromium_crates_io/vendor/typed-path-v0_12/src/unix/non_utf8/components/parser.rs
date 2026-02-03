use crate::common::parser::*;
use crate::unix::constants::{CURRENT_DIR, PARENT_DIR, SEPARATOR};
use crate::unix::UnixComponent;

/// Parser to get [`UnixComponent`]s
///
/// ### Details
///
/// When parsing the path, there is a small amount of normalization:
///
/// Repeated separators are ignored, so a/b and a//b both have a and b as components.
///
/// Occurrences of . are normalized away, except if they are at the beginning of the path. For
/// example, a/./b, a/b/, a/b/. and a/b all have a and b as components, but ./a/b starts with an
/// additional CurDir component.
///
/// A trailing slash is normalized away, /a/b and /a/b/ are equivalent.
///
/// Note that no other normalization takes place; in particular, a/c and a/b/../c are distinct, to
/// account for the possibility that b is a symbolic link (so its parent isn’t a).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Parser<'a> {
    input: &'a [u8],
    state: State,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum State {
    // If input is still at the beginning
    AtBeginning,

    // If input has moved passed the beginning
    NotAtBeginning,
}

impl State {
    #[inline]
    pub fn is_at_beginning(self) -> bool {
        matches!(self, Self::AtBeginning)
    }
}

impl<'a> Parser<'a> {
    /// Create a new parser for the given `input`
    pub fn new(input: &'a [u8]) -> Self {
        Self {
            input,
            state: State::AtBeginning,
        }
    }

    /// Returns the input remaining for the parser
    pub fn remaining(&self) -> &'a [u8] {
        self.input
    }

    /// Parses next component, advancing an internal input pointer past the component
    pub fn next_front(&mut self) -> Result<UnixComponent<'a>, ParseError> {
        let (input, component) = parse_front(self.state)(self.input)?;
        self.input = input;
        self.state = State::NotAtBeginning;
        Ok(component)
    }

    /// Parses next component, advancing an internal input pointer past the component, but from the
    /// back of the input instead of the front
    pub fn next_back(&mut self) -> Result<UnixComponent<'a>, ParseError> {
        let (input, component) = parse_back(self.state)(self.input)?;
        self.input = input;
        Ok(component)
    }
}

fn parse_front(state: State) -> impl FnMut(ParseInput) -> ParseResult<UnixComponent> {
    move |input: ParseInput| {
        match state {
            // If we are at the beginning, we want to allow for root directory and '.'
            State::AtBeginning => suffixed(
                any_of!('_, root_dir, parent_dir, cur_dir, normal),
                move_front_to_next,
            )(input),

            // If we are not at the beginning, then we only want to allow for '..' and file names
            State::NotAtBeginning => {
                suffixed(any_of!('_, parent_dir, normal), move_front_to_next)(input)
            }
        }
    }
}

fn parse_back(state: State) -> impl FnMut(ParseInput) -> ParseResult<UnixComponent> {
    move |input: ParseInput| {
        let original_input = input;
        let is_sep = |b: u8| b == SEPARATOR as u8;

        // Skip any '.' and trailing separators we encounter
        let (input, _) = move_back_to_next(input)?;

        // If at beginning and our resulting input is empty, this means that we only had '.' and
        // separators remaining, which means that we want to check the front instead for our
        // component since we are supporting '.' and root directory (which is our separator)
        if state.is_at_beginning() && input.is_empty() {
            let (_, component) = parse_front(state)(original_input)?;

            return Ok((b"", component));
        }

        // Otherwise, look for next separator in reverse so we can parse everything after it
        let (input, after_sep) = rtake_until_byte_1(is_sep)(input)?;

        // Parse the component, failing if we don't fully parse it
        let (_, component) = fully_consumed(any_of!('_, parent_dir, normal))(after_sep)?;

        // Trim off any remaining trailing '.' and separators
        //
        // NOTE: This would cause problems for detecting root/current dir in reverse, so we must
        // provide an input subset if we detect at beginning and start with root
        let (input, _) = match state {
            State::AtBeginning if root_dir(input).is_ok() || cur_dir(input).is_ok() => {
                let (new_input, cnt) = consumed_cnt(move_back_to_next)(input)?;

                // Preserve root dir!
                if input.len() == cnt {
                    (&input[..1], ())
                } else {
                    (new_input, ())
                }
            }
            _ => move_back_to_next(input)?,
        };

        Ok((input, component))
    }
}

///  Move from front to the next component that is not current directory
fn move_front_to_next(input: ParseInput) -> ParseResult<()> {
    let parser = zero_or_more(any_of!('_, separator, map(cur_dir, |_| ())));
    map(parser, |_| ())(input)
}

///  Move from back to the next component that is not current directory
fn move_back_to_next(input: ParseInput) -> ParseResult<()> {
    let is_sep = |b: u8| b == SEPARATOR as u8;

    // Loop to continually read backwards up to a separator, verify the contents
    let mut input = input;
    while !input.is_empty() {
        // Clear out trailing separators
        let (new_input, _) = rtake_until_byte(|b| !is_sep(b))(input)?;

        input = new_input;

        // Clear out trailing current directory
        match input.strip_suffix(CURRENT_DIR) {
            // Preceded by a separator, so we know this is actually a current directory
            Some(new_input) if new_input.ends_with(&[SEPARATOR as u8]) => input = new_input,

            // Consumed all input, so we know it was the final dangling current directory
            Some(new_input) if new_input.is_empty() => input = new_input,

            // Otherwise, not actually a current directory and we're done
            _ => break,
        }
    }

    Ok((input, ()))
}

fn root_dir(input: ParseInput) -> ParseResult<UnixComponent> {
    let (input, _) = separator(input)?;
    Ok((input, UnixComponent::RootDir))
}

fn cur_dir(input: ParseInput) -> ParseResult<UnixComponent> {
    let (input, _) = suffixed(bytes(CURRENT_DIR), any_of!('_, empty, peek(separator)))(input)?;
    Ok((input, UnixComponent::CurDir))
}

fn parent_dir(input: ParseInput) -> ParseResult<UnixComponent> {
    let (input, _) = suffixed(bytes(PARENT_DIR), any_of!('_, empty, peek(separator)))(input)?;
    Ok((input, UnixComponent::ParentDir))
}

fn normal(input: ParseInput) -> ParseResult<UnixComponent> {
    let (input, normal) = take_until_byte_1(|b| b == SEPARATOR as u8)(input)?;
    Ok((input, UnixComponent::Normal(normal)))
}

fn separator(input: ParseInput) -> ParseResult<()> {
    let (input, _) = byte(SEPARATOR as u8)(input)?;
    Ok((input, ()))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::no_std_compat::*;

    fn sep(cnt: usize) -> Vec<u8> {
        let mut v = Vec::new();
        for _ in 0..cnt {
            v.push(SEPARATOR as u8);
        }
        v
    }

    #[test]
    fn should_support_zero_or_more_trailing_separators_from_front() {
        let mut parser = Parser::new(b"a/b/c///");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"b/c///");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"c///");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_zero_or_more_trailing_separators_from_back() {
        let mut parser = Parser::new(b"a/b/c///");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"a/b");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"a");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_trailing_current_directory_from_front() {
        let mut parser = Parser::new(b"a/b/c/.");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"b/c/.");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"c/.");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_trailing_current_directory_from_back() {
        let mut parser = Parser::new(b"a/b/c/.");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"a/b");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"a");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_relative_directory_from_front() {
        let mut parser = Parser::new(b"a/b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_relative_directory_from_back() {
        let mut parser = Parser::new(b"a/b/c");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"a/b");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"a");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_relative_directory_starting_with_current_directory_from_front() {
        let mut parser = Parser::new(b"./a/b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.remaining(), b"a/b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_relative_directory_starting_with_current_directory_from_back() {
        let mut parser = Parser::new(b"./a/b/c");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"./a/b");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"./a");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b".");

        assert_eq!(parser.next_back(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_root_directory_from_front() {
        let mut parser = Parser::new(b"/a/b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"a/b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"b/c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"c");

        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_root_directory_from_back() {
        let mut parser = Parser::new(b"/a/b/c");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"/a/b");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.remaining(), b"/a");

        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"/");

        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_normalizing_current_directories_from_front() {
        let mut parser = Parser::new(b"/././.");

        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_normalizing_current_directories_from_back() {
        let mut parser = Parser::new(b"/././.");

        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_parsing_just_root_directory_from_front() {
        let mut parser = Parser::new(b"/");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");

        // Also works with multiple separators
        let mut parser = Parser::new(b"//");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_parsing_just_root_directory_from_back() {
        let mut parser = Parser::new(b"/");
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");

        // Also works with multiple separators
        let mut parser = Parser::new(b"//");
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_parsing_parent_directories_from_front() {
        let mut parser = Parser::new(b"..");
        assert_eq!(parser.next_front(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");

        // Supports back-to-back parent directories
        let mut parser = Parser::new(b"../..");
        assert_eq!(parser.next_front(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.next_front(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_parsing_parent_directories_from_back() {
        let mut parser = Parser::new(b"..");
        assert_eq!(parser.next_back(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");

        // Supports back-to-back parent directories
        let mut parser = Parser::new(b"../..");
        assert_eq!(parser.next_back(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.next_back(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
    }

    #[test]
    fn should_support_parsing_single_component_from_front() {
        // Empty input fails
        Parser::new(b"").next_front().unwrap_err();

        // Supports parsing any component individually
        let mut parser = Parser::new(&[SEPARATOR as u8]);
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(CURRENT_DIR);
        assert_eq!(parser.next_front(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(PARENT_DIR);
        assert_eq!(parser.next_front(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(b"hello");
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"hello")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Accepts invalid filname characters
        let mut parser = Parser::new(b"abc\0def");
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"abc\0def")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());
    }

    #[test]
    fn should_support_parsing_single_component_from_back() {
        // Empty input fails
        Parser::new(b"").next_back().unwrap_err();

        // Supports parsing any component individually
        let mut parser = Parser::new(&[SEPARATOR as u8]);
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        let mut parser = Parser::new(CURRENT_DIR);
        assert_eq!(parser.next_back(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        let mut parser = Parser::new(PARENT_DIR);
        assert_eq!(parser.next_back(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        let mut parser = Parser::new(b"hello");
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"hello")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Accepts invalid filname characters
        let mut parser = Parser::new(b"abc\0def");
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"abc\0def")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());
    }

    #[test]
    fn should_support_parsing_from_multiple_components_from_front() {
        // Empty input fails
        Parser::new(b"").next_front().unwrap_err();

        // Succeeds if finds a root dir
        let mut parser = Parser::new(b"/");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Multiple separators still just mean root
        let mut parser = Parser::new(b"//");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Succeeds even if there isn't a root
        //
        // E.g. a/b/c
        let mut parser = Parser::new(b"a/b/c");
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should support '.' at beginning of path
        //
        // E.g. ./b/c
        let mut parser = Parser::new(b"./b/c");
        assert_eq!(parser.next_front(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should remove current dir from anywhere if not at beginning
        //
        // E.g. /./b/./c/. -> /b/c
        let mut parser = Parser::new(b"/./b/./c/.");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should strip multiple separators and normalize '.'
        //
        // E.g. /////a///.//../// -> [ROOT, "a", CURRENT_DIR, PARENT_DIR]
        let mut parser = Parser::new(b"/////a///.//..///");
        assert_eq!(parser.next_front(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());
    }

    #[test]
    fn should_support_parsing_from_multiple_components_from_back() {
        // Empty input fails
        Parser::new(b"").next_back().unwrap_err();

        // Succeeds if finds a root dir
        let mut parser = Parser::new(b"/");
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Multiple separators still just mean root
        let mut parser = Parser::new(b"//");
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Succeeds even if there isn't a root
        //
        // E.g. a/b/c
        let mut parser = Parser::new(b"a/b/c");
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Should support '.' at beginning of path
        //
        // E.g. ./b/c
        let mut parser = Parser::new(b"./b/c");
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Should remove current dir from anywhere if not at beginning
        //
        // E.g. /./b/./c/. -> /b/c
        let mut parser = Parser::new(b"/./b/./c/.");
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"c")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"b")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());

        // Should strip multiple separators and normalize '.'
        //
        // E.g. /////a///.//../// -> [ROOT, "a", CURRENT_DIR, PARENT_DIR]
        let mut parser = Parser::new(b"/////a///.//..///");
        assert_eq!(parser.next_back(), Ok(UnixComponent::ParentDir));
        assert_eq!(parser.next_back(), Ok(UnixComponent::Normal(b"a")));
        assert_eq!(parser.next_back(), Ok(UnixComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_back().is_err());
    }

    mod helpers {
        use super::*;

        #[test]
        fn validate_move_front_to_next() {
            let (input, _) = move_front_to_next(b"").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_front_to_next(b".").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_front_to_next(b"./").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_front_to_next(b"./.").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_front_to_next(b"./a").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_front_to_next(b".//a").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_front_to_next(b"././a").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_front_to_next(b"././..").unwrap();
            assert_eq!(input, b"..");

            let (input, _) = move_front_to_next(b"..").unwrap();
            assert_eq!(input, b"..");

            let (input, _) = move_front_to_next(b"../.").unwrap();
            assert_eq!(input, b"../.");

            let (input, _) = move_front_to_next(b"/").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_front_to_next(b"/.").unwrap();
            assert_eq!(input, b"");
        }

        #[test]
        fn validate_move_back_to_next() {
            let (input, _) = move_back_to_next(b"").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_back_to_next(b".").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_back_to_next(b"./").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_back_to_next(b"./.").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_back_to_next(b"a/.").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_back_to_next(b"a//.").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_back_to_next(b"a/./.").unwrap();
            assert_eq!(input, b"a");

            let (input, _) = move_back_to_next(b".././.").unwrap();
            assert_eq!(input, b"..");

            let (input, _) = move_back_to_next(b"..").unwrap();
            assert_eq!(input, b"..");

            let (input, _) = move_back_to_next(b"./..").unwrap();
            assert_eq!(input, b"./..");

            let (input, _) = move_back_to_next(b"/").unwrap();
            assert_eq!(input, b"");

            let (input, _) = move_back_to_next(b"/.").unwrap();
            assert_eq!(input, b"");
        }

        #[test]
        fn validate_root_dir() {
            // Empty input fails
            root_dir(b"").unwrap_err();

            // Not starting with root dir fails
            root_dir(&[b'a', SEPARATOR as u8]).unwrap_err();

            // Succeeds just on its own
            let (input, value) = root_dir(&[SEPARATOR as u8]).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, UnixComponent::RootDir);

            // Succeeds, taking only what it matches
            let (input, value) = root_dir(&[SEPARATOR as u8, b'a', SEPARATOR as u8]).unwrap();
            assert_eq!(input, &[b'a', SEPARATOR as u8]);
            assert_eq!(value, UnixComponent::RootDir);
        }

        #[test]
        fn validate_cur_dir() {
            // Empty input fails
            cur_dir(b"").unwrap_err();

            // Not starting with current dir fails
            cur_dir(&[b"a", CURRENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = cur_dir(CURRENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, UnixComponent::CurDir);

            // Fails if more content after itself that is not a separator
            // E.g. .. will fail, .a will fail
            cur_dir(&[CURRENT_DIR, b"."].concat()).unwrap_err();
            cur_dir(&[CURRENT_DIR, b"a"].concat()).unwrap_err();

            // Succeeds, taking only what it matches
            let input = &[CURRENT_DIR, &sep(1), CURRENT_DIR].concat();
            let (input, value) = cur_dir(input).unwrap();
            assert_eq!(input, &[&sep(1), CURRENT_DIR].concat());
            assert_eq!(value, UnixComponent::CurDir);
        }

        #[test]
        fn validate_parent_dir() {
            // Empty input fails
            parent_dir(b"").unwrap_err();

            // Not starting with parent dir fails
            parent_dir(&[b"a", PARENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = parent_dir(PARENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, UnixComponent::ParentDir);

            // Fails if more content after itself that is not a separator
            // E.g. ... will fail, ..a will fail
            parent_dir(&[PARENT_DIR, b"."].concat()).unwrap_err();
            parent_dir(&[PARENT_DIR, b"a"].concat()).unwrap_err();

            // Succeeds, taking only what it matches
            let input = &[PARENT_DIR, &sep(1), PARENT_DIR].concat();
            let (input, value) = parent_dir(input).unwrap();
            assert_eq!(input, &[&sep(1), PARENT_DIR].concat());
            assert_eq!(value, UnixComponent::ParentDir);
        }

        #[test]
        fn validate_normal() {
            // Empty input fails
            normal(b"").unwrap_err();

            // Fails if takes nothing
            normal(&[SEPARATOR as u8, b'a']).unwrap_err();

            // Succeeds just on its own
            let (input, value) = normal(b"hello").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, UnixComponent::Normal(b"hello"));

            // Succeeds, taking up to next separator
            let (input, value) = normal(b"hello/world").unwrap();
            assert_eq!(input, b"/world");
            assert_eq!(value, UnixComponent::Normal(b"hello"));

            // Accepts invalid characters in filename
            let (input, value) = normal(b"hel\0lo").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, UnixComponent::Normal(b"hel\0lo"));
        }

        #[test]
        fn validate_separator() {
            // Empty input fails
            separator(b"").unwrap_err();

            // Not starting with separator fails
            separator(&[b'a', SEPARATOR as u8]).unwrap_err();

            // Succeeds just on its own
            let (input, _) = separator(&[SEPARATOR as u8]).unwrap();
            assert_eq!(input, b"");

            // Succeeds, taking only what it matches
            let (input, _) = separator(&[SEPARATOR as u8, b'a', SEPARATOR as u8]).unwrap();
            assert_eq!(input, &[b'a', SEPARATOR as u8]);
        }
    }
}
