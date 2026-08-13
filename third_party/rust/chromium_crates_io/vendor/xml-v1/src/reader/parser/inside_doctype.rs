use std::fmt::Write;

use crate::attribute::OwnedAttribute;
use crate::common::{is_name_char, is_name_start_char, is_pubid_char, is_whitespace_char};
use crate::name::OwnedName;
use crate::reader::error::SyntaxError;
use crate::reader::lexer::Token;
use crate::reader::XmlEvent;

use super::{DoctypeSubstate, ProcessingInstructionSubstate, PullParser, QuoteToken, Result, State};

impl PullParser {
    pub fn inside_doctype(&mut self, t: Token, substate: DoctypeSubstate) -> Option<Result> {
        if let Some(ref mut doctype) = self.data.doctype {
            write!(doctype, "{t}").ok()?;
            if doctype.len() > self.config.max_data_length {
                return Some(self.error(SyntaxError::ExceededConfiguredLimit));
            }
        }

        match substate {
            DoctypeSubstate::BeforeDoctypeName => match t {
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::Character(c) if is_name_start_char(c) => {
                    self.buf.push(c);
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::DoctypeName))
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::DoctypeName => match t {
                Token::TagEnd => {
                    self.data.doctype_name = Some(self.take_buf_boxed());
                    let event = XmlEvent::Doctype {
                        syntax: self.data.doctype.clone().unwrap_or_default(),
                    };
                    self.into_state_emit(State::OutsideTag, Ok(event))
                },
                Token::Character(c) if is_whitespace_char(c) => {
                    self.data.doctype_name = Some(self.take_buf_boxed());
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::Outside))
                },
                Token::Character(c) if is_name_char(c) => {
                    self.buf.push(c);
                    if self.buf.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    None
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::ExternalIdKeyword => match t {
                Token::Character(c @ 'A'..='Z') => {
                    self.buf.push(c);
                    if self.buf == "SYSTEM" {
                        self.buf.clear();
                        return self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::BeforeSystemLiteral,
                        ));
                    }
                    if self.buf == "PUBLIC" {
                        self.buf.clear();
                        return self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::BeforePubId,
                        ));
                    }
                    if "PUBLIC".starts_with(&self.buf) || "SYSTEM".starts_with(&self.buf) {
                        return None;
                    }
                    Some(self.error(SyntaxError::UnexpectedToken(t)))
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::BeforeSystemLiteral => match t {
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = QuoteToken::from_token(t);
                    self.buf.clear();
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::SystemLiteral))
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::SystemLiteral => match t {
                Token::SingleQuote if self.data.quote != Some(QuoteToken::SingleQuoteToken) => {
                    self.buf.push('\'');
                    None
                },
                Token::DoubleQuote if self.data.quote != Some(QuoteToken::DoubleQuoteToken) => {
                    self.buf.push('"');
                    None
                },
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = None;
                    self.data.doctype_system_id = Some(self.take_buf_boxed());
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::Outside))
                },
                Token::Character(c) => {
                    self.buf.push(c);
                    None
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::BeforePubId => match t {
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = QuoteToken::from_token(t);
                    self.buf.clear();
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::PubId))
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::PubId => match t {
                Token::SingleQuote if self.data.quote != Some(QuoteToken::SingleQuoteToken) => {
                    self.buf.push('\'');
                    None
                },
                Token::DoubleQuote if self.data.quote != Some(QuoteToken::DoubleQuoteToken) => {
                    self.buf.push('"');
                    None
                },
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = None;
                    self.data.doctype_public_id = Some(self.take_buf_boxed());
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::BeforeSystemLiteral,
                    ))
                }
                Token::Character(c) if is_pubid_char(c) => {
                    self.buf.push(c);
                    None
                },
                Token::ReferenceEnd => {
                    self.buf.push(';');
                    None
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::Outside => match t {
                Token::TagEnd => {
                    let event = XmlEvent::Doctype {
                        syntax: self.data.doctype.clone().unwrap_or_default(),
                    };
                    self.into_state_emit(State::OutsideTag, Ok(event))
                }
                Token::CDataEnd | Token::CDataStart => {
                    Some(self.error(SyntaxError::UnexpectedToken(t)))
                }
                Token::Character('[') => {
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::InternalSubset))
                }
                Token::Character(c @ ('S' | 'P')) => {
                    self.buf.push(c);
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::ExternalIdKeyword,
                    ))
                }
                Token::Character(c) if is_whitespace_char(c) => None,
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::InternalSubset => match t {
                Token::Character(']') => {
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::Outside))
                }

                Token::SingleQuote | Token::DoubleQuote => {
                    // just discard string literals
                    self.data.quote = QuoteToken::from_token(t);
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::String))
                }
                Token::CommentStart => {
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::Comment))
                }
                Token::Character('%') => {
                    self.data.ref_data.clear();
                    self.data.ref_data.push('%');
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::PEReferenceInDtd,
                    ))
                }
                Token::MarkupDeclarationStart => {
                    self.buf.clear();
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::InsideName))
                }
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::ProcessingInstructionStart => {
                    self.buf.clear();
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::PI(ProcessingInstructionSubstate::PIReadingName)))
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::PI(s) => self.inside_processing_instruction(t, s),
            DoctypeSubstate::String => match t {
                Token::SingleQuote if self.data.quote != Some(QuoteToken::SingleQuoteToken) => None,
                Token::DoubleQuote if self.data.quote != Some(QuoteToken::DoubleQuoteToken) => None,
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = None;
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::InternalSubset))
                }
                _ => None,
            },
            DoctypeSubstate::Comment => match t {
                Token::CommentEnd => {
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::InternalSubset))
                }
                _ => None,
            },
            DoctypeSubstate::InsideName => match t {
                Token::Character(c @ 'A'..='Z') => {
                    self.buf.push(c);
                    None
                },
                Token::Character(c) if is_whitespace_char(c) => {
                    let state = match self.buf.as_str() {
                        "ENTITY" => self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::BeforeEntityName,
                        )),
                        "ATTLIST" => self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::AttlistElementName,
                        )),
                        "NOTATION" | "ELEMENT" => self.into_state_continue(
                            State::InsideDoctype(DoctypeSubstate::SkipDeclaration),
                        ),
                        _ => Some(self.error(SyntaxError::UnknownMarkupDeclaration(self.buf.as_str().into()))),
                    };
                    self.buf.clear();
                    state
                },
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::BeforeEntityName => {
                self.data.name.clear();
                match t {
                    Token::Character(c) if is_whitespace_char(c) => None,
                    Token::Character('%') => {
                        // % is for PEDecl
                        self.data.name.push('%');
                        self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::PEReferenceDefinitionStart,
                        ))
                    }
                    Token::Character(c) if is_name_start_char(c) => {
                        if self.data.name.len() > self.config.max_name_length {
                            return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                        }
                        self.data.name.push(c);
                        self.into_state_continue(State::InsideDoctype(DoctypeSubstate::EntityName))
                    },
                    _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
                }
            },
            DoctypeSubstate::EntityName => match t {
                Token::Character(c) if is_whitespace_char(c) => self
                    .into_state_continue(State::InsideDoctype(DoctypeSubstate::BeforeEntityValue)),
                Token::Character(c) if is_name_char(c) => {
                    if self.data.name.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    self.data.name.push(c);
                    None
                },
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::BeforeEntityValue => {
                self.buf.clear();
                match t {
                    Token::Character(c) if is_whitespace_char(c) => None,
                    // SYSTEM/PUBLIC not supported
                    Token::Character('S' | 'P') => {
                        let name = self.data.take_name();
                        self.entities.entry(name).or_default(); // Dummy value, but at least the name is recognized

                        self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::SkipDeclaration,
                        ))
                    }
                    Token::SingleQuote | Token::DoubleQuote => {
                        self.data.quote = QuoteToken::from_token(t);
                        self.into_state_continue(State::InsideDoctype(DoctypeSubstate::EntityValue))
                    }
                    _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
                }
            }
            DoctypeSubstate::EntityValue => match t {
                Token::SingleQuote if self.data.quote != Some(QuoteToken::SingleQuoteToken) => {
                    self.buf.push('\'');
                    None
                }
                Token::DoubleQuote if self.data.quote != Some(QuoteToken::DoubleQuoteToken) => {
                    self.buf.push('"');
                    None
                }
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = None;
                    let name = self.data.take_name();
                    let val = self.take_buf();
                    self.entities.entry(name).or_insert(val); // First wins
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::SkipDeclaration))
                    // FIXME
                }
                Token::ReferenceStart | Token::Character('&') => {
                    self.data.ref_data.clear();
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::NumericReferenceStart,
                    ))
                }
                Token::Character('%') => {
                    self.data.ref_data.clear();
                    self.data.ref_data.push('%'); // include literal % in the name to distinguish from regular entities
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::PEReferenceInValue,
                    ))
                }
                Token::Character(c) if !self.is_valid_xml_char(c) => {
                    Some(self.error(SyntaxError::InvalidCharacterEntity(c as u32)))
                }
                Token::Character(c) => {
                    self.buf.push(c);
                    None
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::PEReferenceDefinitionStart => match t {
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::Character(c) if is_name_start_char(c) => {
                    debug_assert_eq!(self.data.name, "%");
                    self.data.name.push(c);
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::PEReferenceDefinition,
                    ))
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::PEReferenceDefinition => match t {
                Token::Character(c) if is_name_char(c) => {
                    if self.data.name.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    self.data.name.push(c);
                    None
                }
                Token::Character(c) if is_whitespace_char(c) => self
                    .into_state_continue(State::InsideDoctype(DoctypeSubstate::BeforeEntityValue)),
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::PEReferenceInDtd => match t {
                Token::Character(c) if is_name_char(c) => {
                    self.data.ref_data.push(c);
                    None
                }
                Token::ReferenceEnd | Token::Character(';') => {
                    let name = self.data.take_ref_data();
                    match self.entities.get(&name) {
                        Some(ent) => {
                            if let Err(e) = self.lexer.reparse(ent) {
                                return Some(Err(e));
                            }
                            self.into_state_continue(State::InsideDoctype(
                                DoctypeSubstate::InternalSubset,
                            ))
                        }
                        None => Some(self.error(SyntaxError::UndefinedEntity(name.into()))),
                    }
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::PEReferenceInValue => match t {
                Token::Character(c) if is_name_char(c) => {
                    self.data.ref_data.push(c);
                    None
                }
                Token::ReferenceEnd | Token::Character(';') => {
                    let name = self.data.take_ref_data();
                    match self.entities.get(&name) {
                        Some(ent) => {
                            self.buf.push_str(ent);
                            self.into_state_continue(State::InsideDoctype(
                                DoctypeSubstate::EntityValue,
                            ))
                        }
                        None => Some(self.error(SyntaxError::UndefinedEntity(name.into()))),
                    }
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::NumericReferenceStart => match t {
                Token::Character('#') => self
                    .into_state_continue(State::InsideDoctype(DoctypeSubstate::NumericReference)),
                Token::Character(c) if !self.is_valid_xml_char(c) => {
                    Some(self.error(SyntaxError::InvalidCharacterEntity(c as u32)))
                }
                Token::Character(c) => {
                    self.buf.push('&');
                    self.buf.push(c);
                    // named entities are not expanded inside doctype
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::EntityValue))
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::NumericReference => match t {
                Token::ReferenceEnd | Token::Character(';') => {
                    let r = self.data.take_ref_data();
                    // https://www.w3.org/TR/xml/#sec-entexpand
                    match self.numeric_reference_from_str(&r) {
                        Ok(c) => {
                            self.buf.push(c);
                            self.into_state_continue(State::InsideDoctype(
                                DoctypeSubstate::EntityValue,
                            ))
                        }
                        Err(e) => Some(self.error(e)),
                    }
                }
                Token::Character(c) if !self.is_valid_xml_char(c) => {
                    Some(self.error(SyntaxError::InvalidCharacterEntity(c as u32)))
                }
                Token::Character(c) => {
                    self.data.ref_data.push(c);
                    None
                }
                _ => Some(self.error(SyntaxError::UnexpectedTokenInEntity(t))),
            },
            DoctypeSubstate::SkipDeclaration => match t {
                Token::TagEnd => {
                    self.into_state_continue(State::InsideDoctype(DoctypeSubstate::InternalSubset))
                }
                _ => None,
            },
            DoctypeSubstate::AttlistElementName => match t {
                Token::Character(c) if is_whitespace_char(c) => {
                    if self.buf.is_empty() {
                        None
                    } else {
                        self.attlist_element_name = Some(self.take_buf());
                        self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::AttlistInside,
                        ))
                    }
                }
                Token::Character(c) if is_name_char(c) => {
                    if self.buf.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    self.buf.push(c);
                    None
                }
                Token::TagEnd => {
                    self.buf.clear();
                    self.attlist_element_name = None;
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::InternalSubset,
                    ))
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::AttlistInside => match t {
                Token::Character(c) if is_whitespace_char(c) => None,
                Token::TagEnd => {
                    self.attlist_element_name = None;
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::InternalSubset,
                    ))
                }
                Token::Character(c) if is_name_start_char(c) => {
                    self.buf.push(c);
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::AttlistAttributeName,
                    ))
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::AttlistAttributeName => match t {
                Token::Character(c) if is_name_char(c) => {
                    if self.buf.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    self.buf.push(c);
                    None
                }
                Token::Character(c) if is_whitespace_char(c) => {
                    let attr_name_str = self.take_buf();
                    match attr_name_str.parse::<OwnedName>() {
                        Ok(name) => {
                            self.attlist_attr_name = Some(name);
                            self.into_state_continue(State::InsideDoctype(
                                DoctypeSubstate::AttlistType,
                            ))
                        }
                        Err(()) => Some(self.error(SyntaxError::InvalidQualifiedName(attr_name_str.into()))),
                    }
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::AttlistType => match t {
                Token::Character('#') => {
                    self.buf.push('#');
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::AttlistDefaultDeclKeyword,
                    ))
                }
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = QuoteToken::from_token(t);
                    self.buf.clear();
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::AttlistDefaultValue,
                    ))
                }
                Token::TagEnd => {
                    self.attlist_attr_name = None;
                    self.attlist_element_name = None;
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::InternalSubset,
                    ))
                }
                _ => None,
            },
            DoctypeSubstate::AttlistDefaultDeclKeyword => match t {
                Token::Character(c) if is_name_char(c) => {
                    if self.buf.len() > self.config.max_name_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    self.buf.push(c);
                    None
                }
                Token::Character(c) if is_whitespace_char(c) => {
                    let kw = self.take_buf();
                    if kw == "#FIXED" {
                        self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::AttlistType,
                        ))
                    } else {
                        self.attlist_attr_name = None;
                        self.into_state_continue(State::InsideDoctype(
                            DoctypeSubstate::AttlistInside,
                        ))
                    }
                }
                Token::TagEnd => {
                    self.buf.clear();
                    self.attlist_attr_name = None;
                    self.attlist_element_name = None;
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::InternalSubset,
                    ))
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
            DoctypeSubstate::AttlistDefaultValue => match t {
                Token::SingleQuote if self.data.quote != Some(QuoteToken::SingleQuoteToken) => {
                    self.buf.push('\'');
                    None
                }
                Token::DoubleQuote if self.data.quote != Some(QuoteToken::DoubleQuoteToken) => {
                    self.buf.push('"');
                    None
                }
                Token::SingleQuote | Token::DoubleQuote => {
                    self.data.quote = None;
                    let val = self.take_buf();
                    if let (Some(ref elem_name), Some(attr_name)) = (&self.attlist_element_name, self.attlist_attr_name.take()) {
                        let list = self.default_attributes.entry(elem_name.clone()).or_default();
                        if !list.iter().any(|attr| attr.name.local_name == attr_name.local_name && attr.name.prefix == attr_name.prefix) {
                            list.push(OwnedAttribute {
                                name: attr_name,
                                value: val,
                            });
                        }
                    }
                    self.into_state_continue(State::InsideDoctype(
                        DoctypeSubstate::AttlistInside,
                    ))
                }
                Token::ReferenceStart | Token::Character('&') => {
                    self.data.ref_data.clear();
                    self.state_after_reference = self.st;
                    self.into_state_continue(State::InsideReference)
                }
                Token::Character(c) if !self.is_valid_xml_char(c) => {
                    Some(self.error(SyntaxError::InvalidCharacterEntity(c as u32)))
                }
                Token::Character(c) => {
                    if self.buf.len() > self.config.max_attribute_length {
                        return Some(self.error(SyntaxError::ExceededConfiguredLimit));
                    }
                    if is_whitespace_char(c) {
                        self.buf.push(' ');
                    } else {
                        self.buf.push(c);
                    }
                    None
                }
                _ => Some(self.error(SyntaxError::UnexpectedToken(t))),
            },
        }
    }
}
