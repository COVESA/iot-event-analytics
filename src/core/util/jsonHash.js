const xxHash = require('xxhashjs')
const stringify = require('json-stringify-deterministic')

/**
 * Module JSON Hash.
 * 
 * @module jsonHash
 */

/**
 * The json object is stringified by sorting the keys, removing spaces and new lines. It is then passed to a function to
 * calculate the hash code.
 * 
 * @param {*} jsonObject - A json object.
 * @returns a string representing 64-bit hash
 */
module.exports = function jsonHash(jsonObject) {
    const canonicalJsonObject = stringify(jsonObject)
    return xxHash.h64(canonicalJsonObject, 0).toString(16)
}


