from enum import Enum
from typing import Any, Optional, Mapping, MutableMapping, Union, Dict

from ._module import BidiModule, command


class ScriptEvaluateResultException(Exception):
    def __init__(self, result: Mapping[str, Any]):
        self.result = result
        super().__init__("Script execution failed.")


class OwnershipModel(Enum):
    NONE = "none"
    ROOT = "root"


class RealmTarget(Dict[str, Any]):
    def __init__(self, realm: str):
        dict.__init__(self, realm=realm)


class ContextTarget(Dict[str, Any]):
    def __init__(self, context: str, sandbox: Optional[str] = None):
        if sandbox is None:
            dict.__init__(self, context=context)
        else:
            dict.__init__(self, context=context, sandbox=sandbox)


Target = Union[RealmTarget, ContextTarget]


class Script(BidiModule):
    @command
    def evaluate(self,
                 expression: str,
                 target: Target,
                 await_promise: Optional[bool] = None,
                 result_ownership: Optional[OwnershipModel] = None) -> Mapping[str, Any]:
        params: MutableMapping[str, Any] = {
            "expression": expression,
            "target": target,
        }

        if await_promise is not None:
            params["awaitPromise"] = await_promise
        if result_ownership is not None:
            params["resultOwnership"] = result_ownership
        return params

    @evaluate.result
    def _evaluate(self, result: Mapping[str, Any]) -> Any:
        if "result" not in result:
            raise ScriptEvaluateResultException(result)
        return result["result"]
