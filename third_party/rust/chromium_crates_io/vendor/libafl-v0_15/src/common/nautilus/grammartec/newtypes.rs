use core::ops::Add;

use serde::{Deserialize, Serialize};

#[derive(PartialEq, Eq, Debug, Copy, Clone, Hash, Serialize, Deserialize)]
pub struct RuleId(usize);

#[derive(PartialEq, PartialOrd, Eq, Debug, Copy, Clone, Hash, Serialize, Deserialize)]
pub struct NodeId(usize);

#[derive(PartialEq, Eq, Debug, Copy, Clone, Hash, Serialize, Deserialize)]
pub struct NTermId(usize);

impl RuleId {
    #[must_use]
    pub fn to_i(&self) -> usize {
        self.0
    }
}

impl From<usize> for RuleId {
    fn from(i: usize) -> RuleId {
        RuleId(i)
    }
}

impl From<RuleId> for usize {
    fn from(rule: RuleId) -> usize {
        rule.0
    }
}

impl Add<usize> for RuleId {
    type Output = RuleId;
    fn add(self, rhs: usize) -> RuleId {
        RuleId(self.0 + rhs)
    }
}

impl NodeId {
    #[must_use]
    pub fn to_i(&self) -> usize {
        self.0
    }
}

impl From<usize> for NodeId {
    fn from(i: usize) -> Self {
        NodeId(i)
    }
}

impl From<NodeId> for usize {
    fn from(val: NodeId) -> Self {
        val.0
    }
}

impl Add<usize> for NodeId {
    type Output = NodeId;
    fn add(self, rhs: usize) -> NodeId {
        NodeId(self.0 + rhs)
    }
}

impl NodeId {
    #[expect(dead_code)]
    fn steps_between(start: Self, end: Self) -> Option<usize> {
        let start_i = start.to_i();
        let end_i = end.to_i();
        if start > end {
            return None;
        }
        Some(end_i - start_i)
    }

    #[expect(dead_code)]
    fn add_one(self) -> Self {
        self.add(1)
    }

    #[expect(dead_code)]
    fn sub_one(self) -> Self {
        NodeId(self.0 - 1)
    }

    #[expect(dead_code)]
    fn add_usize(self, n: usize) -> Option<Self> {
        self.0.checked_add(n).map(NodeId::from)
    }
}

impl NTermId {
    #[must_use]
    pub fn to_i(self) -> usize {
        self.0
    }
}

impl From<usize> for NTermId {
    fn from(i: usize) -> Self {
        NTermId(i)
    }
}

impl From<NTermId> for usize {
    fn from(val: NTermId) -> Self {
        val.0
    }
}

impl Add<usize> for NTermId {
    type Output = NTermId;
    fn add(self, rhs: usize) -> NTermId {
        NTermId(self.0 + rhs)
    }
}

#[cfg(test)]
mod tests {
    use super::{NTermId, NodeId, RuleId};

    #[test]
    fn rule_id() {
        let r1: RuleId = 1337.into();
        let r2 = RuleId::from(1338);
        let i1: usize = r1.into();
        assert_eq!(i1, 1337);
        let i2: usize = 1338;
        assert_eq!(i2, r2.to_i());
        let r3 = r2 + 3;
        assert_eq!(r3, 1341.into());
    }

    #[test]
    fn node_id() {
        let r1: NodeId = 1337.into();
        let r2 = NodeId::from(1338);
        let i1: usize = r1.into();
        assert_eq!(i1, 1337);
        let i2: usize = 1338;
        assert_eq!(i2, r2.to_i());
        let r3 = r2 + 3;
        assert_eq!(r3, 1341.into());
    }

    #[test]
    fn nterm_id() {
        let r1: NTermId = 1337.into();
        let r2 = NTermId::from(1338);
        let i1: usize = r1.into();
        assert_eq!(i1, 1337);
        let i2: usize = 1338;
        assert_eq!(i2, r2.to_i());
        let r3 = r2 + 3;
        assert_eq!(r3, 1341.into());
    }
}
