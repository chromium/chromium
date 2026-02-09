pub(crate) fn get_timezone_inner() -> Result<String, crate::GetTimezoneError> {
    std::env::var("TZ").or_else(|_| Ok("Etc/UTC".to_owned()))
}
