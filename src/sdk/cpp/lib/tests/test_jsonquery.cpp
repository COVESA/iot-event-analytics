/*****************************************************************************
 * Copyright (c) 2021 Bosch.IO GmbH
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * SPDX-License-Identifier: MPL-2.0
 ****************************************************************************/

#include "gtest/gtest.h"
#include "nlohmann/json.hpp"

#include "jsonquery.hpp"

using json = nlohmann::json;

using iotea::core::QueryResult;
using iotea::core::Tokenizer;
using iotea::core::JsonQuery;
using iotea::core::JsonQueryException;

/**
 * @brief Verify that Tokenizer::Next returns the expected tokens.
 */
TEST(jsonquery, Tokenizer_Next) {
    const auto query = "alpha.*.beta[1:2]:label";
    auto t = Tokenizer{query, ".*[]:'"};

    // Verify that all the tokens and nothing but the tokens are returned.
    auto tokens = std::vector<std::string>{ "alpha", ".", "*", ".", "beta", "[", "1", ":", "2", "]", ":", "label" };

    for (const auto& token : tokens) {
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
    }
    ASSERT_FALSE(t.HasNext());
}

/**
 * @brief Verify that Tokenizer::PushBack permitts the caller to push the last
 * token back into the stream.
 */
TEST(jsonquery, Tokenizer_PushBack) {
    const auto query = "alpha.*.beta[1:2]:label";
    auto t = Tokenizer{query, ".*[]:'"};

    // Verify that all the tokens and nothing but the tokens are returned.
    auto tokens = std::vector<std::string>{ "alpha", ".", "*", ".", "beta", "[", "1", ":", "2", "]", ":", "label" };

    for (const auto& token : tokens) {
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
        t.PushBack();
        ASSERT_TRUE(t.HasNext());
        ASSERT_EQ(t.Next(), token);
    }
    ASSERT_FALSE(t.HasNext());
}

/**
 * @brief Verify that JsonQuery can parse the entire range of query
 * permutations.
 */
TEST(jsonquery, JsonQuery) {
    struct {
        std::string query;
        std::vector<json> value;
        std::vector<std::vector<QueryResult>> want;
    } tests[] {
        {
            "foo.bar:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                }
            }
        },
        {
            "'foo.bar'.baz:label",
            {
                json::parse(R"({ "foo.bar": { "baz": "qux" } })")
            },
            {
                {
                    QueryResult{"'foo.bar'.baz", "label", json("qux")}
                }
            }
        },
        {
            "foo.*:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({ "foo": { "bar1": "baz1", "bar2": "baz2" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")},
                },
                {
                    QueryResult{"foo.bar1", "label", json("baz1")},
                    QueryResult{"foo.bar2", "label", json("baz2")}
                }
            }
        },
        {
            "*.bar:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({ "foo1": { "bar": "baz1" }, "foo2": { "bar": "baz2" } })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                },
                {
                    QueryResult{"foo1.bar", "label", json("baz1")},
                    QueryResult{"foo2.bar", "label", json("baz2")}
                }
            }
        },
        {
            "*.*:label",
            {
                json::parse(R"({ "foo": { "bar": "baz" } })"),
                json::parse(R"({
                                "foo1": { "bar1": "baz1" },
                                "foo2": { "bar2": "baz2" },
                                "foo3": { "bar3": "baz3" }
                })")
            },
            {
                {
                    QueryResult{"foo.bar", "label", json("baz")}
                },
                {
                    QueryResult{"foo1.bar1", "label", json("baz1")},
                    QueryResult{"foo2.bar2", "label", json("baz2")},
                    QueryResult{"foo3.bar3", "label", json("baz3")}
                }
            }
        },
        {
            "foo[:][1:3]:label",
            {
                json::parse(R"({ "foo": [
                    [1, 2, 3],
                    [4, 5, 6],
                    [7, 8, 9]
                ] })"),
            },
            {
                {
                    QueryResult{"foo[0][1]", "label", json(2)},
                    QueryResult{"foo[0][2]", "label", json(3)},
                    QueryResult{"foo[1][1]", "label", json(5)},
                    QueryResult{"foo[1][2]", "label", json(6)},
                    QueryResult{"foo[2][1]", "label", json(8)},
                    QueryResult{"foo[2][2]", "label", json(9)},
                }
            }
        },
        {
            "foo.bar[:]:label",
            {
                json::parse(R"({ "foo": { "bar": [] } })"),
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [1, 2, 3] } })")
            },
            {
                {
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                    QueryResult{"foo.bar[1]", "label", json(2)},
                    QueryResult{"foo.bar[2]", "label", json(3)}
                }
            }
        },
        {
            "foo.bar[0]:label",
            {
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [2, 1] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[0]", "label", json(2)},
                }
            }
        },
        {
            "foo.bar[-1]:label",
            {
                json::parse(R"({ "foo": { "bar": [1] } })"),
                json::parse(R"({ "foo": { "bar": [1, 2] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                },
                {
                    QueryResult{"foo.bar[1]", "label", json(2)},
                }
            }
        },
        {
            "foo.bar[0:3]:label",
            {
                json::parse(R"({ "foo": { "bar": [1, 2, 3, 4, 5] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[0]", "label", json(1)},
                    QueryResult{"foo.bar[1]", "label", json(2)},
                    QueryResult{"foo.bar[2]", "label", json(3)},
                },
            }
        },
        {
            "foo.bar[2:5]:label",
            {
                json::parse(R"({ "foo": { "bar": [1, 2, 3, 4, 5] } })"),
            },
            {
                {
                    QueryResult{"foo.bar[2]", "label", json(3)},
                    QueryResult{"foo.bar[3]", "label", json(4)},
                    QueryResult{"foo.bar[4]", "label", json(5)},
                },
            }
        },
        {
            "foo.bar.*[1:3].'foo.bar':label",
            {
                json::parse(R"({
                    "foo": {
                        "bar": {
                            "alpha": [
                                {
                                    "foo.bar": 1,
                                    "bar.foo": 2
                                },
                                {
                                    "foo.bar": 3,
                                    "bar.foo": 4
                                },
                                {
                                    "foo.bar": 5,
                                    "bar.foo": 6
                                },
                                {
                                    "foo.bar": 7,
                                    "bar.foo": 8
                                }
                            ],
                            "beta": [
                                {
                                    "foo.bar": 9,
                                    "bar.foo": 10
                                },
                                {
                                    "foo.bar": 11,
                                    "bar.foo": 12
                                },
                                {
                                    "foo.bar": 13,
                                    "bar.foo": 14
                                },
                                {
                                    "foo.bar": 15,
                                    "bar.foo": 16
                                }
                            ]
                        }
                    }
                })")
            },
            {
                {
                    QueryResult{"foo.bar.alpha[1].'foo.bar'", "label", json(3)},
                    QueryResult{"foo.bar.alpha[2].'foo.bar'", "label", json(5)},
                    QueryResult{"foo.bar.beta[1].'foo.bar'", "label", json(11)},
                    QueryResult{"foo.bar.beta[2].'foo.bar'", "label", json(13)}
                }
            }
        }
    };


    for (const auto& t : tests) {
        auto q = JsonQuery{t.query};

        for (size_t i = 0; i < t.value.size(); i++) {
            auto have = q.Query(t.value[i]);
            ASSERT_EQ(have, t.want[i]);
        }
    }
}

/**
 * @brief Verify that JsonQuery throws expections as expected.
 */
TEST(jsonquery, JsonQuery_Exceptions) {
    auto obj = json::parse(R"({"foo": {"bar": [1, 2, 3, 4], "baz": [1, 2, 3, 4]}})");

    // Key not found
    ASSERT_THROW(JsonQuery("foo.car[0]:label").Query(obj), JsonQueryException);

    // Unterminated range
    ASSERT_THROW(JsonQuery("foo.bar[0:label").Query(obj), JsonQueryException);

    // Invalid range parameter
    ASSERT_THROW(JsonQuery("foo.bar[*]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[0:*]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[*:0]:label").Query(obj), JsonQueryException);


    // Invalid range (start is after end)
    ASSERT_THROW(JsonQuery("foo.bar[1:0]:label").Query(obj), JsonQueryException);

    // Out of bounds
    ASSERT_THROW(JsonQuery("foo.bar[100]:label").Query(obj), JsonQueryException);
    ASSERT_THROW(JsonQuery("foo.bar[0:100]:label").Query(obj), JsonQueryException);

    // Invalid query (missing label)
    ASSERT_THROW(JsonQuery("foo.bar[0]").Query(obj), JsonQueryException);
}
