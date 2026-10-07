#![allow(clippy::useless_conversion)] // This seems to be a false-positive(?)

use alloc::{ffi::CString, string::String, vec::Vec};

use pyo3::{prelude::*, pyclass, types::IntoPyDict};

use crate::nautilus::grammartec::context::Context;

#[pyclass]
struct PyContext {
    ctx: Context,
}
impl PyContext {
    fn get_context(&self) -> Context {
        self.ctx.clone()
    }
}

#[pymethods]
impl PyContext {
    #[new]
    fn new() -> Self {
        PyContext {
            ctx: Context::new(),
        }
    }

    fn rule(&mut self, _py: Python, nt: &str, format: &Bound<PyAny>) -> PyResult<()> {
        if let Ok(s) = format.extract::<&str>() {
            self.ctx.add_rule(nt, s.as_bytes());
        } else if let Ok(s) = format.extract::<&[u8]>() {
            self.ctx.add_rule(nt, s);
        } else {
            return Err(pyo3::exceptions::PyValueError::new_err(
                "format argument should be string or bytes",
            ));
        }
        Ok(())
    }

    #[expect(clippy::needless_pass_by_value)]
    fn script(&mut self, nt: &str, nts: Vec<String>, script: Py<PyAny>) {
        self.ctx.add_script(nt, &nts, script);
    }

    fn regex(&mut self, nt: &str, regex: &str) {
        self.ctx.add_regex(nt, regex);
    }
}

fn loader(py: Python, grammar: &str) -> PyResult<Context> {
    let py_ctx = Bound::new(py, PyContext::new())?;
    let locals = [("ctx", &py_ctx)].into_py_dict(py)?;
    py.run(&CString::new(grammar)?, None, Some(&locals))?;
    Ok(py_ctx.borrow().get_context())
}

/// Create a `NautilusContext` from a python grammar file
#[must_use]
pub fn load_python_grammar(grammar: &str) -> Context {
    Python::attach(|py| {
        loader(py, grammar)
            .map_err(|e| e.print_and_set_sys_last_vars(py))
            .expect("failed to parse python grammar")
    })
}
