from os import path
import pytest
from unittest import TestCase
from src.iotea.core.talent import Talent
from src.iotea.core.rules import AndRules, Rule, ChangeConstraint, Constraint
from tests.helpers.json_loader import load_json

@pytest.fixture
def test_case():
    return TestCase()

class MyTalent(Talent):
    def __init__(self):
        super(MyTalent, self).__init__('test-talent', 'mqtt://localhost:1883')

    def get_rules(self):
        return AndRules([
            Rule(ChangeConstraint('temp', 'kuehlschrank', Constraint.VALUE_TYPE['RAW']))
        ])

    async def on_event(self, ev, evtctx):
        print(f'Raw value {TalentInput.get_raw_value(ev)}')

@pytest.fixture
def talent():
    talent = MyTalent()
    yield talent
    # Teardown logic

class TestTalent:
    def test_create_discovery_response(self, talent, test_case):
        test_case.assertDictEqual(
            talent._Talent__create_discovery_response(),
            load_json(path.normpath(path.join(path.dirname(__file__), '../../resources/talent.discovery-response.json')))
        )
