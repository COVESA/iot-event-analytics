from os import path
import pytest
from unittest import TestCase
from src.iotea.core.util.json_model import JsonModel
from tests.helpers.json_loader import load_json

@pytest.fixture
def test_case():
    return TestCase()

@pytest.fixture
def model():
    json = load_json(path.normpath(path.join(path.dirname(__file__), '../../../resources/jsonModel.input.json')))
    return JsonModel(json)

class TestJsonModel:
    def test_return_a_given_value(self, model):
        assert model.get('bar.baz') == 'Hello World'

    def test_return_default_value(self, model):
        assert model.get('bar.bar', 'Hello') == 'Hello'

    def test_raise_error_on_given_false_path(self, test_case, model):
        with pytest.raises(Exception) as exc_info:
            model.get('bar.bar')

        assert 'Path bar does not exist' == str(exc_info.value)