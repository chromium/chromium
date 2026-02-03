use crate::common::parser::*;
use crate::windows::constants::{ALT_SEPARATOR, CURRENT_DIR, PARENT_DIR, SEPARATOR};
use crate::windows::{WindowsComponent, WindowsPrefix, WindowsPrefixComponent};

/// Parse input to get [`WindowsComponents`]
///
/// ### Details
///
/// When parsing the path, there is a small amount of normalization:
///
/// Repeated separators are ignored, so a\\b and a\\\\b both have a and b as components.
///
/// Occurrences of '.' are normalized away, except if they are at the beginning of the path. For
/// example, a\\.\\b, a\\b\\, a\\b\\. and a\\b all have a and b as components, but .\\a\\b starts
/// with an additional CurDir component.
///
/// A trailing slash is normalized away, \\a\\b and \\a\\b\\ are equivalent.
///
/// Note that no other normalization takes place; in particular, a\c and a\b\..\c are distinct, to
/// account for the possibility that b is a symbolic link (so its parent isn’t a).
///
/// ### Verbatim disabling of normalization
///
/// When using `\\?\`, [normalization is skipped](https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats#skip-normalization).
///
/// This means that from the above rules:
///
/// * '.' is not normalized away
/// * '/' is not used as a separator
///
/// Note that repeat separators are still removed and trailing slashes are still not included
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct Parser<'a> {
    input: &'a [u8],
    state: State,

    /// Pre-calculated prefix for more optimized backwards traversal stops
    prefix: Option<WindowsPrefixComponent<'a>>,

    /// Whether or not to normalize while parsing
    normalize: bool,
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
enum State {
    // If input is still at the beginning, meaning we could see a root
    AtBeginning,

    // If input has moved passed the beginning to the point where we will see a root
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
        // Before the parser can operate, it needs to know if it should normalize the path. This
        // happens in all cases EXCEPT when the path starts with exactly \\?\
        let normalize = !input.starts_with(br"\\?\");

        // NOTE: Usage of `maybe` guarantees that we will never have an error
        let (_, prefix) = maybe(prefix_component)(input).unwrap();

        Self {
            input,
            state: State::AtBeginning,
            prefix,
            normalize,
        }
    }

    /// Returns the input remaining for the parser
    pub fn remaining(&self) -> &'a [u8] {
        self.input
    }

    /// Parses next component, advancing an internal input pointer past the component
    pub fn next_front(&mut self) -> Result<WindowsComponent<'a>, ParseError> {
        // If we have a prefix, return it instead of parsing
        // NOTE: We don't actually update the state to not be at the beginning since our state is
        //       reflecting being at the beginning of the path, not the prefix
        if let Some(prefix) = self.prefix.take() {
            self.input = &self.input[prefix.len()..];
            return Ok(WindowsComponent::Prefix(prefix));
        }

        // Otherwise, parse our input like usual
        let (input, component) = parse_front(self.state, self.normalize)(self.input)?;
        self.input = input;
        self.state = State::NotAtBeginning;
        Ok(component)
    }

    /// Parses next component, advancing an internal input pointer past the component, but from the
    /// back of the input instead of the front
    pub fn next_back(&mut self) -> Result<WindowsComponent<'a>, ParseError> {
        // If we are parsing from the back, see if we still have something to parse that is not the
        // prefix; otherwise, take our prefix if it exists or fail
        let input = self.remaining_without_prefix();
        let prefix_len = self.prefix_len();

        if !input.is_empty() {
            let (input, component) = parse_back(self.state, self.normalize)(input)?;

            // NOTE: The updated input does not include the prefix, so we need to adjust our update
            //       to factor in the prefix len
            let len = input.len() + prefix_len;
            self.input = &self.input[..len];

            Ok(component)
        } else if let Some(prefix) = self.prefix.take() {
            self.input = &self.input[prefix_len..];
            Ok(WindowsComponent::Prefix(prefix))
        } else {
            Err("empty input")
        }
    }

    /// Returns the input remaining for the parser except for the prefix if it exists
    fn remaining_without_prefix(&self) -> &'a [u8] {
        &self.input[self.prefix_len()..]
    }

    #[inline]
    fn prefix_len(&self) -> usize {
        self.prefix.map(|p| p.len()).unwrap_or(0)
    }
}

fn parse_front(
    state: State,
    normalize: bool,
) -> impl FnMut(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        // If normalizing, we accept '\' or '/'. If not normalizing, only accept '\'.
        let root_dir = root_dir(normalize);
        let cur_dir = cur_dir(normalize);
        let filename = filename(normalize);
        let move_to_next = move_front_to_next(normalize);

        match state {
            // If we have seen a prefix and not moved beyond, we want to allow for root
            // directory and current directory
            State::AtBeginning => {
                suffixed(any_of!('_, root_dir, cur_dir, filename), move_to_next)(input)
            }

            // If we are not at the beginning, then we only want to allow for '..' and file
            // names
            State::NotAtBeginning => suffixed(filename, move_to_next)(input),
        }
    }
}

fn parse_back(
    state: State,
    normalize: bool,
) -> impl FnMut(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        let original_input = input;

        // Skip any trailing separators we encounter AND -- if normalizing -- current directories
        let (input, _) = move_back_to_next(normalize)(input)?;

        // If at beginning and our resulting input is empty, this means that we only had '.' and
        // separators remaining, which means that we want to check the front instead for our
        // component since we are supporting '.' and root directory (which is our separator)
        if state.is_at_beginning() && input.is_empty() {
            let (_, component) = parse_front(state, normalize)(original_input)?;

            return Ok((b"", component));
        }

        // Otherwise, look for next separator in reverse so we can parse everything after it
        let (input, after_sep) = rtake_until_byte_1(|b| is_separator(b, normalize))(input)?;

        // Parse the component, failing if we don't fully parse it
        let (_, component) = fully_consumed(filename(normalize))(after_sep)?;

        // Trim off any remaining trailing '.' and separators
        //
        // NOTE: This would cause problems for detecting prefix/root/current dir in reverse, so we
        // must provide an input subset if we detect at beginning and start with root
        let (input, _) = match state {
            State::AtBeginning
                if root_dir(normalize)(input).is_ok() || cur_dir(normalize)(input).is_ok() =>
            {
                let (new_input, cnt) = consumed_cnt(move_back_to_next(normalize))(input)?;

                // Preserve root dir!
                if input.len() == cnt {
                    (&input[..1], ())
                } else {
                    (new_input, ())
                }
            }
            _ => move_back_to_next(normalize)(input)?,
        };

        Ok((input, component))
    }
}

fn root_dir(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        if input.is_empty() {
            Err("empty input")
        } else if !is_separator(input[0], normalize) {
            Err("not root dir")
        } else {
            Ok((&input[1..], WindowsComponent::RootDir))
        }
    }
}

// Filename will not include current directory (except at beginning) if normalizing
fn filename(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<WindowsComponent> {
    let cur_dir = cur_dir(normalize);
    let parent_dir = parent_dir(normalize);
    let normal = normal(normalize);

    // NOTE: For some reason, I couldn't get the lifetimes to work using any_of! with
    //       the below closure; so, have to do it manually instead
    move |input: ParseInput| {
        if let Ok((input, value)) = parent_dir(input) {
            return Ok((input, value));
        }

        if !normalize {
            if let Ok((input, value)) = cur_dir(input) {
                return Ok((input, value));
            }
        }

        if let Ok((input, value)) = normal(input) {
            return Ok((input, value));
        }

        Err("invalid filename")
    }
}

fn cur_dir(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        let (input, _) = bytes(CURRENT_DIR)(input)?;

        // Check if we consumed everything or have a separator next
        if !input.is_empty() && !is_separator(input[0], normalize) {
            return Err("more non-separator bytes after parent dir");
        }

        Ok((input, WindowsComponent::CurDir))
    }
}

fn parent_dir(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        let (input, _) = bytes(PARENT_DIR)(input)?;

        // Check if we consumed everything or have a separator next
        if !input.is_empty() && !is_separator(input[0], normalize) {
            return Err("more non-separator bytes after parent dir");
        }

        Ok((input, WindowsComponent::ParentDir))
    }
}

fn normal(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<WindowsComponent> {
    move |input: ParseInput| {
        let (input, normal) = normal_bytes(normalize)(input)?;
        Ok((input, WindowsComponent::Normal(normal)))
    }
}

/// Parse normal bytes
///
/// NOTE: If we were really nice, we'd parse up to the disallowed filenames, but Rust and other
///       implementations don't appear to do that and instead just jump to the next separator
fn normal_bytes(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<ParseInput> {
    move |input: ParseInput| {
        let (input, normal) = take_until_byte_1(|b| is_separator(b, normalize))(input)?;
        Ok((input, normal))
    }
}

///  Move from front to the next component
///
///  * if `normalize` is true, this will skip over current directories and also support the
///    alternate separator (`/`)
///  * if `normalize` is false, this will NOT skip over current directories and will only support
///    the primary separator (`\`)
fn move_front_to_next(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<()> {
    move |input: ParseInput| {
        if normalize {
            let parser =
                zero_or_more(any_of!('_, separator(normalize), map(cur_dir(normalize), |_| ())));
            map(parser, |_| ())(input)
        } else {
            map(zero_or_more(separator(normalize)), |_| ())(input)
        }
    }
}

///  Move from back to the next component
///
///  * if `normalize` is true, this will skip over current directories and also support the
///    alternate separator (`/`)
///  * if `normalize` is false, this will NOT skip over current directories and will only support
///    the primary separator (`\`)
fn move_back_to_next(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<()> {
    move |input: ParseInput| {
        // Loop to continually read backwards up to a separator, verify the contents
        let mut input = input;
        while !input.is_empty() {
            // Clear out trailing separators
            let (new_input, _) = rtake_until_byte(|b| !is_separator(b, normalize))(input)?;

            input = new_input;

            // If we are not normalizing, we can exit now as we've consumed all trailing separators
            if !normalize {
                break;
            }

            // Clear out trailing current directory
            match input.strip_suffix(CURRENT_DIR) {
                // Preceded by a separator, so we know this is actually a current directory
                Some(new_input) if ends_with_separator(new_input, normalize) => input = new_input,

                // Consumed all input, so we know it was the final dangling current directory
                Some(new_input) if new_input.is_empty() => input = new_input,

                // Otherwise, not actually a current directory and we're done
                _ => break,
            }
        }

        Ok((input, ()))
    }
}

fn is_separator(b: u8, normalize: bool) -> bool {
    b == SEPARATOR as u8 || (normalize && b == ALT_SEPARATOR as u8)
}

fn ends_with_separator(input: &[u8], normalize: bool) -> bool {
    input
        .last()
        .map(|b| is_separator(*b, normalize))
        .unwrap_or(false)
}

/// For Windows, verbatim paths only use '\' as a separator
fn separator(normalize: bool) -> impl Fn(ParseInput) -> ParseResult<()> {
    move |input: ParseInput| {
        if input.starts_with(&[SEPARATOR as u8])
            || (normalize && input.starts_with(&[ALT_SEPARATOR as u8]))
        {
            Ok((&input[1..], ()))
        } else {
            Err("not a separator")
        }
    }
}

pub fn prefix_component(input: ParseInput) -> ParseResult<WindowsPrefixComponent> {
    let (new_input, parsed) = prefix(input)?;

    Ok((
        new_input,
        WindowsPrefixComponent {
            raw: &input[..(input.len() - new_input.len())],
            parsed,
        },
    ))
}

#[allow(mismatched_lifetime_syntaxes)]
fn prefix<'a>(input: ParseInput<'a>) -> ParseResult<WindowsPrefix> {
    any_of!('a,
        prefix_verbatim_unc,
        prefix_verbatim_disk,
        prefix_verbatim,
        prefix_device_ns,
        prefix_unc,
        prefix_disk,
    )(input)
}

/// Format is `\\?\UNC\SERVER\SHARE` where the backslash is interchangeable with a forward slash
fn prefix_verbatim_unc(input: ParseInput) -> ParseResult<WindowsPrefix> {
    // Based on verbatim, if it is EXACTLY \\?\, then we do NOT want to normalize
    let normalize = !input.starts_with(br"\\?\");

    let (input, _) = verbatim(input)?;
    let (input, _) = bytes(b"UNC")(input)?;
    let (input, _) = separator(normalize)(input)?;
    let (input, server) = normal_bytes(normalize)(input)?;
    let (input, _) = maybe(separator(normalize))(input)?;
    let (input, maybe_share) = maybe(normal_bytes(normalize))(input)?;

    Ok((
        input,
        WindowsPrefix::VerbatimUNC(server, maybe_share.unwrap_or(b"")),
    ))
}

/// Format is `\\?\PICTURES:` where the backslash is interchangeable with a forward slash
#[allow(mismatched_lifetime_syntaxes)]
fn prefix_verbatim<'a>(input: ParseInput<'a>) -> ParseResult<WindowsPrefix> {
    let (input, _) = not(prefix_verbatim_disk)(input)?;
    let (input, _) = not(prefix_verbatim_unc)(input)?;

    // Based on verbatim, if it is EXACTLY \\?\, then we do NOT want to normalize
    let normalized = !input.starts_with(br"\\?\");

    let (input, _) = verbatim(input)?;

    // NOTE: Support a blank verbatim because this is what Rust's stdlib does
    //
    // e.g. `\\?\\hello\world` -> [Verbatim(""), RootDir, Normal("hello"), Normal("world")]
    let (input, value) =
        any_of!('a, normal_bytes(normalized), map(peek(separator(normalized)), |_| b""))(input)?;

    Ok((input, WindowsPrefix::Verbatim(value)))
}

/// Format is `\\?\DISK:` where the backslash is interchangeable with a forward slash
fn prefix_verbatim_disk(input: ParseInput) -> ParseResult<WindowsPrefix> {
    map(prefixed(verbatim, disk_byte), WindowsPrefix::VerbatimDisk)(input)
}

/// Matches `\\?\` where the backslash is interchangeable with a forward slash
fn verbatim(input: ParseInput) -> ParseResult<()> {
    let (input, _) = separator(true)(input)?;
    let (input, _) = separator(true)(input)?;
    let (input, _) = byte(b'?')(input)?;
    let (input, _) = separator(true)(input)?;
    Ok((input, ()))
}

/// Format is `\\.\DEVICE` where the backslash is interchangeable with a forward slash
fn prefix_device_ns(input: ParseInput) -> ParseResult<WindowsPrefix> {
    let (input, _) = separator(true)(input)?;
    let (input, _) = separator(true)(input)?;
    let (input, _) = byte(b'.')(input)?;
    let (input, _) = separator(true)(input)?;

    map(normal_bytes(true), WindowsPrefix::DeviceNS)(input)
}

/// Format is `\\SERVER\SHARE` where the backslash is interchangeable with a forward slash
fn prefix_unc(input: ParseInput) -> ParseResult<WindowsPrefix> {
    let (input, _) = separator(true)(input)?;
    let (input, _) = separator(true)(input)?;

    let (input, server) = normal_bytes(true)(input)?;
    let (input, _) = maybe(separator(true))(input)?;
    let (input, maybe_share) = maybe(normal_bytes(true))(input)?;

    Ok((
        input,
        WindowsPrefix::UNC(server, maybe_share.unwrap_or(b"")),
    ))
}

/// Format is `C:`
fn prefix_disk(input: ParseInput) -> ParseResult<WindowsPrefix> {
    map(disk_byte, WindowsPrefix::Disk)(input)
}

/// `"C:" -> "C"` and `"c:" -> "C"` (always use uppercase)
fn disk_byte(input: ParseInput) -> ParseResult<u8> {
    let (input, drive_letter) = drive_letter(input)?;

    let (input, _) = byte(b':')(input)?;
    Ok((input, drive_letter))
}

/// `"C:" -> "C"` and `"c:" -> "C"` (always use uppercase)
fn drive_letter(input: ParseInput) -> ParseResult<u8> {
    let (input, drive_letter) = take(1)(input)?;

    // Drive letter should ONLY be a-zA-Z
    if !(drive_letter[0] as char).is_alphabetic() {
        return Err("drive not alphabetic");
    }

    Ok((input, drive_letter[0].to_ascii_uppercase()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn get_prefix<'a, E: alloc::fmt::Display>(
        component: impl Into<Result<WindowsComponent<'a>, E>>,
    ) -> WindowsPrefix<'a> {
        match component.into() {
            Ok(WindowsComponent::Prefix(p)) => p.kind(),
            Ok(x) => panic!("not a prefix: {x:?}"),
            Err(x) => panic!("get_prefix(err({x}))"),
        }
    }

    #[test]
    fn should_support_parsing_single_component_from_front() {
        // Empty input fails
        Parser::new(b"").next_front().unwrap_err();

        // Supports parsing any component individually
        let mut parser = Parser::new(&[SEPARATOR as u8]);
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(&[ALT_SEPARATOR as u8]);
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(CURRENT_DIR);
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(PARENT_DIR);
        assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(b"hello");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"hello")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parsers = Parser::new(b"C:");
        assert_eq!(get_prefix(parsers.next_front()), WindowsPrefix::Disk(b'C'));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());
    }

    #[test]
    fn should_support_parsing_from_multiple_components_from_front() {
        // Empty input fails
        Parser::new(b"").next_front().unwrap_err();

        // Succeeds if finds a prefix
        let mut parser = Parser::new(br"\\server\share");
        assert_eq!(
            get_prefix(parser.next_front()),
            WindowsPrefix::UNC(b"server", b"share")
        );
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Succeeds if finds a root dir
        let mut parser = Parser::new(br"\");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Multiple separators still just mean root
        let mut parser = Parser::new(br"\\");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Succeeds even if there isn't a root or prefix
        //
        // E.g. a\b\c
        let mut parser = Parser::new(br"a\b\c");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should support '.' at beginning of path
        //
        // E.g. .\b\c
        let mut parser = Parser::new(br".\b\c");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should support '.' at beginning of path after a prefix as long as the
        // prefix does not imply that there is an implicit root
        //
        // implicit root is essentially every prefix except the normal drive
        //
        // E.g. C:. and C:.\ are okay to keep
        // E.g. \\?\C:. is verbatim mode, so the dot counts as well
        let mut parser = Parser::new(br"C:.");
        assert_eq!(get_prefix(parser.next_front()), WindowsPrefix::Disk(b'C'));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(br"C:.\");
        assert_eq!(get_prefix(parser.next_front()), WindowsPrefix::Disk(b'C'));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(br"\\?\C:.");
        assert_eq!(
            get_prefix(parser.next_front()),
            WindowsPrefix::VerbatimDisk(b'C')
        );
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should remove current dir from anywhere if not at beginning
        //
        // E.g. \.\b\c -> \b\c
        // E.g. a\.\c -> a\c
        let mut parser = Parser::new(br"\.\b\c");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        let mut parser = Parser::new(br"a\.\c");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Should strip multiple separators and normalize '.' if not starting with \\?\
        //
        // E.g. \\\\\a\\\.\\..\\\ -> [ROOT, "a", CURRENT_DIR, PARENT_DIR]
        let mut parser = Parser::new(br"\\\\\a\\\.\\..\\\");
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Prefix should come before root should come before path
        //
        // E.g. \\\\\a\\\.\\..\\\ -> [ROOT, "a", CURRENT_DIR, PARENT_DIR]
        let mut parser = Parser::new(br"C:\\\\\a\\\.\\..\\\");
        assert_eq!(get_prefix(parser.next_front()), WindowsPrefix::Disk(b'C'));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());

        // Verbatim path starting with \\?\ should retain . throughout path
        //
        // E.g. \\?\pictures\a\.\..\. -> [PREFIX, ROOT, "a", CURRENT_DIR, PARENT_DIR, CURRENT_DIR]
        let mut parser = Parser::new(br"\\?\pictures\a\.\..\.");
        assert_eq!(
            get_prefix(parser.next_front()),
            WindowsPrefix::Verbatim(b"pictures")
        );
        assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
        assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
        assert_eq!(parser.remaining(), b"");
        assert!(parser.next_front().is_err());
    }

    mod with_no_normalization {
        use super::*;

        #[test]
        fn should_support_verbatim_unc_prefix_retaining_current_directories_from_front() {
            let mut parser = Parser::new(br"\\?\UNC\server\share\.");

            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::VerbatimUNC(b"server", b"share")
            );
            assert_eq!(parser.remaining(), br"\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br".");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_verbatim_unc_prefix_retaining_current_directories_from_back() {
            let mut parser = Parser::new(br"\\?\UNC\server\share\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\UNC\server\share\");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"\\?\UNC\server\share");

            assert_eq!(
                get_prefix(parser.next_back()),
                WindowsPrefix::VerbatimUNC(b"server", b"share")
            );
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_verbatim_disk_prefix_retaining_current_directories_from_front() {
            let mut parser = Parser::new(br"\\?\C:.\.\.");

            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::VerbatimDisk(b'C')
            );
            assert_eq!(parser.remaining(), br".\.\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br".\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br".");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_verbatim_disk_prefix_retaining_current_directories_from_back() {
            let mut parser = Parser::new(br"\\?\C:.\.\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\C:.\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\C:.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\C:");

            assert_eq!(
                get_prefix(parser.next_back()),
                WindowsPrefix::VerbatimDisk(b'C')
            );
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_verbatim_prefix_retaining_current_directories_from_front() {
            let mut parser = Parser::new(br"\\?\pictures\.\.");

            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b"pictures")
            );
            assert_eq!(parser.remaining(), br"\.\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br".\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br".");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_verbatim_prefix_retaining_current_directories_from_back() {
            let mut parser = Parser::new(br"\\?\pictures\.\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\pictures\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"\\?\pictures\");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"\\?\pictures");

            assert_eq!(
                get_prefix(parser.next_back()),
                WindowsPrefix::Verbatim(b"pictures")
            );
            assert_eq!(parser.remaining(), br"");
        }
    }

    mod with_primary_separator {
        use super::*;

        #[test]
        fn should_support_zero_or_more_trailing_separators_from_front() {
            let mut parser = Parser::new(br"a\b\c\\\");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"b\c\\\");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"c\\\");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_zero_or_more_trailing_separators_from_back() {
            let mut parser = Parser::new(br"a\b\c\\\");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"a\b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_trailing_current_directory_from_front() {
            let mut parser = Parser::new(br"a\b\c\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"b\c\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"c\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_trailing_current_directory_from_back() {
            let mut parser = Parser::new(br"a\b\c\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"a\b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_relative_directory_from_front() {
            let mut parser = Parser::new(br"a\b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_relative_directory_from_back() {
            let mut parser = Parser::new(br"a\b\c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"a\b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_relative_directory_starting_with_current_directory_from_front() {
            let mut parser = Parser::new(br".\a\b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"a\b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_relative_directory_starting_with_current_directory_from_back() {
            let mut parser = Parser::new(br".\a\b\c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br".\a\b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br".\a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br".");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_root_directory_from_front() {
            let mut parser = Parser::new(br"\a\b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"a\b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"b\c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_root_directory_from_back() {
            let mut parser = Parser::new(br"\a\b\c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"c")));
            assert_eq!(parser.remaining(), br"\a\b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"b")));
            assert_eq!(parser.remaining(), br"\a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(br"a")));
            assert_eq!(parser.remaining(), br"\");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_normalizing_current_directories_from_front() {
            let mut parser = Parser::new(br"\.\.\.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_normalizing_current_directories_from_back() {
            let mut parser = Parser::new(br"\.\.\.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_parsing_just_root_directory_from_front() {
            let mut parser = Parser::new(br"\");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");

            // Also works with multiple separators
            let mut parser = Parser::new(br"\\");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_parsing_just_root_directory_from_back() {
            let mut parser = Parser::new(br"\");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");

            // Also works with multiple separators
            let mut parser = Parser::new(br"\\");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_parsing_parent_directories_from_front() {
            let mut parser = Parser::new(br"..");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), br"");

            // Supports back-to-back parent directories
            let mut parser = Parser::new(br"..\..");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), br"");
        }

        #[test]
        fn should_support_parsing_parent_directories_from_back() {
            let mut parser = Parser::new(br"..");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), br"");

            // Supports back-to-back parent directories
            let mut parser = Parser::new(br"..\..");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), br"");
        }
    }

    mod with_alternate_separator {
        use super::*;

        #[test]
        fn should_support_zero_or_more_trailing_separators_from_front() {
            let mut parser = Parser::new(b"a/b/c///");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"b/c///");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"c///");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_zero_or_more_trailing_separators_from_back() {
            let mut parser = Parser::new(b"a/b/c///");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"a/b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_trailing_current_directory_from_front() {
            let mut parser = Parser::new(b"a/b/c/.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"b/c/.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"c/.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_trailing_current_directory_from_back() {
            let mut parser = Parser::new(b"a/b/c/.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"a/b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_relative_directory_from_front() {
            let mut parser = Parser::new(b"a/b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_relative_directory_from_back() {
            let mut parser = Parser::new(b"a/b/c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"a/b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_relative_directory_starting_with_current_directory_from_front() {
            let mut parser = Parser::new(b"./a/b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), b"a/b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_relative_directory_starting_with_current_directory_from_back() {
            let mut parser = Parser::new(b"./a/b/c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"./a/b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"./a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b".");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_root_directory_from_front() {
            let mut parser = Parser::new(b"/a/b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"a/b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"b/c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"c");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_root_directory_from_back() {
            let mut parser = Parser::new(b"/a/b/c");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"c")));
            assert_eq!(parser.remaining(), b"/a/b");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"b")));
            assert_eq!(parser.remaining(), b"/a");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::Normal(b"a")));
            assert_eq!(parser.remaining(), b"/");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_normalizing_current_directories_from_front() {
            let mut parser = Parser::new(b"/././.");

            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_normalizing_current_directories_from_back() {
            let mut parser = Parser::new(b"/././.");

            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_parsing_just_root_directory_from_front() {
            let mut parser = Parser::new(b"/");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");

            // Also works with multiple separators
            let mut parser = Parser::new(b"//");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_parsing_just_root_directory_from_back() {
            let mut parser = Parser::new(b"/");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");

            // Also works with multiple separators
            let mut parser = Parser::new(b"//");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_parsing_parent_directories_from_front() {
            let mut parser = Parser::new(b"..");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), b"");

            // Supports back-to-back parent directories
            let mut parser = Parser::new(b"../..");
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), b"");
        }

        #[test]
        fn should_support_parsing_parent_directories_from_back() {
            let mut parser = Parser::new(b"..");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), b"");

            // Supports back-to-back parent directories
            let mut parser = Parser::new(b"../..");
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.next_back(), Ok(WindowsComponent::ParentDir));
            assert_eq!(parser.remaining(), b"");
        }
    }

    /// Tests to call out known variations from stdlib
    mod stdlib {
        use super::*;

        #[test]
        #[ignore]
        fn validate_rust_variation_1() {
            // Verbatim disk with something other than a trailing slash following is just verbatim
            //
            // CASE: \\?\C:. -> [Verbatim("C:.")]
            //
            // NOTE: This seems wrong to me as I would expect this to be verbatim with a current dir;
            //       so, I am not going to worry about this right now
            let mut parser = Parser::new(br"\\?\C:.");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b"C:.")
            );
            assert!(parser.next_front().is_err());
        }

        #[test]
        #[ignore]
        fn validate_rust_variation_2() {
            // Disk with a '.' following is just disk, not disk and current dir
            //
            // CASE: C:. -> [Disk('C')]
            //
            // NOTE: This seems wrong to me as I would expect this to include current dir since there
            //       is no root at the beginning; so, I am not going to worry about this right now
            let mut parser = Parser::new(b"C:.");
            assert_eq!(get_prefix(parser.next_front()), WindowsPrefix::Disk(b'C'));
            assert!(parser.next_front().is_err());
        }

        #[test]
        #[ignore]
        fn validate_rust_variation_3() {
            // Disk with '.' at beginning of path has the current dir removed
            //
            // NOTE: This seems wrong to me as I would expect this to include current dir since there
            //       is no root at the beginning
            let mut parser = Parser::new(br"C:.\hello");
            assert_eq!(get_prefix(parser.next_front()), WindowsPrefix::Disk(b'C'));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"hello")));
            assert!(parser.next_front().is_err());
        }

        #[test]
        fn validate_rust_variation_4() {
            // Verbatim mode can have an empty verbatim field if separators at the beginning
            //
            // NOTE: I'm not sure about this one. It seems like something incorrect, but the current
            //       parser implementation is outputting UNC when this fails
            let mut parser = Parser::new(br"\\?\\hello");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b"")
            );
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"hello")));
            assert!(parser.next_front().is_err());
        }

        #[test]
        fn validate_arbitrary_rules_to_match_std_lib() {
            // Support verbatim disk on its own
            let mut parser = Parser::new(br"\\?\C:");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::VerbatimDisk(b'C')
            );
            assert!(parser.next_front().is_err());

            // Support verbatim disk with root
            let mut parser = Parser::new(br"\\?\C:\");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::VerbatimDisk(b'C')
            );
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert!(parser.next_front().is_err());

            // Verbatim can have '.' as value
            let mut parser = Parser::new(br"\\?\.\hello\.\again");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b".")
            );
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"hello")));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"again")));
            assert!(parser.next_front().is_err());

            // Verbatim mode still removes extra separators
            let mut parser = Parser::new(br"\\?\.\\\hello\\\.\\\again");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b".")
            );
            assert_eq!(parser.next_front(), Ok(WindowsComponent::RootDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"hello")));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::CurDir));
            assert_eq!(parser.next_front(), Ok(WindowsComponent::Normal(b"again")));
            assert!(parser.next_front().is_err());

            // Verbatim UNC can have an optional share
            let mut parser = Parser::new(br"\\?\UNC\server");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::VerbatimUNC(b"server", b"")
            );
            assert!(parser.next_front().is_err());

            // Verbatim will include '/' as part of name
            let mut parser = Parser::new(br"\\?\some/name");
            assert_eq!(
                get_prefix(parser.next_front()),
                WindowsPrefix::Verbatim(b"some/name")
            );
            assert!(parser.next_front().is_err());
        }
    }

    mod helpers {
        use super::*;

        #[test]
        fn validate_move_front_to_next_non_normalized_for_primary_separator() {
            let move_front_to_next = move_front_to_next(false);

            let (input, _) = move_front_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".").unwrap();
            assert_eq!(input, br".");

            let (input, _) = move_front_to_next(br".\").unwrap();
            assert_eq!(input, br".\");

            let (input, _) = move_front_to_next(br".\.").unwrap();
            assert_eq!(input, br".\.");

            let (input, _) = move_front_to_next(br".\a").unwrap();
            assert_eq!(input, br".\a");

            let (input, _) = move_front_to_next(br".\\a").unwrap();
            assert_eq!(input, br".\\a");

            let (input, _) = move_front_to_next(br".\.\a").unwrap();
            assert_eq!(input, br".\.\a");

            let (input, _) = move_front_to_next(br".\.\..").unwrap();
            assert_eq!(input, br".\.\..");

            let (input, _) = move_front_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_front_to_next(br"..\.").unwrap();
            assert_eq!(input, br"..\.");

            let (input, _) = move_front_to_next(br"\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br"\.").unwrap();
            assert_eq!(input, br".");
        }

        #[test]
        fn validate_move_front_to_next_non_normalized_for_alternate_separator() {
            let move_front_to_next = move_front_to_next(false);

            let (input, _) = move_front_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".").unwrap();
            assert_eq!(input, br".");

            let (input, _) = move_front_to_next(br"./").unwrap();
            assert_eq!(input, br"./");

            let (input, _) = move_front_to_next(br"./.").unwrap();
            assert_eq!(input, br"./.");

            let (input, _) = move_front_to_next(br"./a").unwrap();
            assert_eq!(input, br"./a");

            let (input, _) = move_front_to_next(br".//a").unwrap();
            assert_eq!(input, br".//a");

            let (input, _) = move_front_to_next(br"././a").unwrap();
            assert_eq!(input, br"././a");

            let (input, _) = move_front_to_next(br"././..").unwrap();
            assert_eq!(input, br"././..");

            let (input, _) = move_front_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_front_to_next(br"../.").unwrap();
            assert_eq!(input, br"../.");

            let (input, _) = move_front_to_next(br"/").unwrap();
            assert_eq!(input, br"/");

            let (input, _) = move_front_to_next(br"/.").unwrap();
            assert_eq!(input, br"/.");
        }

        #[test]
        fn validate_move_front_to_next_normalized_for_primary_separator() {
            let move_front_to_next = move_front_to_next(true);

            let (input, _) = move_front_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".\.").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br".\a").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_front_to_next(br".\\a").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_front_to_next(br".\.\a").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_front_to_next(br".\.\..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_front_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_front_to_next(br"..\.").unwrap();
            assert_eq!(input, br"..\.");

            let (input, _) = move_front_to_next(br"\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_front_to_next(br"\.").unwrap();
            assert_eq!(input, br"");
        }

        #[test]
        fn validate_move_front_to_next_normalized_for_alternate_separator() {
            let move_front_to_next = move_front_to_next(true);

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
        fn validate_move_back_to_next_non_normalized_for_primary_separator() {
            let move_back_to_next = move_back_to_next(false);

            let (input, _) = move_back_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br".").unwrap();
            assert_eq!(input, br".");

            let (input, _) = move_back_to_next(br".\").unwrap();
            assert_eq!(input, br".");

            let (input, _) = move_back_to_next(br".\.").unwrap();
            assert_eq!(input, br".\.");

            let (input, _) = move_back_to_next(br"a\.").unwrap();
            assert_eq!(input, br"a\.");

            let (input, _) = move_back_to_next(br"a\\.").unwrap();
            assert_eq!(input, br"a\\.");

            let (input, _) = move_back_to_next(br"a\.\.").unwrap();
            assert_eq!(input, br"a\.\.");

            let (input, _) = move_back_to_next(br"..\.\.").unwrap();
            assert_eq!(input, br"..\.\.");

            let (input, _) = move_back_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_back_to_next(br".\..").unwrap();
            assert_eq!(input, br".\..");

            let (input, _) = move_back_to_next(br"\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br"\.").unwrap();
            assert_eq!(input, br"\.");
        }

        #[test]
        fn validate_move_back_to_next_non_normalized_for_alternate_separator() {
            let move_back_to_next = move_back_to_next(false);

            let (input, _) = move_back_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br".").unwrap();
            assert_eq!(input, br".");

            let (input, _) = move_back_to_next(br"./").unwrap();
            assert_eq!(input, br"./");

            let (input, _) = move_back_to_next(br"./.").unwrap();
            assert_eq!(input, br"./.");

            let (input, _) = move_back_to_next(br"./a").unwrap();
            assert_eq!(input, br"./a");

            let (input, _) = move_back_to_next(br".//a").unwrap();
            assert_eq!(input, br".//a");

            let (input, _) = move_back_to_next(br"././a").unwrap();
            assert_eq!(input, br"././a");

            let (input, _) = move_back_to_next(br"././..").unwrap();
            assert_eq!(input, br"././..");

            let (input, _) = move_back_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_back_to_next(br"../.").unwrap();
            assert_eq!(input, br"../.");

            let (input, _) = move_back_to_next(br"/").unwrap();
            assert_eq!(input, br"/");

            let (input, _) = move_back_to_next(br"/.").unwrap();
            assert_eq!(input, br"/.");
        }

        #[test]
        fn validate_move_back_to_next_normalized_for_primary_separator() {
            let move_back_to_next = move_back_to_next(true);

            let (input, _) = move_back_to_next(br"").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br".").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br".\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br".\.").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br"a\.").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_back_to_next(br"a\\.").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_back_to_next(br"a\.\.").unwrap();
            assert_eq!(input, br"a");

            let (input, _) = move_back_to_next(br"..\.\.").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_back_to_next(br"..").unwrap();
            assert_eq!(input, br"..");

            let (input, _) = move_back_to_next(br".\..").unwrap();
            assert_eq!(input, br".\..");

            let (input, _) = move_back_to_next(br"\").unwrap();
            assert_eq!(input, br"");

            let (input, _) = move_back_to_next(br"\.").unwrap();
            assert_eq!(input, br"");
        }

        #[test]
        fn validate_move_back_to_next_normalized_for_alternate_separator() {
            let move_back_to_next = move_back_to_next(true);

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
        fn validate_prefix_component() {
            // Empty input fails
            prefix_component(b"").unwrap_err();

            // Not starting with a prefix fails
            prefix_component(&[SEPARATOR as u8]).unwrap_err();

            // Should succeed if a disk
            let (input, value) = prefix_component(b"C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, b"C:");
            assert_eq!(value.parsed, WindowsPrefix::Disk(b'C'));

            // Should succeed if verbatim
            let (input, value) = prefix_component(br"\\?\pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, br"\\?\pictures");
            assert_eq!(value.parsed, WindowsPrefix::Verbatim(b"pictures"));

            // Should succeed if verbatim UNC
            let (input, value) = prefix_component(br"\\?\UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, br"\\?\UNC\server\share");
            assert_eq!(
                value.parsed,
                WindowsPrefix::VerbatimUNC(b"server", b"share")
            );

            // Should succeed if verbatim disk
            let (input, value) = prefix_component(br"\\?\C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, br"\\?\C:");
            assert_eq!(value.parsed, WindowsPrefix::VerbatimDisk(b'C'));

            // Should succeed if Device NS
            let (input, value) = prefix_component(br"\\.\BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, br"\\.\BrainInterface");
            assert_eq!(value.parsed, WindowsPrefix::DeviceNS(b"BrainInterface"));

            // Should succeed if UNC
            let (input, value) = prefix_component(br"\\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value.raw, br"\\server\share");
            assert_eq!(value.parsed, WindowsPrefix::UNC(b"server", b"share"));

            // Should set raw to only consumed portion of input
            let (input, value) = prefix_component(br"C:\path").unwrap();
            assert_eq!(input, br"\path");
            assert_eq!(value.raw, b"C:");
            assert_eq!(value.parsed, WindowsPrefix::Disk(b'C'));
        }

        #[test]
        fn validate_prefix() {
            // Empty input fails
            prefix(b"").unwrap_err();

            // Not starting with a prefix fails
            prefix(&[SEPARATOR as u8]).unwrap_err();

            // Should succeed if a disk
            let (input, value) = prefix(b"C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Disk(b'C'));

            // Should succeed if verbatim
            let (input, value) = prefix(br"\\?\pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));

            // Should succeed if verbatim UNC
            let (input, value) = prefix(br"\\?\UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));

            // Should succeed if verbatim disk
            let (input, value) = prefix(br"\\?\C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            // Should succeed if device NS
            let (input, value) = prefix(br"\\.\BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));

            // Should succeed if UNC
            let (input, value) = prefix(br"\\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));
        }

        #[test]
        fn validate_prefix_verbatim_unc() {
            // Empty input fails
            prefix_verbatim_unc(b"").unwrap_err();

            // Not starting with appropriate character set
            prefix_verbatim_unc(br"server\share").unwrap_err();
            prefix_verbatim_unc(br"\server\share").unwrap_err();
            prefix_verbatim_unc(br"\\server\share").unwrap_err();
            prefix_verbatim_unc(br"\\?server\share").unwrap_err();
            prefix_verbatim_unc(br"\?\server\share").unwrap_err();
            prefix_verbatim_unc(br"?\\server\share").unwrap_err();
            prefix_verbatim_unc(br"?\\?\server\share").unwrap_err();
            prefix_verbatim_unc(br"\\?\UNCserver\share").unwrap_err();
            prefix_verbatim_unc(br"\?\UNC\server\share").unwrap_err();
            prefix_verbatim_unc(br"\\.\UNC\server\share").unwrap_err();
            prefix_verbatim_unc(br"\\?\UN\server\share").unwrap_err();

            // Fails if not verbatim type (other forms of verbatim)
            prefix_verbatim_unc(br"\\?\C:").unwrap_err();
            prefix_verbatim_unc(br"\\?\pictures").unwrap_err();

            // Supports both primary and alternate separators UNLESS prefixed with EXACTLY \\?\
            // in which case it only parses '\' as the separator
            let (input, value) = prefix_verbatim_unc(br"\\?/UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));

            let (input, value) = prefix_verbatim_unc(br"\/?\UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));

            let (input, value) = prefix_verbatim_unc(br"/\?\UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));

            // Verify that \\?\ causes the exact match requirement
            let (input, value) = prefix_verbatim_unc(br"\\?\UNC\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));

            let (input, value) = prefix_verbatim_unc(br"\\?\UNC\server/share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server/share", b""));

            let (input, value) = prefix_verbatim_unc(br"\\?\UNC\server/share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server/share", b""));

            // Share is optional
            let (input, value) = prefix_verbatim_unc(br"\\?\UNC\server").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b""));

            // These will NOT parse correctly because of the \\?\ prefix forcing UNC\
            prefix_verbatim_unc(br"\\?\UNC/server\share").unwrap_err();
            prefix_verbatim_unc(br"\\?\UNC/server/share").unwrap_err();

            // Consumes up to the next separator
            let (input, value) = prefix_verbatim_unc(br"\\?\UNC\server\share\hello").unwrap();
            assert_eq!(input, br"\hello");
            assert_eq!(value, WindowsPrefix::VerbatimUNC(b"server", b"share"));
        }

        #[test]
        fn validate_prefix_verbatim() {
            // Empty input fails
            prefix_verbatim(b"").unwrap_err();

            // Not starting with appropriate character set
            prefix_verbatim(br"pictures").unwrap_err();
            prefix_verbatim(br"\pictures").unwrap_err();
            prefix_verbatim(br"\\pictures").unwrap_err();
            prefix_verbatim(br"\\?pictures").unwrap_err();
            prefix_verbatim(br"\?\pictures").unwrap_err();
            prefix_verbatim(br"?\\pictures").unwrap_err();
            prefix_verbatim(br"?\\?\pictures").unwrap_err();

            // Fails if not verbatim type (other forms of verbatim)
            prefix_verbatim(br"\\?\C:").unwrap_err();
            prefix_verbatim(br"\\?\UNC\server\share").unwrap_err();

            // Supports both primary and alternate separators
            let (input, value) = prefix_verbatim(br"\\?\pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));

            let (input, value) = prefix_verbatim(br"\\?/pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));

            let (input, value) = prefix_verbatim(br"\/?\pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));

            let (input, value) = prefix_verbatim(br"/\?\pictures").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));

            // Consumes up to the next separator
            let (input, value) = prefix_verbatim(br"\\?\pictures\hello").unwrap();
            assert_eq!(input, br"\hello");
            assert_eq!(value, WindowsPrefix::Verbatim(b"pictures"));
        }

        #[test]
        fn validate_prefix_verbatim_disk() {
            // Empty input fails
            prefix_verbatim_disk(b"").unwrap_err();

            // Not starting with appropriate character set
            prefix_verbatim_disk(br"C:").unwrap_err();
            prefix_verbatim_disk(br"\C:").unwrap_err();
            prefix_verbatim_disk(br"\\C:").unwrap_err();
            prefix_verbatim_disk(br"\\?C:").unwrap_err();
            prefix_verbatim_disk(br"\?\C:").unwrap_err();
            prefix_verbatim_disk(br"?\\C:").unwrap_err();
            prefix_verbatim_disk(br"?\\?\C:").unwrap_err();

            // Fails if not a drive letter (other forms of verbatim)
            prefix_verbatim_disk(br"\\?\pictures").unwrap_err();
            prefix_verbatim_disk(br"\\?\UNC\server\share").unwrap_err();

            // Supports both primary and alternate separators
            let (input, value) = prefix_verbatim_disk(br"\\?\C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            let (input, value) = prefix_verbatim_disk(br"\\?/C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            let (input, value) = prefix_verbatim_disk(br"\/?\C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            let (input, value) = prefix_verbatim_disk(br"/\?\C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            // Consumes up to the drive and :
            let (input, value) = prefix_verbatim_disk(br"\\?\C:.").unwrap();
            assert_eq!(input, b".");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));

            let (input, value) = prefix_verbatim_disk(br"\\?\C:\").unwrap();
            assert_eq!(input, br"\");
            assert_eq!(value, WindowsPrefix::VerbatimDisk(b'C'));
        }

        #[test]
        fn validate_prefix_device_ns() {
            // Empty input fails
            prefix_device_ns(b"").unwrap_err();

            // Not starting with appropriate character set
            prefix_device_ns(br"BrainInterface").unwrap_err();
            prefix_device_ns(br"\BrainInterface").unwrap_err();
            prefix_device_ns(br"\\BrainInterface").unwrap_err();
            prefix_device_ns(br"\\.BrainInterface").unwrap_err();
            prefix_device_ns(br"\.\BrainInterface").unwrap_err();
            prefix_device_ns(br".\\BrainInterface").unwrap_err();
            prefix_device_ns(br".\\.\BrainInterface").unwrap_err();

            // Supports both primary and alternate separators
            let (input, value) = prefix_device_ns(br"\\.\BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));

            let (input, value) = prefix_device_ns(br"\\./BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));

            let (input, value) = prefix_device_ns(br"\/.\BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));

            let (input, value) = prefix_device_ns(br"/\.\BrainInterface").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));

            // Consumes up to the next separator
            let (input, value) = prefix_device_ns(br"\\.\BrainInterface\hello").unwrap();
            assert_eq!(input, br"\hello");
            assert_eq!(value, WindowsPrefix::DeviceNS(b"BrainInterface"));
        }

        #[test]
        fn validate_prefix_unc() {
            // Empty input fails
            prefix_unc(b"").unwrap_err();

            // Not starting with two separators ('\\', '\/', '//', '/\')
            prefix_unc(br"server\share").unwrap_err();
            prefix_unc(br"\server\share").unwrap_err();

            // Supports parsing just the server and not the share
            let (input, value) = prefix_unc(br"\\server").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b""));

            let (input, value) = prefix_unc(br"\\server\").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b""));

            // Supports both primary and alternate separators
            let (input, value) = prefix_unc(br"\\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));

            let (input, value) = prefix_unc(br"\\server/share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));

            let (input, value) = prefix_unc(br"\/server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));

            let (input, value) = prefix_unc(br"/\server\share").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));

            // Consumes up to the next separator
            let (input, value) = prefix_unc(br"\\server\share\hello").unwrap();
            assert_eq!(input, br"\hello");
            assert_eq!(value, WindowsPrefix::UNC(b"server", b"share"));
        }

        #[test]
        fn validate_prefix_disk() {
            // Empty input fails
            prefix_disk(b"").unwrap_err();

            // Not starting with a drive letter fails
            prefix_disk(b"1:").unwrap_err();

            // Not ending with : fails
            prefix_disk(b"C").unwrap_err();
            prefix_disk(b"CC").unwrap_err();

            // Supports uppercase alphabet
            let (input, value) = prefix_disk(b"C:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Disk(b'C'));

            // Supports lowercase alphabet being converted to uppercase
            let (input, value) = prefix_disk(b"c:").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsPrefix::Disk(b'C'));

            // Consumes only drive letter and :
            let (input, value) = prefix_disk(br"C:\path").unwrap();
            assert_eq!(input, br"\path");
            assert_eq!(value, WindowsPrefix::Disk(b'C'));
        }

        #[test]
        fn validate_root_dir() {
            let root_dir = root_dir(true);

            // Empty input fails
            root_dir(b"").unwrap_err();

            // Not starting with root dir fails
            root_dir(&[b'a', SEPARATOR as u8]).unwrap_err();

            // Succeeds just on its own
            let (input, value) = root_dir(&[SEPARATOR as u8]).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::RootDir);

            // Succeeds, taking only what it matches
            let (input, value) = root_dir(&[SEPARATOR as u8, b'a', SEPARATOR as u8]).unwrap();
            assert_eq!(input, &[b'a', SEPARATOR as u8]);
            assert_eq!(value, WindowsComponent::RootDir);
        }

        #[test]
        fn validate_cur_dir_normalized() {
            let cur_dir = cur_dir(true);

            // Empty input fails
            cur_dir(b"").unwrap_err();

            // Not starting with current dir fails
            cur_dir(&[b"a", CURRENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = cur_dir(CURRENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::CurDir);

            // Fails if more content after itself that is not a separator
            // E.g. .. will fail, .a will fail
            cur_dir(&[CURRENT_DIR, b"."].concat()).unwrap_err();
            cur_dir(&[CURRENT_DIR, b"a"].concat()).unwrap_err();

            // Succeeds, taking up to primary separator
            let input = &[CURRENT_DIR, br"\", CURRENT_DIR].concat();
            let (input, value) = cur_dir(input).unwrap();
            assert_eq!(input, &[br"\", CURRENT_DIR].concat());
            assert_eq!(value, WindowsComponent::CurDir);

            // Succeeds, taking up to alternate separator
            let input = &[CURRENT_DIR, br"\", CURRENT_DIR].concat();
            let (input, value) = cur_dir(input).unwrap();
            assert_eq!(input, &[br"\", CURRENT_DIR].concat());
            assert_eq!(value, WindowsComponent::CurDir);
        }

        #[test]
        fn validate_cur_dir_non_normalized() {
            let cur_dir = cur_dir(false);

            // Empty input fails
            cur_dir(b"").unwrap_err();

            // Not starting with current dir fails
            cur_dir(&[b"a", CURRENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = cur_dir(CURRENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::CurDir);

            // Fails if more content after itself that is not a separator
            // E.g. .. will fail, .a will fail
            cur_dir(&[CURRENT_DIR, b"."].concat()).unwrap_err();
            cur_dir(&[CURRENT_DIR, b"a"].concat()).unwrap_err();

            // Also fails with alternate separator
            cur_dir(&[CURRENT_DIR, b"/"].concat()).unwrap_err();

            // Succeeds, taking up to primary separator
            let input = &[CURRENT_DIR, br"\", CURRENT_DIR].concat();
            let (input, value) = cur_dir(input).unwrap();
            assert_eq!(input, &[br"\", CURRENT_DIR].concat());
            assert_eq!(value, WindowsComponent::CurDir);
        }

        #[test]
        fn validate_parent_dir_normalized() {
            let parent_dir = parent_dir(true);

            // Empty input fails
            parent_dir(b"").unwrap_err();

            // Not starting with parent dir fails
            parent_dir(&[b"a", PARENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = parent_dir(PARENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::ParentDir);

            // Fails if more content after itself that is not a separator
            // E.g. ... will fail, ..a will fail
            parent_dir(&[PARENT_DIR, b"."].concat()).unwrap_err();
            parent_dir(&[PARENT_DIR, b"a"].concat()).unwrap_err();

            // Succeeds, taking up to primary separator
            let input = &[PARENT_DIR, br"\", PARENT_DIR].concat();
            let (input, value) = parent_dir(input).unwrap();
            assert_eq!(input, &[br"\", PARENT_DIR].concat());
            assert_eq!(value, WindowsComponent::ParentDir);

            // Succeeds, taking up to alternate separator
            let input = &[PARENT_DIR, br"/", PARENT_DIR].concat();
            let (input, value) = parent_dir(input).unwrap();
            assert_eq!(input, &[br"/", PARENT_DIR].concat());
            assert_eq!(value, WindowsComponent::ParentDir);
        }

        #[test]
        fn validate_parent_dir_non_normalized() {
            let parent_dir = parent_dir(false);

            // Empty input fails
            parent_dir(b"").unwrap_err();

            // Not starting with parent dir fails
            parent_dir(&[b"a", PARENT_DIR].concat()).unwrap_err();

            // Succeeds just on its own
            let (input, value) = parent_dir(PARENT_DIR).unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::ParentDir);

            // Fails if more content after itself that is not a separator
            // E.g. ... will fail, ..a will fail
            parent_dir(&[PARENT_DIR, b"."].concat()).unwrap_err();
            parent_dir(&[PARENT_DIR, b"a"].concat()).unwrap_err();

            // Also fails with alternate separator
            parent_dir(&[PARENT_DIR, b"/"].concat()).unwrap_err();

            // Succeeds, taking up to primary separator
            let input = &[PARENT_DIR, br"\", PARENT_DIR].concat();
            let (input, value) = parent_dir(input).unwrap();
            assert_eq!(input, &[br"\", PARENT_DIR].concat());
            assert_eq!(value, WindowsComponent::ParentDir);
        }

        #[test]
        fn validate_normal() {
            let normal = normal(true);

            // Empty input fails
            normal(b"").unwrap_err();

            // Fails if takes nothing
            normal(br"\a").unwrap_err();

            // Succeeds just on its own
            let (input, value) = normal(b"hello").unwrap();
            assert_eq!(input, b"");
            assert_eq!(value, WindowsComponent::Normal(b"hello"));

            // Succeeds, taking up to the next separator
            let (input, value) = normal(br"abc\").unwrap();
            assert_eq!(input, br"\");
            assert_eq!(value, WindowsComponent::Normal(b"abc"));

            let (input, value) = normal(br"abc/").unwrap();
            assert_eq!(input, br"/");
            assert_eq!(value, WindowsComponent::Normal(b"abc"));

            let (input, value) = normal(br"abc\").unwrap();
            assert_eq!(input, br"\");
            assert_eq!(value, WindowsComponent::Normal(b"abc"));

            let (input, value) = normal(br"abc/").unwrap();
            assert_eq!(input, b"/");
            assert_eq!(value, WindowsComponent::Normal(b"abc"));
        }

        #[test]
        fn validate_separator() {
            // Empty input fails
            separator(true)(b"").unwrap_err();
            separator(false)(b"").unwrap_err();

            // Not starting with separator fails
            separator(true)(&[b'a', SEPARATOR as u8]).unwrap_err();
            separator(false)(&[b'a', SEPARATOR as u8]).unwrap_err();

            // Succeeds just on its own with primary when normalizing
            let (input, _) = separator(true)(&[SEPARATOR as u8]).unwrap();
            assert_eq!(input, b"");

            // Succeeds just on its own with alternate when normalizing
            let (input, _) = separator(true)(&[ALT_SEPARATOR as u8]).unwrap();
            assert_eq!(input, b"");

            // Fails if not normalizing and reading an alternate separator
            let _ = separator(false)(&[ALT_SEPARATOR as u8]).unwrap_err();

            // Succeeds, taking only one separator
            let input = &[SEPARATOR as u8, SEPARATOR as u8];
            let (input, _) = separator(true)(input).unwrap();
            assert_eq!(input, &[SEPARATOR as u8]);

            // Succeeds, taking only one alternate separator
            let input = &[ALT_SEPARATOR as u8, ALT_SEPARATOR as u8];
            let (input, _) = separator(true)(input).unwrap();
            assert_eq!(input, &[ALT_SEPARATOR as u8]);
        }
    }
}
