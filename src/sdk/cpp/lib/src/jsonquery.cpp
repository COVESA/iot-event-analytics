/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"

#include "jsonquery.hpp"

using json = nlohmann::json;

namespace iotea {
namespace core {

QueryResult::QueryResult(const std::string& query, const std::string& label, const json& value)
    : query_{query}
    , label_{label}
    , value_(value) {
}

bool QueryResult::operator==(const QueryResult& other) const {
    return query_ == other.query_ &&
        label_ == other.label_ &&
        value_ == other.value_;
}

std::ostream& operator<<(std::ostream& os, const QueryResult& r) {
    return os <<
        "{\"query\": \"" << r.GetQuery() <<
        "\", \"label\": \"" << r.GetLabel() <<
        "\", \"value\": " << r.GetValue().dump() <<
        "}";
}

std::string QueryResult::GetQuery() const {
    return query_;
}

std::string QueryResult::GetLabel() const {
    return label_;
}

json QueryResult::GetValue() const {
    return value_;
}

JsonQueryException::JsonQueryException(const std::string& msg, const JsonQueryException::Code& code)
    : std::exception{}
    , msg_{msg}
    , code_{code} {}

const char* JsonQueryException::what() const noexcept {
    return msg_.c_str();
}

JsonQueryException::Code JsonQueryException::GetCode() const noexcept {
    return code_;
}

JsonQuery::JsonQuery(const std::string& query)
    : query_{query} {}

bool JsonQuery::StringToInt(const std::string& str, int& val) {
    try {
        val = std::stoi(str);
    } catch (const std::invalid_argument& e) {
        return false;
    }

    return true;
}

std::string JsonQuery::StateToString(const State& state) const {
    switch (state) {
        case State::PARSE_INIT: return "PARSE_INIT";
        case State::PARSE_ATTR: return "PARSE_ATTR";
        case State::PARSE_SEPARATOR: return "PARSE_SEPARATOR";
        case State::PARSE_QUOTE: return "PARSE_QUOTE";
        case State::PARSE_RANGE_OPEN: return "PARSE_RANGE_OPEN";
        case State::PARSE_RANGE: return "PARSE_RANGE";
        case State::PARSE_RANGE_CLOSE: return "PARSE_RANGE_CLOSE";
        case State::PARSE_LABEL: return "PARSE_LABEL";
        default: throw std::logic_error("should not happen");
    }
}

std::vector<QueryResult> JsonQuery::ParseInternal(State state, Tokenizer tok, std::string path, const json& value) {
    std::vector<QueryResult> results;
    std::string sym;
    std::string quoted_key;
    int range_from;
    int range_to;

    while (tok.HasNext()) {
        sym = tok.Next();

        switch (state) {
            case State::PARSE_INIT: {
                // The first symbol in a query must be one of
                // - a wildcard "*"
                // - a single quote "'"
                // - a key found in value

                if (sym == "*" || sym == "'" || value.contains(sym)) {
                    tok.PushBack();
                    state = State::PARSE_ATTR;
                    break;
                }

                throw JsonQueryException("invalid query", JsonQueryException::Code::INVALID_QUERY);
            }
            case State::PARSE_ATTR: {
                // A valid attribute is one of
                // - a wildcard "*"
                // - a single quote "'"
                // - a key found in value
                //
                // If sym is neither of these it should be treated as a separator

                if (sym == "*") {
                    for (const auto& v : value.items()) {
                        auto sub_results = ParseInternal(State::PARSE_SEPARATOR, tok, path + v.key(), v.value());
                        results.insert(results.end(), sub_results.begin(), sub_results.end());
                    }

                    return results;
                }

                if (sym == "'") {
                    quoted_key.clear();
                    state = State::PARSE_QUOTE;
                    break;
                }

                if (value.contains(sym)) {
                    auto sub_results = ParseInternal(State::PARSE_SEPARATOR, tok, path + sym, value[sym]);
                    results.insert(results.end(), sub_results.begin(), sub_results.end());

                    return results;
                }

                tok.PushBack();
                state = State::PARSE_SEPARATOR;
                break;
            }
            case State::PARSE_QUOTE: {
                if (sym != "'") {
                    quoted_key += sym;
                    break;
                }

                auto quoted = "'" + quoted_key + "'";
                auto sub_results = ParseInternal(State::PARSE_SEPARATOR, tok, path + quoted, value[quoted_key]);
                results.insert(results.end(), sub_results.begin(), sub_results.end());

                return results;
            }
            case State::PARSE_SEPARATOR: {
                // A speparator must be one of
                // - a period "." -> expect an attribute to follow
                // - an opening square bracket "[" -> expect a range to follow
                // - a colon ":" -> expect a label to follow

                if (sym == ".") {
                    path += ".";
                    state = State::PARSE_ATTR;
                    break;
                }

                if (sym == "[") {
                    state = State::PARSE_RANGE_OPEN;
                    break;
                }

                if (sym == ":") {
                    state = State::PARSE_LABEL;
                    break;
                }

                throw JsonQueryException{"missing separator", JsonQueryException::Code::INVALID_QUERY};
            }
            case State::PARSE_RANGE_OPEN: {
                // A range must contain either
                // - a colon ":" -> the entire array, expect a closing square bracket to follow
                // - an integer -> a specific element or the beginning of a range, continue parsing the range
                if (sym == ":") {
                    // Range given as [:
                    range_from = 0;
                    range_to = value.size();
                    state = State::PARSE_RANGE_CLOSE;
                    break;
                }

                // Expect range to begin as [M
                if (!StringToInt(sym, range_from)) {
                    throw JsonQueryException{"range parameters must be integers", JsonQueryException::Code::INVALID_RANGE_PARAMETER};
                }

                state = State::PARSE_RANGE;
                break;
            }
            case State::PARSE_RANGE: {
                // The "insides" of a range must either
                // - end immediately with a closing square bracket "]", push back and handle the "close" in a separate state
                // - contain a colon ":" followed by an integer representing the end of the range, expect a "close"
                auto size = static_cast<int>(value.size());

                // Range given as [M]
                if (sym == "]") {
                    tok.PushBack();

                    if (range_from < -size || range_from >= size) {
                        throw JsonQueryException{"range start is out of bounds", JsonQueryException::Code::OUT_OF_BOUNDS};
                    }

                    if (range_from < 0) {
                        range_from = size + range_from;
                    }

                    range_to = range_from + 1;
                    state = State::PARSE_RANGE_CLOSE;
                    break;
                }

                // Range given as [M:N
                if (sym == ":") {
                    // Remain in this state
                    break;
                }

                // Expect range to be given as [M:N]
                if (!StringToInt(sym, range_to)) {
                    throw JsonQueryException{"range parameters must be integers", JsonQueryException::Code::INVALID_RANGE_PARAMETER};
                }

                if (range_to < -size || range_to > size) {
                    throw JsonQueryException{"range end is out of bounds", JsonQueryException::Code::OUT_OF_BOUNDS};
                }

                if (range_to < 0) {
                    range_to = size + range_to;
                }

                if (range_from > range_to) {
                    throw JsonQueryException{"range start is after range end", JsonQueryException::Code::INVALID_RANGE};
                }

                state = State::PARSE_RANGE_CLOSE;
                break;
            }
            case State::PARSE_RANGE_CLOSE: {
                // A range close must begin with a closing square brace "]". Expect an attribute, a range or a label to follow
                if (sym != "]") {
                    throw JsonQueryException{"range not terminated", JsonQueryException::Code::UNTERMINATED_RANGE};
                }

                if (range_from == range_to) {
                    // An empty range yields an empty result
                    return {};
                }

                for (int i = range_from; i < range_to; i++) {
                    auto range = "[" + std::to_string(i) + "]";
                    auto sub_result = ParseInternal(State::PARSE_ATTR, tok, path + range, value[i]);
                    results.insert(results.end(), sub_result.begin(), sub_result.end());
                }

                return results;
            }
            case State::PARSE_LABEL: {
                // A label is any string i.e. everything that follows the label
                // colon. This is a terminal state in which the QueryResult is
                // created.
                auto label = tok.Next();
                while (tok.HasNext()) {
                    label += tok.Next();
                }

                results.insert(results.end(), QueryResult{path, label, value});

                return results;
            }

            default:
                throw std::logic_error("invalid state");
        }
    }

    // Any valid query will terminate in the PARSE_LABEL state
    throw JsonQueryException{"invalid query", JsonQueryException::Code::INVALID_QUERY};
}

std::vector<QueryResult> JsonQuery::Query(const json& value) {
    auto tok = Tokenizer{query_, ".:'[]"};

    return ParseInternal(State::PARSE_INIT, tok, "", value);
}

Tokenizer::Tokenizer(const std::string& str, const std::string& delimiters)
    : str_{str}
    , delimiters_{delimiters}
    , idx_{0} {}

std::string Tokenizer::Next() {
    while (idx_ < str_.size()) {
        auto c = str_[idx_++];

        if (delimiters_.find(c) != std::string::npos) {
            if (word_.empty()) {
                last_ = std::string{c};
                return last_;
            }

            idx_--;

            last_ = word_;
            word_.clear();
            return last_;
        }

        word_.push_back(c);
    }

    last_ = word_;
    return word_;
}

void Tokenizer::PushBack() {
    // TODO PushBack() is only allowed once between each Next()
    // Throw exception in case of consecutive PushBack()s
    if (last_.size() == 0) {
        throw std::logic_error("nothing to push back");
    }

    idx_ -= last_.size();

    word_.clear();
    last_.clear();
}

bool Tokenizer::HasNext() const {
    return idx_ < str_.size();
}

std::ostream& operator<<(std::ostream& os, const Tokenizer& t) {
    return os << t.str_.substr(t.idx_) << "\n";
}

}  // namespace core
}  // namespace iotea
