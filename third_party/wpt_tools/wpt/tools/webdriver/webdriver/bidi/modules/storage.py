from typing import Any, Dict, Mapping, Union
from ._module import BidiModule, command
from webdriver.bidi.modules.network import NetworkBytesValue
from ..undefined import UNDEFINED, Undefined


class BrowsingContextPartitionDescriptor(Dict[str, Any]):
    def __init__(self, context: str):
        dict.__init__(self, type="context", context=context)


class StorageKeyPartitionDescriptor(Dict[str, Any]):
    def __init__(self, user_context: Union[Undefined, str] = UNDEFINED,
                 source_origin: Union[Undefined, str] = UNDEFINED):
        dict.__init__(self, type="storageKey")
        if user_context is not UNDEFINED:
            self["userContext"] = user_context
        if source_origin is not UNDEFINED:
            self["sourceOrigin"] = source_origin


class PartialCookie(Dict[str, Any]):
    def __init__(
            self,
            name: str,
            value: NetworkBytesValue,
            domain: str,
            path: Union[Undefined, str] = UNDEFINED,
            http_only: Union[Undefined, bool] = UNDEFINED,
            secure: Union[Undefined, bool] = UNDEFINED,
            same_site: Union[Undefined, str] = UNDEFINED,
            expiry: Union[Undefined, int] = UNDEFINED,
    ):
        dict.__init__(self, name=name, value=value, domain=domain)
        if path is not UNDEFINED:
            self["path"] = path
        if http_only is not UNDEFINED:
            self["httpOnly"] = http_only
        if secure is not UNDEFINED:
            self["secure"] = secure
        if same_site is not UNDEFINED:
            self["sameSite"] = same_site
        if expiry is not UNDEFINED:
            self["expiry"] = expiry


PartitionDescriptor = Union[StorageKeyPartitionDescriptor, BrowsingContextPartitionDescriptor]


class Storage(BidiModule):

    # TODO: extend with `filter`.
    @command
    def get_cookies(self, partition: Union[Undefined, PartitionDescriptor] = UNDEFINED) -> Mapping[str, Any]:
        return {"partition": partition}

    @command
    def set_cookie(
            self,
            cookie: PartialCookie,
            partition: Union[Undefined, PartitionDescriptor] = UNDEFINED
    ) -> Mapping[str, Any]:
        return {
            'cookie': cookie,
            "partition": partition
        }
