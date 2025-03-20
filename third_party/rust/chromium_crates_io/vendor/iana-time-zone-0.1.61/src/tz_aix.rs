use std::fs::OpenOptions;
use std::io::{BufRead, BufReader};
use std::env;

pub(crate) fn get_timezone_inner() -> Result<String, crate::GetTimezoneError> {
    env::var("TZ").map_err(|_| crate::GetTimezoneError::OsError)
}

fn read_environment() -> Result<String, crate::GetTimezoneError> {
    // https://www.ibm.com/docs/en/aix/7.2?topic=files-environment-file

    let file = OpenOptions::new().read(true).open("/etc/environment")?;
    let mut reader = BufReader::new(file);
    let mut line = String::with_capacity(80);
    loop {
        line.clear();
        let count = reader.read_line(&mut line)?;
        if count == 0 {
            return Err(crate::GetTimezoneError::FailedParsingString);
        } else if line.starts_with("TZ=") {
            line.truncate(line.trim_end().len());
            line.replace_range(..3, "");
            return Ok(line);
        }
    }
}
