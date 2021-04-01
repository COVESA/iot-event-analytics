/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

const MASKED_FIELD_NAME_MGROUP = 1;
const FIELD_NAME_MGROUP = 2;
const SPECIFIC_ARRAY_FIELD_MGROUP = 3;
const RANGE_MGROUP = 4;
const START_RANGE_MGROUP = 5;
const END_RANGE_MGROUP = 6;
const LABEL_MGROUP = 7;
function* jsonQuery(value, query = '', options = { modify: v => v, limit: -1, $cnt: 0 }, regex = __createRegex(), normalizedQuery = '', parent = null, field = null) {
    if (parent === null && field === null) {
        // Initialize $cnt on first function call
        options = Object.assign(options, {
            $cnt: 0
        });
    }

    if (options.limit > 0 && options.$cnt >= options.limit) {
        return;
    }

    let match = null;

    while ((match = regex.exec(query)) !== null) {
        // Either first or second matching groups are actual field values
        // We have to distinguish between masked field names [1] and non-masked field names [2]
        let fieldMatch = match[MASKED_FIELD_NAME_MGROUP];

        if (fieldMatch === undefined) {
            fieldMatch = match[FIELD_NAME_MGROUP];
        }

        if (fieldMatch !== undefined) {
            if (fieldMatch === '*') {
                for (let key of Object.keys(value)) {
                    const li = regex.lastIndex;
                    yield * jsonQuery(value[key], query, options, regex, `${normalizedQuery}.${__maskKey(key)}`, value, key);
                    regex.lastIndex = li;
                }

                return;
            }

            if (!(typeof value === 'object') || !Object.prototype.hasOwnProperty.call(value, fieldMatch)) {
                throw new Error(`Path ${fieldMatch} does not exist`);
            }

            normalizedQuery = `${normalizedQuery}.${__maskKey(fieldMatch)}`;
            parent = value;
            field = fieldMatch;
            value = value[fieldMatch];
            continue;
        }

        if (match[LABEL_MGROUP] !== undefined) {
            break;
        }

        if (!Array.isArray(value)) {
            throw new Error(`Index accessors only work on Arrays`);
        }

        if (match[SPECIFIC_ARRAY_FIELD_MGROUP] !== undefined) {
            // Specific field in array
            const idx = __wrapIndex(__parseInt(match[SPECIFIC_ARRAY_FIELD_MGROUP]), value);
            normalizedQuery = `${normalizedQuery}[${idx}]`;
            parent = value;
            field = idx;
            value = value[idx];
            continue;
        }

        if (match[RANGE_MGROUP] === undefined) {
            throw new Error(`Index accessor without range`);
        }

        if (value.length === 0) {
            return;
        }

        let startRange = __wrapIndex(__parseInt(match[START_RANGE_MGROUP], 0), value);
        let endRange = __wrapIndex(__parseInt(match[END_RANGE_MGROUP], value.length - 1), value);

        for (let idx of __intRange(startRange, endRange)) {
            const li = regex.lastIndex;
            yield * jsonQuery(value[idx], query, options, regex, `${normalizedQuery}[${idx}]`, value, idx);
            regex.lastIndex = li;
        }

        return;
    }

    if (normalizedQuery[0] === '.') {
        normalizedQuery = normalizedQuery.substring(1);
    }

    const label = match && match[LABEL_MGROUP] ? match[LABEL_MGROUP] : null;

    if (options.modify) {
        options.modify(value, parent, field, normalizedQuery);

        if (parent !== null) {
            value = parent[field];
        }
    }

    // Increment iteration counter
    options.$cnt++;

    yield {
        value,
        label,
        query: normalizedQuery + (label ? ':' + label : '')
    };
}

function __maskKey(fieldName) {
    if (fieldName.indexOf('.') === -1) {
        return fieldName;
    }

    return `'${fieldName}'`;
}

function* __intRange(start, end) {
    while(start !== end) {
        yield start;
        start += end > start ? 1 : -1;
    }

    yield start;
}

function __createRegex() {
    return new RegExp(`(?:\\.?\\'([^\\']+)\\')|(?:\\.?([^\\:\\.\\[\\]]+))|(?:\\[(?:(-?[0-9])+|((-?[0-9]+)?:(-?[0-9]+)?)))\\]|(?:\\:(.+)$)`, 'g');
}

function __parseInt(value, defaultValue, radix = 10) {
    const p = parseInt(value, radix);

    if (isNaN(p)) {
        if (defaultValue === undefined || !Number.isFinite(defaultValue)) {
            throw new Error(`Neither parsed valuevalue ${value} nor default value are numbers`);
        }

        return defaultValue;
    }

    return p;
}

function __wrapIndex(idx, arr) {
    if (idx < 0) {
        idx += arr.length;
    }

    if (idx < 0 || idx >= arr.length) {
        throw new Error(`Index ${idx} is out of bounds`);
    }

    return idx;
}

jsonQuery.first = (data, query, options = {}) => {
    for (const result of jsonQuery(data, query, Object.assign(options, { limit: 1 }))) {
        return result;
    }

    throw new Error(`No match found for query ${query}`);
};

jsonQuery.updateFirst = (data, query, value) => {
    jsonQuery.first(data, query, {
        modify: (v, p, f) => {
            p[f] = value;
        }
    });

    return data;
};

jsonQuery.updateAll = (data, query, value) => {
    Array.from(jsonQuery(data, query, {
        modify: (v, p, f, q) => {
            try {
                const result = jsonQuery.first(value, q);
                p[f] = result.value;
            }
            catch(err) {
                // Field could not be found in replacement values. Skip that
            }
        }
    }));

    return data;
};

module.exports = jsonQuery;