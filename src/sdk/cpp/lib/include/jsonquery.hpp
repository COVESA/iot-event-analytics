/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#ifndef SRC_SDK_CPP_LIB_INCLUDE_JSONQUERY_HPP_
#define SRC_SDK_CPP_LIB_INCLUDE_JSONQUERY_HPP_

#include <ostream>
#include <regex>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

/**
 * @brief A very simple tokenizer used for parsing JSON queries
 */
class Tokenizer {
   public:
    /**
     * @brief Construct a new Tokenizer object.
     *
     * @param str A string to tokenizer
     * @param delimiters A string containing token delimiters
     */
    Tokenizer(const std::string& str, const std::string& delimiters);

    /**
     * @brief Get the next token.
     *
     * @return std::string
     */
    std::string Next();

    /**
     * @brief Push back token last returned by Next(). At least on call to
     * Next() must preceed one call to PushBack().
     */
    void PushBack();

    /**
     * @brief Tell whether this tokenizer has more more tokens.
     *
     * @return bool
     */
    bool HasNext() const;

   private:
    friend std::ostream& operator<<(std::ostream& os, const Tokenizer& t);

    const std::string str_;
    const std::string delimiters_;
    std::string word_;
    std::string last_;
    std::size_t idx_ = 0;
};

/**
 * @brief QueryResult represents a single JSON query result.
 */
class QueryResult {
   public:
    /**
     * @brief Construct a new QueryResult object.
     *
     * @param query The resolved JSON query
     * @param label The label associated with the query
     * @param value The query result value
     */
    QueryResult(const std::string& query, const std::string& label, const json& value);

    /**
     * @brief Compare two QueryResults for equality.
     *
     * @return bool
     */
    bool operator==(const QueryResult& other) const;


    /**
     * @brief Get the resolved JSON query.
     *
     * @return std::string
     */
    std::string GetQuery() const;

    /**
     * @brief Get the label associated with the query.
     *
     * @return std:.string
     */
    std::string GetLabel() const;

    /**
     * @brief Get the resulting query value
     *
     * @return json
     */
    json GetValue() const;

   private:
    friend std::ostream& operator<<(std::ostream& os, const QueryResult& r);

    std::string query_;
    std::string label_;
    json value_;
};

class JsonQueryException : public std::exception {
   public:
       enum class Code {
           KEY_NOT_FOUND,
           INVALID_RANGE_PARAMETER,
           INVALID_RANGE,
           OUT_OF_BOUNDS,
           UNTERMINATED_RANGE,
           INVALID_QUERY,
       };

       explicit JsonQueryException(const std::string& msg, const Code& code);

       virtual ~JsonQueryException() = default;

       virtual const char* what() const noexcept;

       virtual Code GetCode() const noexcept;

   private:
    std::string msg_;
    Code code_;
};

/**
 * @brief JsonQuery extracts values from a JSON object using a specialized query syntax.
 *
 * A query consists of one or more period delimited positional attributes and
 * ranges followed by a single label.
 *
 * Attribute:
 *  - A literal JSON object key
 *  - A single quoted JSON object key, required if the key contains any of the characters .[]:
 *    Attributes containing single quotes are not supported.
 *  - A '*' wildcard meaning any key at the given position should be included in the result.
 *
 * Range:
 *  - [:]   extract all elements in an array
 *  - [N]   extract the N:th element in an array
 *  - [-N]  extract the N:th element counting from the end of the array, -1
 *          denotes the last element of the array, -2 the second to last element and so on.
 *  - [M:N] extract elements M through N (not inclusive). If M=N an empty result is returned.
 *          If M > N an exception is thrown
 *
 * Label:
 *  - :<label> where <label> is a string string of non zero length not
 *             containing any of the characters .[]:'
 *
 * Example 1:
 * Given the object
 *
 * @code
    {
        "foo": { "bar": "baz" } 
    }
 * @endcode
 *
 * The query "foo.bar:my_label" would return
 *
 * @code
    std::vector<QueryResult>{
        QueryResult{"foo.bar", "my_label", json("baz")}
    }
 * @endcode
 *
 * Example 2:
 * Given the object
 *
 * @code
    {
        "foo": {
            "bar": {
                "alpha": [
                    { "foo.bar": 1, "bar.foo": 2 },
                    { "foo.bar": 3, "bar.foo": 4 },
                    { "foo.bar": 5, "bar.foo": 6 },
                    { "foo.bar": 7, "bar.foo": 8 }
                ],
                "beta": [
                    { "foo.bar": 9, "bar.foo": 10 },
                    { "foo.bar": 11, "bar.foo": 12 },
                    { "foo.bar": 13, "bar.foo": 14 },
                    { "foo.bar": 15, "bar.foo": 16 }
                ]
            }
        }
    }
 * @endcode
 *
 * The query "foo.bar.*[1:3].'foo.bar':my_label" would return
 *
 * @code
    std::vector<QueryResult>{
        QueryResult{"foo.bar.alpha[1].'foo.bar'", "my_label", json(3)},
        QueryResult{"foo.bar.alpha[2].'foo.bar'", "my_label", json(5)},
        QueryResult{"foo.bar.beta[1].'foo.bar'", "my_label", json(11)},
        QueryResult{"foo.bar.beta[2].'foo.bar'", "my_label", json(13)}
    }
 * @endcode
 */
class JsonQuery {
   public:
    /**
     * @brief Construct a new QueryParser object.
     * 
     * @param query The JSON query
     */
    explicit JsonQuery(const std::string& query);

    /**
     * @brief Apply query to a JSON object.
     *
     * @param value The JSON object to query
     * @return std::vector<QueryResult>
     */
    std::vector<QueryResult> Query(const json& value);

   private:
    enum class State {
        PARSE_INIT,
        PARSE_ATTR,
        PARSE_SEPARATOR,
        PARSE_QUOTE,
        PARSE_RANGE_OPEN,
        PARSE_RANGE,
        PARSE_RANGE_CLOSE,
        PARSE_LABEL,
    };

    std::string StateToString(const State& state) const;
    bool StringToInt(const std::string& str, int& val);
    std::vector<QueryResult> ParseInternal(State state, Tokenizer tok, std::string path, const json& value);

    const std::string query_;

};

}  // namespace core
}  // namespace iotea

#endif // SRC_SDK_CPP_LIB_INCLUDE_JSONQUERY_HPP_
