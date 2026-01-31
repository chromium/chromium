use std::rc::Weak;
use parser::BaseParser;

pub struct TraceListener {
    parser: Box<BaseParser>,
}

impl TraceListener {
    fn new_trace_listener(parser: Box<BaseParser>) -> * TraceListener { unimplemented!() }

    fn visit_error_node(&self, _: ErrorNode) { unimplemented!() }

    fn enter_every_rule(&self, ctx: ParserRuleContext) { unimplemented!() }

    fn visit_terminal(&self, node: TerminalNode) { unimplemented!() }

    fn exit_every_rule(&self, ctx: ParserRuleContext) { unimplemented!() }
}
 