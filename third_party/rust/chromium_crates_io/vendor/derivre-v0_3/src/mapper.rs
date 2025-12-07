use anyhow::Result;

struct StackNode<'a, T, S> {
    ast: &'a T,
    trg: usize,
    args: Vec<S>,
}

pub fn map_ast<T, S>(
    ast: &T,
    get_args: impl Fn(&T) -> &[T],
    mut map_node: impl FnMut(&T, &mut Vec<S>) -> Result<S>,
) -> Result<S> {
    let mut stack = vec![StackNode {
        ast,
        trg: 0,
        args: Vec::new(),
    }];

    while let Some(mut entry) = stack.pop() {
        let args = get_args(entry.ast);
        if !args.is_empty() && entry.args.is_empty() {
            // children not yet processed
            let trg = stack.len();
            // re-push current
            stack.push(entry);
            // and children
            for ast in args.iter().rev() {
                stack.push(StackNode {
                    ast,
                    trg,
                    args: Vec::new(),
                });
            }
        } else {
            assert!(entry.args.len() == args.len());
            let r = map_node(entry.ast, &mut entry.args)?;
            if stack.is_empty() {
                return Ok(r);
            }
            stack[entry.trg].args.push(r);
        }
    }

    unreachable!()
}
