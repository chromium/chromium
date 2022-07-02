import configparser
from dataclasses import dataclass
from datetime import datetime
from decimal import Decimal
from pathlib import Path

import pytest

from mako.testing._config import ConfigValueTypeError
from mako.testing._config import MissingConfig
from mako.testing._config import MissingConfigItem
from mako.testing._config import MissingConfigSection
from mako.testing._config import ReadsCfg
from mako.testing.assertions import assert_raises_message_with_given_cause
from mako.testing.assertions import assert_raises_with_given_cause

PATH_TO_TEST_CONFIG = Path(__file__).parent / "dummy.cfg"


@dataclass
class BasicConfig(ReadsCfg):
    int_value: int
    bool_value: bool
    float_value: float
    str_value: str

    section_header = "basic_values"


@dataclass
class BooleanConfig(ReadsCfg):
    yes: bool
    one: bool
    true: bool
    on: bool
    no: bool
    zero: bool
    false: bool
    off: bool

    section_header = "boolean_values"


@dataclass
class UnsupportedTypesConfig(ReadsCfg):
    decimal_value: Decimal
    datetime_value: datetime

    section_header = "additional_types"


@dataclass
class SupportedTypesConfig(ReadsCfg):
    decimal_value: Decimal
    datetime_value: datetime

    section_header = "additional_types"
    converters = {
        Decimal: lambda v: Decimal(str(v)),
        datetime: lambda v: datetime.fromisoformat(v),
    }


@dataclass
class NonexistentSectionConfig(ReadsCfg):
    some_value: str
    another_value: str

    section_header = "i_dont_exist"


@dataclass
class TypeMismatchConfig(ReadsCfg):
    int_value: int

    section_header = "type_mismatch"


@dataclass
class MissingItemConfig(ReadsCfg):
    present_item: str
    missing_item: str

    section_header = "missing_item"


class BasicConfigTest:
    @pytest.fixture(scope="class")
    def config(self):
        return BasicConfig.from_cfg_file(PATH_TO_TEST_CONFIG)

    def test_coercions(self, config):
        assert isinstance(config.int_value, int)
        assert isinstance(config.bool_value, bool)
        assert isinstance(config.float_value, float)
        assert isinstance(config.str_value, str)

    def test_values(self, config):
        assert config.int_value == 15421
        assert config.bool_value == True
        assert config.float_value == 14.01
        assert config.str_value == "Ceci n'est pas une chaîne"

    def test_error_on_loading_from_nonexistent_file(self):
        assert_raises_with_given_cause(
            MissingConfig,
            FileNotFoundError,
            BasicConfig.from_cfg_file,
            "./n/o/f/i/l/e/h.ere",
        )

    def test_error_on_loading_from_nonexistent_section(self):
        assert_raises_with_given_cause(
            MissingConfigSection,
            configparser.NoSectionError,
            NonexistentSectionConfig.from_cfg_file,
            PATH_TO_TEST_CONFIG,
        )


class BooleanConfigTest:
    @pytest.fixture(scope="class")
    def config(self):
        return BooleanConfig.from_cfg_file(PATH_TO_TEST_CONFIG)

    def test_values(self, config):
        assert config.yes is True
        assert config.one is True
        assert config.true is True
        assert config.on is True
        assert config.no is False
        assert config.zero is False
        assert config.false is False
        assert config.off is False


class UnsupportedTypesConfigTest:
    @pytest.fixture(scope="class")
    def config(self):
        return UnsupportedTypesConfig.from_cfg_file(PATH_TO_TEST_CONFIG)

    def test_values(self, config):
        assert config.decimal_value == "100001.01"
        assert config.datetime_value == "2021-12-04 00:05:23.283"


class SupportedTypesConfigTest:
    @pytest.fixture(scope="class")
    def config(self):
        return SupportedTypesConfig.from_cfg_file(PATH_TO_TEST_CONFIG)

    def test_values(self, config):
        assert config.decimal_value == Decimal("100001.01")
        assert config.datetime_value == datetime(2021, 12, 4, 0, 5, 23, 283000)


class TypeMismatchConfigTest:
    def test_error_on_load(self):
        assert_raises_message_with_given_cause(
            ConfigValueTypeError,
            "Wrong value type for int_value",
            ValueError,
            TypeMismatchConfig.from_cfg_file,
            PATH_TO_TEST_CONFIG,
        )


class MissingItemConfigTest:
    def test_error_on_load(self):
        assert_raises_message_with_given_cause(
            MissingConfigItem,
            "No config item for missing_item",
            configparser.NoOptionError,
            MissingItemConfig.from_cfg_file,
            PATH_TO_TEST_CONFIG,
        )
