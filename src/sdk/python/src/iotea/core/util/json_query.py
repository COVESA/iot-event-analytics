##############################################################################
# Copyright (c) 2021 Bosch.IO GmbH
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.
#
# SPDX-License-Identifier: MPL-2.0
##############################################################################

import re
from functools import partial

# query
# :<label> > Gets value as is
# foo.bar:<label> > Gets nested value 'baz' in { foo: { bar: 'baz' }}
# *.bar:<label> > Gets all nested values 'baz1' and 'baz2' in { foo1: { bar: 'baz1' }, foo2: { bar: 'baz2' }}
# foo[1]:<label> > Gets second member of array in { foo: [0, 2, 3] } > 2
# foo[1:2]:<label> > Gets range of array in { foo: [0, 2, 3] } > 2
# foo.bar[:]:<label> > Gets array in { foo: { bar: [0, 2, 3] } } > 0, 2, 3
# foo.bar[-1]:<label> > Gets last member of array in { foo: { bar: [0, 2, 3] } } > 3
# foo.bar[0:]:<label> > Gets array in { foo: { bar: [0, 2, 3] } } > 0, 2, 3
# foo.bar[0:-1]:<label> > Gets array in { foo: { bar: [0, 2, 3] } } > 0, 2
# 'foo.bar' > Searches for field 'foo.bar' > The point is masked

MASKED_FIELD_NAME_MGROUP = 0
FIELD_NAME_MGROUP = 1
SPECIFIC_ARRAY_FIELD_MGROUP = 2
RANGE_MGROUP = 3
START_RANGE_MGROUP = 4
END_RANGE_MGROUP = 5
LABEL_MGROUP = 6

def __create_regex():
    return re.compile("(?:\\.?\\'([^\\']+)\\')|(?:\\.?([^\\:\\.\\[\\]]+))|(?:\\[(?:(-?[0-9])+|((-?[0-9]+)?:(-?[0-9]+)?)))\\]|(?:\\:(.+)$)")

def __mask_key(field_name):
    try:
        field_name.index('.')
        return "'{}'".format(field_name)
    except:
        return field_name

def __wrap_index(idx, arr):
    if idx < 0:
        idx += len(arr)

    if idx < 0 or idx > len(arr):
        raise Exception('Index {} is out of bounds'.format(idx))

    return idx

def __parse_int(value, default_value=None, radix=10):
    try:
        return int(value, base=radix)
    except:
        if default_value is None:
            raise Exception('Default value is not defined')

        try:
            return int(default_value)
        except:
            raise Exception('Neither parsed value {} nor default value are numbers'.format(value))

def __int_range(start, end):
    rng = []

    while start != end:
        rng.append(start)
        start += 1 if end > start else -1

    return rng

def __check_if_match_exists_at(match, index):
    return match[index] != ''

# pylint: disable=unused-argument
def __replace_value_at_path(replacement, value, parent, field, query):
    parent[field] = replacement

# pylint: disable=unused-argument
def __replace_matching_value_at_path(replacement, value, parent, field, query):
    try:
        result = json_query_first(replacement, query)
        parent[field] = result['value']
    except:
        pass

def json_query(value, query='', options=None, match_index=0, normalized_query='', parent=None, field=None, matches=None, result=None):
    if options is None:
        options = {
            'modify': lambda value, parent, field, normalized_query: value,
            'limit': -1,
            '$cnt': 0
        }

    if result is None:
        result = []

    if 'limit' not in options:
        options['limit'] = -1

    match = None

    if matches is None:
        matches = re.findall(__create_regex(), query)

    if parent is None and field is None:
        # Initialize $cnt on first function call
        if '$cnt' not in options:
            options['$cnt'] = 0

    if options['limit'] > 0 and options['$cnt'] >= options['limit']:
        return result

    for i in range(match_index, len(matches)):
        match = matches[i]

        # Either first or second matching groups are actual field values
        # We have to distinguish between masked field names [1] and non-masked field names [2]
        field_match = match[MASKED_FIELD_NAME_MGROUP]

        if not __check_if_match_exists_at(match, MASKED_FIELD_NAME_MGROUP):
            field_match = match[FIELD_NAME_MGROUP]

        if field_match != '':
            if field_match == '*':
                for key in value:
                    json_query(value[key], query, options, i + 1, f'{normalized_query}.{__mask_key(key)}', value, key, matches, result)

                return result

            if field_match not in value:
                raise Exception('Path {} does not exist'.format(field_match))

            normalized_query = '{}.{}'.format(normalized_query, __mask_key(field_match))
            parent = value
            field = field_match
            value = value[field_match]
            continue

        if __check_if_match_exists_at(match, LABEL_MGROUP):
            break

        if not isinstance(value, list):
            raise Exception('Index accessors only work on Arrays')

        if __check_if_match_exists_at(match, SPECIFIC_ARRAY_FIELD_MGROUP):
            idx = __wrap_index(__parse_int(match[SPECIFIC_ARRAY_FIELD_MGROUP]), value)
            normalized_query = f'{normalized_query}[{idx}]'
            parent = value
            field = idx
            value = value[idx]
            continue

        if not __check_if_match_exists_at(match, RANGE_MGROUP):
            raise Exception('Index accessor without range')

        if len(value) == 0:
            return result

        start_range = __wrap_index(__parse_int(match[START_RANGE_MGROUP], 0), value)
        # Skip last element if given
        end_range = __wrap_index(__parse_int(match[END_RANGE_MGROUP], len(value)), value)

        if start_range >= end_range:
            return result

        for idx in __int_range(start_range, end_range):
            json_query(value[idx], query, options, i + 1, '{}[{}]'.format(normalized_query, idx), value, idx, matches, result)

        return result

    if len(normalized_query) > 0 and normalized_query[0] == '.':
        normalized_query = normalized_query[1:]

    label = match[LABEL_MGROUP] if match is not None and __check_if_match_exists_at(match, LABEL_MGROUP) else None

    if 'modify' in options:
        options['modify'](value, parent, field, normalized_query)

        if parent is not None:
            value = parent[field]

    # Increment iteration counter
    options['$cnt'] += 1

    result.append({
        'value': value,
        'label': label,
        'query': '{}{}'.format(normalized_query, ':{}'.format(label) if label is not None else '')
    })

    return result

def json_query_first(data, query, options=None):
    if options is None:
        options = {}

    if 'limit' not in options:
        options['limit'] = 1

    for result in json_query(data, query, options):
        return result

    raise Exception('No match found for query {}'.format(query))

def json_query_update_first(data, query, value):
    json_query_first(data, query, {
        'modify': partial(__replace_value_at_path, value)
    })

    return data

def json_query_update_all(data, query, value):
    json_query(data, query, {
        'modify': partial(__replace_matching_value_at_path, value)
    })

    return data
