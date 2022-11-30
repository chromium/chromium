use super::core::display_width;

#[derive(Debug)]
pub(crate) struct LineWrapper {
    line_width: usize,
    hard_width: usize,
}

impl LineWrapper {
    pub(crate) fn new(hard_width: usize) -> Self {
        Self {
            line_width: 0,
            hard_width,
        }
    }

    pub(crate) fn reset(&mut self) {
        self.line_width = 0;
    }

    pub(crate) fn wrap<'w>(&mut self, mut words: Vec<&'w str>) -> Vec<&'w str> {
        let mut i = 0;
        while i < words.len() {
            let word = &words[i];
            let trimmed = word.trim_end();
            let word_width = display_width(trimmed);
            let trimmed_delta = word.len() - trimmed.len();
            if i != 0 && self.hard_width < self.line_width + word_width {
                if 0 < i {
                    let last = i - 1;
                    let trimmed = words[last].trim_end();
                    words[last] = trimmed;
                }
                words.insert(i, "\n");
                i += 1;
                self.reset();
            }
            self.line_width += word_width + trimmed_delta;

            i += 1;
        }
        words
    }
}
