import json

def load_json(abs_json_path):
    with open(abs_json_path) as fh:
        return json.load(fh)