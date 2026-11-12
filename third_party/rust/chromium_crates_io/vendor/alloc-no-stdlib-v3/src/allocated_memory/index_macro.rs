#[macro_export]
macro_rules! define_index_ops_mut {
    ($T:ident, $MemoryType:ty) => {
        impl<$T> ::core::ops::Index<usize> for $MemoryType
        {
            type Output = T;

            #[inline]
            fn index(&self, index: usize) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
                
        impl<$T> ::core::ops::IndexMut<usize> for $MemoryType
        {
            #[inline]
            fn index_mut(&mut self, index: usize) -> &mut Self::Output {
                ::core::ops::IndexMut::index_mut(&mut **self, index)
            }
        }
        
                
        impl<$T> ::core::ops::Index<::core::ops::Range<usize>> for $MemoryType
        {
            type Output = [T];
    #[inline]
            fn index(&self, index: ::core::ops::Range<usize>) -> &Self::Output {
        ::core::ops::Index::index(&**self, index)
            }
        }
        
        impl<$T> ::core::ops::IndexMut<::core::ops::Range<usize>> for $MemoryType
        {
            #[inline]
            fn index_mut(&mut self, index: ::core::ops::Range<usize>) -> &mut Self::Output {
                ::core::ops::IndexMut::index_mut(&mut **self, index)
            }
        }

        
        impl<$T> ::core::ops::Deref for $MemoryType {
            type Target = [T];
            
            fn deref(&self) -> &[T] {
                self.slice()
            }
        }
        
        impl<T> ::core::ops::DerefMut for $MemoryType {
            fn deref_mut(&mut self) -> &mut [T] {
                self.slice_mut()
            }
        }
    };
    ($T0: ident, $T:ident, $MemoryType:ty) => {
        impl<'a, $T> ::core::ops::Index<usize> for $MemoryType
        {
            type Output = T;

            #[inline]
            fn index(&self, index: usize) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        impl<'a, $T> ::core::ops::IndexMut<usize> for $MemoryType
        {
            #[inline]
            fn index_mut(&mut self, index: usize) -> &mut Self::Output {
                ::core::ops::IndexMut::index_mut(&mut **self, index)
            }
        }
        
        
        impl<'a, $T> ::core::ops::Index<::core::ops::Range<usize>> for $MemoryType
        {
            type Output = [T];
            
            #[inline]
            fn index(&self, index: ::core::ops::Range<usize>) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        
        impl<'a, $T> ::core::ops::IndexMut<::core::ops::Range<usize>> for $MemoryType
        {
            #[inline]
            fn index_mut(&mut self, index: ::core::ops::Range<usize>) -> &mut Self::Output {
                ::core::ops::IndexMut::index_mut(&mut **self, index)
            }
        }


        impl<'a, $T> ::core::ops::Deref for $MemoryType {
            type Target = [T];
            
            fn deref(&self) -> &[T] {
                self.slice()
            }
        }
        impl<'a, $T> ::core::ops::DerefMut for $MemoryType {
            fn deref_mut(&mut self) -> &mut [T] {
                self.slice_mut()
            }
        }
    }
}
#[macro_export]
macro_rules! define_index_ops {
    ($T:ident, $MemoryType:ty) => {
        impl<$T> ::core::ops::Index<usize> for $MemoryType
        {
            type Output = T;

            #[inline]
            fn index(&self, index: usize) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        
        impl<$T> ::core::ops::Index<::core::ops::Range<usize>> for $MemoryType
        {
            type Output = [T];
            
            #[inline]
            fn index(&self, index: ::core::ops::Range<usize>) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        impl<$T> ::core::ops::Deref for $MemoryType {
            type Target = [T];
            
            fn deref(&self) -> &[T] {
                self.slice()
            }
        }
        
    };
    ($T0: tt, $T:ident, $MemoryType:ty) => {
        impl<'a, $T> ::core::ops::Index<usize> for $MemoryType
        {
            type Output = T;

            #[inline]
            fn index(&self, index: usize) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        impl<'a, $T> ::core::ops::Index<::core::ops::Range<usize>> for $MemoryType
        {
            type Output = [T];
            
            #[inline]
            fn index(&self, index: ::core::ops::Range<usize>) -> &Self::Output {
                ::core::ops::Index::index(&**self, index)
            }
        }
        
        
        impl<'a, $T> ::core::ops::Deref for $MemoryType {
            type Target = [T];
            
            fn deref(&self) -> &[T] {
                self.slice()
            }
        }
    }
}
