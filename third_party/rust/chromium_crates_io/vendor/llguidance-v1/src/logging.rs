use std::fmt::Write;

pub struct Logger {
    effective_level: u32,
    buffer_level: u32,
    stderr_level: u32,
    buffer: String,
}

pub struct InfoLogger<'a> {
    logger: &'a mut Logger,
}

pub struct WarningLogger<'a> {
    logger: &'a mut Logger,
}

impl Clone for Logger {
    fn clone(&self) -> Self {
        Self {
            effective_level: self.effective_level,
            buffer_level: self.buffer_level,
            stderr_level: self.stderr_level,
            buffer: String::new(), // clean logs on clone
        }
    }
}

impl Logger {
    // The buffer_level is used to determine the level of messages that are stored
    // in a buffer in the sequence object. These can be read and sent back to the user
    // over network etc. Note that some client libraries will look in these logs
    // for return values from the parser. If you support sending these back, set it to 2.
    //
    // The stderr_level is just for printing stuff out on stderr.
    // 0 - no output
    // 1 - warnings only
    // 2 - info output
    pub fn new(buffer_level: u32, stderr_level: u32) -> Self {
        Self {
            buffer_level,
            stderr_level,
            effective_level: std::cmp::max(buffer_level, stderr_level),
            buffer: String::new(),
        }
    }

    pub fn warn(&mut self, s: &str) {
        if self.level_enabled(1) {
            self.write_warning("Warning: ");
            self.write_warning(s);
            self.write_warning("\n");
        }
    }

    pub fn info(&mut self, s: &str) {
        if self.level_enabled(2) {
            self.write_info(s);
            self.write_info("\n");
        }
    }

    pub fn write_warning(&mut self, s: &str) {
        self.write_level(1, s);
    }

    pub fn write_info(&mut self, s: &str) {
        self.write_level(2, s);
    }

    pub fn write_buffer(&mut self, s: &str) {
        self.buffer.push_str(s);
    }

    #[inline(always)]
    pub fn write_level(&mut self, level: u32, s: &str) {
        if self.buffer_level >= level {
            self.buffer.push_str(s);
        }
        if self.stderr_level >= level {
            eprint!("{}", s);
        }
    }

    #[inline(always)]
    pub fn level_enabled(&self, level: u32) -> bool {
        level <= self.effective_level
    }

    #[inline(always)]
    pub fn effective_level(&self) -> u32 {
        self.effective_level
    }

    #[inline(always)]
    pub fn buffer_level(&self) -> u32 {
        self.buffer_level
    }

    #[inline(always)]
    pub fn stderr_level(&self) -> u32 {
        self.stderr_level
    }

    pub fn set_buffer_level(&mut self, buffer_level: u32) {
        self.buffer_level = buffer_level;
        self.effective_level = std::cmp::max(self.effective_level, self.buffer_level);
    }

    pub fn set_stderr_level(&mut self, stderr_level: u32) {
        self.stderr_level = stderr_level;
        self.effective_level = std::cmp::max(self.effective_level, self.stderr_level);
    }

    pub fn get_buffer(&self) -> &str {
        &self.buffer
    }

    pub fn get_and_clear_logs(&mut self) -> String {
        std::mem::take(&mut self.buffer)
    }

    pub fn info_logger(&mut self) -> InfoLogger {
        InfoLogger { logger: self }
    }

    pub fn warning_logger(&mut self) -> WarningLogger {
        WarningLogger { logger: self }
    }
}

impl Write for InfoLogger<'_> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.logger.write_info(s);
        Ok(())
    }
}

impl Write for WarningLogger<'_> {
    fn write_str(&mut self, s: &str) -> std::fmt::Result {
        self.logger.write_warning(s);
        Ok(())
    }
}
