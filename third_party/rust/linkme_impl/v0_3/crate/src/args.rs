use syn::parse::{Error, Parse, ParseStream, Result};
use syn::{LitInt, Path, Token};

pub enum Args {
    None,
    Path(Path),
    PathPos(Path, usize),
}

impl Parse for Args {
    fn parse(input: ParseStream) -> Result<Self> {
        if input.is_empty() {
            return Ok(Args::None);
        }
        let path: Path = input.parse()?;
        if input.is_empty() {
            return Ok(Args::Path(path));
        }
        input.parse::<Token![,]>()?;
        let lit: LitInt = input.parse()?;
        let pos: usize = lit.base10_parse()?;
        if pos > 9999 {
            return Err(Error::new(lit.span(), "maximum 9999 is supported"));
        }
        Ok(Args::PathPos(path, pos))
    }
}
