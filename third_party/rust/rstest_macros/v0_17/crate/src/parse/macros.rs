macro_rules! wrap_attributes {
    ($ident:ident) => {
        #[derive(Default, Debug, PartialEq, Clone)]
        pub(crate) struct $ident {
            inner: Attributes,
        }

        impl From<Attributes> for $ident {
            fn from(inner: Attributes) -> Self {
                $ident { inner }
            }
        }

        impl $ident {
            fn iter(&self) -> impl Iterator<Item = &Attribute> {
                self.inner.attributes.iter()
            }
        }

        impl $ident {
            #[allow(dead_code)]
            pub(crate) fn append(&mut self, attr: Attribute) {
                self.inner.attributes.push(attr)
            }
        }
    };
}
