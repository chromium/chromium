use anyhow::Result;
use serde_json::Value;
use std::{cell::RefCell, rc::Rc, sync::Arc};

use super::{
    super::{schema::SchemaBuilderOptions, shared_context::SharedContext, RetrieveWrapper},
    Draft, ResourceRef,
};

const DEFAULT_DRAFT: Draft = Draft::Draft202012;

fn draft_for(value: &Value) -> Draft {
    DEFAULT_DRAFT.detect(value).unwrap_or(DEFAULT_DRAFT)
}

pub struct PreContext {
    draft: Draft,
    root_doc: Arc<Value>,
    pub base_uri: String,
}

#[derive(Clone)]
pub struct Context<'a> {
    pub draft: Draft,
    pub shared: Rc<RefCell<SharedContext>>,
    pub options: SchemaBuilderOptions,
    root_doc: Arc<Value>,
    _marker: std::marker::PhantomData<&'a ()>,
}

impl PreContext {
    pub fn new(contents: Value, _retriever: Option<RetrieveWrapper>) -> Result<Self> {
        let draft = draft_for(&contents);
        let base_uri = "#/".to_string();

        Ok(PreContext {
            root_doc: Arc::new(contents),
            draft,
            base_uri,
        })
    }
}

impl<'a> Context<'a> {
    pub fn new(pre_context: &'a PreContext) -> Result<Self> {
        let ctx = Context {
            root_doc: Arc::clone(&pre_context.root_doc),
            draft: pre_context.draft,
            shared: Rc::new(RefCell::new(SharedContext::new())),
            options: SchemaBuilderOptions::default(),
            _marker: std::marker::PhantomData,
        };

        Ok(ctx)
    }

    pub fn in_subresource(&'a self, _resource: ResourceRef) -> Result<Context<'a>> {
        Ok(self.clone())
    }

    pub fn as_resource_ref<'r>(&'a self, contents: &'r Value) -> ResourceRef<'r> {
        self.draft
            .detect(contents)
            .unwrap_or(DEFAULT_DRAFT)
            .create_resource_ref(contents)
    }

    pub fn normalize_ref(&self, reference: &str) -> Result<String> {
        Ok(reference.to_string())
    }

    pub fn lookup_resource(&'a self, reference: &str) -> Result<ResourceRef<'a>> {
        if reference == "#" || reference == "#/" {
            return Ok(self.as_resource_ref(self.root_doc.as_ref()));
        }

        if !reference.starts_with("#/") {
            return Err(anyhow::anyhow!(
                "Only absolute $ref's (#/...) are supported without 'referencing' feature; ref '{}'",
                reference
            ));
        }
        let mut content = self.root_doc.as_ref();
        for segment in reference[2..].split('/') {
            if content.is_array() {
                let index = segment.parse::<usize>()?;
                content = content
                    .get(index)
                    .ok_or_else(|| anyhow::anyhow!("Reference segment '{}' not found.", segment))?;
            } else if let Some(next) = content.get(segment) {
                content = next;
            } else {
                return Err(anyhow::anyhow!(
                    "Reference segment '{}' not found in '{}'.",
                    segment,
                    reference
                ));
            }
        }

        Ok(self.as_resource_ref(content))
    }
}
