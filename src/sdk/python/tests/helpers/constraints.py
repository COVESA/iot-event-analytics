from src.iotea.core.rules import ChangeConstraint, OpConstraint

def create_change_constraint(*args):
    oc = ChangeConstraint(*args)
    return oc

def create_op_constraint(*args):
    oc = OpConstraint(*args)
    oc.value['$id'] = None
    return oc