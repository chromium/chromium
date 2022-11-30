#![cfg_attr(not(feature = "usage"), allow(unused_imports))]
#![cfg_attr(not(feature = "usage"), allow(unused_variables))]
#![cfg_attr(not(feature = "usage"), allow(clippy::manual_map))]
#![cfg_attr(not(feature = "usage"), allow(dead_code))]

// Internal
use crate::builder::StyledStr;
use crate::builder::{ArgPredicate, Command};
use crate::parser::ArgMatcher;
use crate::util::ChildGraph;
use crate::util::FlatSet;
use crate::util::Id;

static DEFAULT_SUB_VALUE_NAME: &str = "COMMAND";

pub(crate) struct Usage<'cmd> {
    cmd: &'cmd Command,
    required: Option<&'cmd ChildGraph<Id>>,
}

impl<'cmd> Usage<'cmd> {
    pub(crate) fn new(cmd: &'cmd Command) -> Self {
        Usage {
            cmd,
            required: None,
        }
    }

    pub(crate) fn required(mut self, required: &'cmd ChildGraph<Id>) -> Self {
        self.required = Some(required);
        self
    }

    // Creates a usage string for display. This happens just after all arguments were parsed, but before
    // any subcommands have been parsed (so as to give subcommands their own usage recursively)
    pub(crate) fn create_usage_with_title(&self, used: &[Id]) -> Option<StyledStr> {
        debug!("Usage::create_usage_with_title");
        let usage = some!(self.create_usage_no_title(used));

        let mut styled = StyledStr::new();
        styled.header("Usage:");
        styled.none(" ");
        styled.extend(usage.into_iter());
        Some(styled)
    }

    // Creates a usage string (*without title*) if one was not provided by the user manually.
    pub(crate) fn create_usage_no_title(&self, used: &[Id]) -> Option<StyledStr> {
        debug!("Usage::create_usage_no_title");
        if let Some(u) = self.cmd.get_override_usage() {
            Some(u.clone())
        } else {
            #[cfg(feature = "usage")]
            {
                if used.is_empty() {
                    Some(self.create_help_usage(true))
                } else {
                    Some(self.create_smart_usage(used))
                }
            }

            #[cfg(not(feature = "usage"))]
            {
                None
            }
        }
    }
}

#[cfg(feature = "usage")]
impl<'cmd> Usage<'cmd> {
    // Creates a usage string for display in help messages (i.e. not for errors)
    fn create_help_usage(&self, incl_reqs: bool) -> StyledStr {
        debug!("Usage::create_help_usage; incl_reqs={:?}", incl_reqs);
        let mut styled = StyledStr::new();
        let name = self
            .cmd
            .get_usage_name()
            .or_else(|| self.cmd.get_bin_name())
            .unwrap_or_else(|| self.cmd.get_name());
        styled.literal(name);

        if self.needs_options_tag() {
            styled.placeholder(" [OPTIONS]");
        }

        self.write_args(&[], !incl_reqs, &mut styled);

        // incl_reqs is only false when this function is called recursively
        if self.cmd.has_visible_subcommands() && incl_reqs
            || self.cmd.is_allow_external_subcommands_set()
        {
            let placeholder = self
                .cmd
                .get_subcommand_value_name()
                .unwrap_or(DEFAULT_SUB_VALUE_NAME);
            if self.cmd.is_subcommand_negates_reqs_set()
                || self.cmd.is_args_conflicts_with_subcommands_set()
            {
                styled.none("\n");
                styled.none("       ");
                if self.cmd.is_args_conflicts_with_subcommands_set() {
                    // Short-circuit full usage creation since no args will be relevant
                    styled.literal(name);
                } else {
                    styled.extend(self.create_help_usage(false).into_iter());
                }
                styled.placeholder(" <");
                styled.placeholder(placeholder);
                styled.placeholder(">");
            } else if self.cmd.is_subcommand_required_set() {
                styled.placeholder(" <");
                styled.placeholder(placeholder);
                styled.placeholder(">");
            } else {
                styled.placeholder(" [");
                styled.placeholder(placeholder);
                styled.placeholder("]");
            }
        }
        styled.trim();
        debug!("Usage::create_help_usage: usage={}", styled);
        styled
    }

    // Creates a context aware usage string, or "smart usage" from currently used
    // args, and requirements
    fn create_smart_usage(&self, used: &[Id]) -> StyledStr {
        debug!("Usage::create_smart_usage");
        let mut styled = StyledStr::new();

        styled.literal(
            self.cmd
                .get_usage_name()
                .or_else(|| self.cmd.get_bin_name())
                .unwrap_or_else(|| self.cmd.get_name()),
        );

        self.write_args(used, false, &mut styled);

        if self.cmd.is_subcommand_required_set() {
            styled.placeholder(" <");
            styled.placeholder(
                self.cmd
                    .get_subcommand_value_name()
                    .unwrap_or(DEFAULT_SUB_VALUE_NAME),
            );
            styled.placeholder(">");
        }
        styled
    }

    // Determines if we need the `[OPTIONS]` tag in the usage string
    fn needs_options_tag(&self) -> bool {
        debug!("Usage::needs_options_tag");
        'outer: for f in self.cmd.get_non_positionals() {
            debug!("Usage::needs_options_tag:iter: f={}", f.get_id());

            // Don't print `[OPTIONS]` just for help or version
            if f.get_long() == Some("help") || f.get_long() == Some("version") {
                debug!("Usage::needs_options_tag:iter Option is built-in");
                continue;
            }

            if f.is_hide_set() {
                debug!("Usage::needs_options_tag:iter Option is hidden");
                continue;
            }
            if f.is_required_set() {
                debug!("Usage::needs_options_tag:iter Option is required");
                continue;
            }
            for grp_s in self.cmd.groups_for_arg(f.get_id()) {
                debug!("Usage::needs_options_tag:iter:iter: grp_s={:?}", grp_s);
                if self.cmd.get_groups().any(|g| g.id == grp_s && g.required) {
                    debug!("Usage::needs_options_tag:iter:iter: Group is required");
                    continue 'outer;
                }
            }

            debug!("Usage::needs_options_tag:iter: [OPTIONS] required");
            return true;
        }

        debug!("Usage::needs_options_tag: [OPTIONS] not required");
        false
    }

    // Returns the required args in usage string form by fully unrolling all groups
    pub(crate) fn write_args(&self, incls: &[Id], force_optional: bool, styled: &mut StyledStr) {
        for required in self.get_args(incls, force_optional) {
            styled.none(" ");
            styled.extend(required.into_iter());
        }
    }

    pub(crate) fn get_args(&self, incls: &[Id], force_optional: bool) -> Vec<StyledStr> {
        debug!("Usage::get_args: incls={:?}", incls,);

        let required_owned;
        let required = if let Some(required) = self.required {
            required
        } else {
            required_owned = self.cmd.required_graph();
            &required_owned
        };

        let mut unrolled_reqs = Vec::new();
        for a in required.iter() {
            let is_relevant = |(val, req_arg): &(ArgPredicate, Id)| -> Option<Id> {
                let required = match val {
                    ArgPredicate::Equals(_) => false,
                    ArgPredicate::IsPresent => true,
                };
                required.then(|| req_arg.clone())
            };

            for aa in self.cmd.unroll_arg_requires(is_relevant, a) {
                // if we don't check for duplicates here this causes duplicate error messages
                // see https://github.com/clap-rs/clap/issues/2770
                unrolled_reqs.push(aa);
            }
            // always include the required arg itself. it will not be enumerated
            // by unroll_requirements_for_arg.
            unrolled_reqs.push(a.clone());
        }
        debug!("Usage::get_args: unrolled_reqs={:?}", unrolled_reqs);

        let mut required_groups_members = FlatSet::new();
        let mut required_groups = FlatSet::new();
        for req in unrolled_reqs.iter().chain(incls.iter()) {
            if self.cmd.find_group(req).is_some() {
                let group_members = self.cmd.unroll_args_in_group(req);
                let elem = self.cmd.format_group(req);
                required_groups.insert(elem);
                required_groups_members.extend(group_members);
            } else {
                debug_assert!(self.cmd.find(req).is_some());
            }
        }

        let mut required_opts = FlatSet::new();
        let mut required_positionals = Vec::new();
        for req in unrolled_reqs.iter().chain(incls.iter()) {
            if let Some(arg) = self.cmd.find(req) {
                if required_groups_members.contains(arg.get_id()) {
                    continue;
                }

                let stylized = arg.stylized(Some(!force_optional));
                if let Some(index) = arg.get_index() {
                    let new_len = index + 1;
                    if required_positionals.len() < new_len {
                        required_positionals.resize(new_len, None);
                    }
                    required_positionals[index] = Some(stylized);
                } else {
                    required_opts.insert(stylized);
                }
            } else {
                debug_assert!(self.cmd.find_group(req).is_some());
            }
        }

        for pos in self.cmd.get_positionals() {
            if pos.is_hide_set() {
                continue;
            }
            if required_groups_members.contains(pos.get_id()) {
                continue;
            }

            let index = pos.get_index().unwrap();
            let new_len = index + 1;
            if required_positionals.len() < new_len {
                required_positionals.resize(new_len, None);
            }
            if required_positionals[index].is_some() {
                if pos.is_last_set() {
                    let styled = required_positionals[index].take().unwrap();
                    let mut new = StyledStr::new();
                    new.literal("-- ");
                    new.extend(styled.into_iter());
                    required_positionals[index] = Some(new);
                }
            } else {
                let mut styled;
                if pos.is_last_set() {
                    styled = StyledStr::new();
                    styled.literal("[-- ");
                    styled.extend(pos.stylized(Some(true)).into_iter());
                    styled.literal("]");
                } else {
                    styled = pos.stylized(Some(false));
                }
                required_positionals[index] = Some(styled);
            }
            if pos.is_last_set() && force_optional {
                required_positionals[index] = None;
            }
        }

        let mut ret_val = Vec::new();
        if !force_optional {
            ret_val.extend(required_opts);
            ret_val.extend(required_groups);
        }
        for pos in required_positionals.into_iter().flatten() {
            ret_val.push(pos);
        }

        debug!("Usage::get_args: ret_val={:?}", ret_val);
        ret_val
    }

    pub(crate) fn get_required_usage_from(
        &self,
        incls: &[Id],
        matcher: Option<&ArgMatcher>,
        incl_last: bool,
    ) -> Vec<StyledStr> {
        debug!(
            "Usage::get_required_usage_from: incls={:?}, matcher={:?}, incl_last={:?}",
            incls,
            matcher.is_some(),
            incl_last
        );

        let required_owned;
        let required = if let Some(required) = self.required {
            required
        } else {
            required_owned = self.cmd.required_graph();
            &required_owned
        };

        let mut unrolled_reqs = Vec::new();
        for a in required.iter() {
            let is_relevant = |(val, req_arg): &(ArgPredicate, Id)| -> Option<Id> {
                let required = match val {
                    ArgPredicate::Equals(_) => {
                        if let Some(matcher) = matcher {
                            matcher.check_explicit(a, val)
                        } else {
                            false
                        }
                    }
                    ArgPredicate::IsPresent => true,
                };
                required.then(|| req_arg.clone())
            };

            for aa in self.cmd.unroll_arg_requires(is_relevant, a) {
                // if we don't check for duplicates here this causes duplicate error messages
                // see https://github.com/clap-rs/clap/issues/2770
                unrolled_reqs.push(aa);
            }
            // always include the required arg itself. it will not be enumerated
            // by unroll_requirements_for_arg.
            unrolled_reqs.push(a.clone());
        }
        debug!(
            "Usage::get_required_usage_from: unrolled_reqs={:?}",
            unrolled_reqs
        );

        let mut required_groups_members = FlatSet::new();
        let mut required_groups = FlatSet::new();
        for req in unrolled_reqs.iter().chain(incls.iter()) {
            if self.cmd.find_group(req).is_some() {
                let group_members = self.cmd.unroll_args_in_group(req);
                let is_present = matcher
                    .map(|m| {
                        group_members
                            .iter()
                            .any(|arg| m.check_explicit(arg, &ArgPredicate::IsPresent))
                    })
                    .unwrap_or(false);
                debug!(
                    "Usage::get_required_usage_from:iter:{:?} group is_present={}",
                    req, is_present
                );
                if is_present {
                    continue;
                }

                let elem = self.cmd.format_group(req);
                required_groups.insert(elem);
                required_groups_members.extend(group_members);
            } else {
                debug_assert!(self.cmd.find(req).is_some());
            }
        }

        let mut required_opts = FlatSet::new();
        let mut required_positionals = Vec::new();
        for req in unrolled_reqs.iter().chain(incls.iter()) {
            if let Some(arg) = self.cmd.find(req) {
                if required_groups_members.contains(arg.get_id()) {
                    continue;
                }

                let is_present = matcher
                    .map(|m| m.check_explicit(req, &ArgPredicate::IsPresent))
                    .unwrap_or(false);
                debug!(
                    "Usage::get_required_usage_from:iter:{:?} arg is_present={}",
                    req, is_present
                );
                if is_present {
                    continue;
                }

                let stylized = arg.stylized(Some(true));
                if let Some(index) = arg.get_index() {
                    if !arg.is_last_set() || incl_last {
                        let new_len = index + 1;
                        if required_positionals.len() < new_len {
                            required_positionals.resize(new_len, None);
                        }
                        required_positionals[index] = Some(stylized);
                    }
                } else {
                    required_opts.insert(stylized);
                }
            } else {
                debug_assert!(self.cmd.find_group(req).is_some());
            }
        }

        let mut ret_val = Vec::new();
        ret_val.extend(required_opts);
        ret_val.extend(required_groups);
        for pos in required_positionals.into_iter().flatten() {
            ret_val.push(pos);
        }

        debug!("Usage::get_required_usage_from: ret_val={:?}", ret_val);
        ret_val
    }
}
