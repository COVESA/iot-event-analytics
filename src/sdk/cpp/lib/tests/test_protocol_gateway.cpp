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
#include "gmock/gmock.h"
#include "nlohmann/json.hpp"

#include "protocol_gateway.hpp"

using json = nlohmann::json;

using iotea::core::Adapter;
using iotea::core::ProtocolGateway;
using iotea::core::PubSubOptions;
using iotea::core::PublishOptions;
using iotea::core::SubscribeOptions;
using iotea::core::on_msg_func_ptr;
using ::testing::StrictMock;

TEST(protocol_gateway, PubSubOptions) {
    struct {
        bool platform_proto_only;
        std::string adapter_id;
    } tests[] {
        {
            true,
            ""
        },
        {
            false,
            ""
        },
        {
            true,
            "my_adapter"
        },
        {
            false,
            "my_adapter"
        }
    };

    for (const auto& t : tests) {
        PubSubOptions opt{t.platform_proto_only, t.adapter_id};
        ASSERT_EQ(opt.IsPlatformProtoOnly(), t.platform_proto_only);
        ASSERT_EQ(opt.GetAdapterId(), t.adapter_id);
    }
}

TEST(protocol_gateway, PublishOptions) {
    // This test only verifies that the default values are what's expected.
    PublishOptions opts{false, "my_adapter"};
    ASSERT_EQ(opts.IsPlatformProtoOnly(), false);
    ASSERT_EQ(opts.GetAdapterId(), "my_adapter");
    ASSERT_FALSE(opts.Retain());
    ASSERT_TRUE(opts.Stash());
}

TEST(protocol_gateway, SubscribeOptions) {
    // This test only verifies that the default values are what's expected.
    SubscribeOptions opts{false, "my_adapter"};
    ASSERT_EQ(opts.IsPlatformProtoOnly(), false);
    ASSERT_EQ(opts.GetAdapterId(), "my_adapter");
}

class MockAdapter : public Adapter {
   public:
    MockAdapter(const std::string& name, bool is_platform_proto) : Adapter(name, is_platform_proto) {}

    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Stop, (), (override));


    MOCK_METHOD(void, Publish, (const std::string&, const std::string&, const PublishOptions&), (override));

    MOCK_METHOD(void, Subscribe, (const std::string&, on_msg_func_ptr, const SubscribeOptions&), (override));

    MOCK_METHOD(void, SubscribeShared, (const std::string&, const std::string&, on_msg_func_ptr, const SubscribeOptions&), (override));

   protected:
    std::string name_;
    bool is_platform_proto_;
};

TEST(protocol_gateway, Add) {
    class MockProtocolGateway : public ProtocolGateway {
       public:
        MockProtocolGateway(bool platform_proto_only) : ProtocolGateway("TestProtocolGateway", platform_proto_only) {}

        // Make Add() public for this test
        bool Add(std::shared_ptr<Adapter> adapter) { return ProtocolGateway::Add(adapter); }
    };

    auto proto_adapter = std::make_shared<MockAdapter>("proto_adatper", true);
    auto non_proto_adapter = std::make_shared<MockAdapter>("non_proto_adapter", false);

    // A "platform only" gateway
    MockProtocolGateway proto_gw{true};

    // If the gatway is "platform only", then only adapters tagged "platform
    // only" should be addable.
    ASSERT_TRUE(proto_gw.Add(proto_adapter));
    ASSERT_FALSE(proto_gw.Add(non_proto_adapter));

    // A non "platform only" gateway
    MockProtocolGateway non_proto_gw{false};

    // If the gatway is "platform only", then adapters should be addable
    // regardless of their "platform only" status.
    ASSERT_TRUE(non_proto_gw.Add(proto_adapter));
    ASSERT_TRUE(non_proto_gw.Add(non_proto_adapter));
}

TEST(protocol_gateway, StartStop) {
    class MockProtocolGateway : public ProtocolGateway {
       public:
        MockProtocolGateway(bool platform_proto_only) : ProtocolGateway("TestProtocolGateway", platform_proto_only) {}

        // Make Add() public for this test
        bool Add(std::shared_ptr<Adapter> adapter) { return ProtocolGateway::Add(adapter); }
    };

    auto a1 = std::make_shared<MockAdapter>("a1", true);
    auto a2 = std::make_shared<MockAdapter>("a2", true);
    auto a3 = std::make_shared<MockAdapter>("a3", true);

    MockProtocolGateway gateway{true};

    gateway.Add(a1);
    gateway.Add(a2);
    gateway.Add(a3);

    EXPECT_CALL(*a1, Start());
    EXPECT_CALL(*a2, Start());
    EXPECT_CALL(*a3, Start());
    gateway.Start();

    EXPECT_CALL(*a1, Stop());
    EXPECT_CALL(*a2, Stop());
    EXPECT_CALL(*a3, Stop());
    gateway.Stop();
}

TEST(protocol_gateway, Publish) {
    // In this test we use a StrictMock tor wrap the ProtocolGateway, this
    // assures that the test fails if an unexpected call (i.e. a call that has
    // not ben explicitly registered with EXPECT_CALL) is made.

    class MockProtocolGateway : public ProtocolGateway {
       public:
        MockProtocolGateway(bool platform_proto_only) : ProtocolGateway("TestProtocolGateway", platform_proto_only) {}

        // Make Add() public for this test
        bool Add(std::shared_ptr<Adapter> adapter) { return ProtocolGateway::Add(adapter); }
    };


    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Publish opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        PublishOptions opts{true, ""};
        EXPECT_CALL(*adapter1, Publish("test/topic", "test_message", opts));
        EXPECT_CALL(*adapter2, Publish("test/topic", "test_message", opts));
        gw.Publish("test/topic", "test_message", opts);
    }


    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is not set
    // Publish opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        PublishOptions opts{true, ""};
        EXPECT_CALL(*adapter1, Publish("test/topic", "test_message", opts));
        gw.Publish("test/topic", "test_message", opts);
    }

    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Publish opt - platform proto is set
    //             - name is set to "adapter1"
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        PublishOptions opts{true, "adapter1"};
        EXPECT_CALL(*adapter1, Publish("test/topic", "test_message", opts));
        gw.Publish("test/topic", "test_message", opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Publish opt - platform proto is not set
    //             - name is set to ""
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        PublishOptions opts{false, ""};
        EXPECT_CALL(*adapter1, Publish("test/topic", "test_message", opts));
        EXPECT_CALL(*adapter2, Publish("test/topic", "test_message", opts));
        gw.Publish("test/topic", "test_message", opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Publish opt - platform proto is set
    //             - name is set to ""
    //
    // Expect call to not be forwarded to all adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        PublishOptions opts{false, ""};
        EXPECT_CALL(*adapter1, Publish("test/topic", "test_message", opts));
        EXPECT_CALL(*adapter2, Publish("test/topic", "test_message", opts));
        gw.Publish("test/topic", "test_message", opts);
    }
}

TEST(protocol_gateway, Subscribe) {
    // In this test we use a StrictMock tor wrap the ProtocolGateway, this
    // assures that the test fails if an unexpected call (i.e. a call that has
    // not ben explicitly registered with EXPECT_CALL) is made.

    class MockProtocolGateway : public ProtocolGateway {
       public:
        MockProtocolGateway(bool platform_proto_only) : ProtocolGateway("TestProtocolGateway", platform_proto_only) {}

        // Make Add() public for this test
        bool Add(std::shared_ptr<Adapter> adapter) { return ProtocolGateway::Add(adapter); }
    };

    on_msg_func_ptr cb = [](const std::string&, const std::string&, const std::string&){};

    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Subscribe opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, ""};
        EXPECT_CALL(*adapter1, Subscribe("test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, Subscribe("test/topic", ::testing::_, opts));
        gw.Subscribe("test/topic", cb, opts);
    }


    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, ""};
        EXPECT_CALL(*adapter1, Subscribe("test/topic", ::testing::_, opts));
        gw.Subscribe("test/topic", cb, opts);
    }

    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Subscribe opt - platform proto is set
    //             - name is set to "adapter1"
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, "adapter1"};
        EXPECT_CALL(*adapter1, Subscribe("test/topic", ::testing::_, opts));
        gw.Subscribe("test/topic", cb, opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is not set
    //             - name is set to ""
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{false, ""};
        EXPECT_CALL(*adapter1, Subscribe("test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, Subscribe("test/topic", ::testing::_, opts));
        gw.Subscribe("test/topic", cb, opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is set
    //             - name is set to ""
    //
    // Expect call to not be forwarded to all adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{false, ""};
        EXPECT_CALL(*adapter1, Subscribe("test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, Subscribe("test/topic", ::testing::_, opts));
        gw.Subscribe("test/topic", cb, opts);
    }
}

TEST(protocol_gateway, SubscribeShared) {
    // In this test we use a StrictMock tor wrap the ProtocolGateway, this
    // assures that the test fails if an unexpected call (i.e. a call that has
    // not ben explicitly registered with EXPECT_CALL) is made.

    class MockProtocolGateway : public ProtocolGateway {
       public:
        MockProtocolGateway(bool platform_proto_only) : ProtocolGateway("TestProtocolGateway", platform_proto_only) {}

        // Make Add() public for this test
        bool Add(std::shared_ptr<Adapter> adapter) { return ProtocolGateway::Add(adapter); }
    };

    on_msg_func_ptr cb = [](const std::string&, const std::string&, const std::string&){};

    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Subscribe opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, ""};
        EXPECT_CALL(*adapter1, SubscribeShared("group", "test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, SubscribeShared("group", "test/topic", ::testing::_, opts));
        gw.SubscribeShared("group", "test/topic", cb, opts);
    }


    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is set
    //             - name is not set
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, ""};
        EXPECT_CALL(*adapter1, SubscribeShared("group", "test/topic", ::testing::_, opts));
        gw.SubscribeShared("group", "test/topic", cb, opts);
    }

    // GW - platform proto is set
    // Adapter1 - platform proto is set
    // Adapter2 - platform proto is set
    // Subscribe opt - platform proto is set
    //             - name is set to "adapter1"
    //
    // Expect call to be forwarded to adapter1
    {
        StrictMock<MockProtocolGateway> gw(true);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", true);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", true);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{true, "adapter1"};
        EXPECT_CALL(*adapter1, SubscribeShared("group", "test/topic", ::testing::_, opts));
        gw.SubscribeShared("group", "test/topic", cb, opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is not set
    //             - name is set to ""
    //
    // Expect call to be forwarded to both adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{false, ""};
        EXPECT_CALL(*adapter1, SubscribeShared("group", "test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, SubscribeShared("group", "test/topic", ::testing::_, opts));
        gw.SubscribeShared("group", "test/topic", cb, opts);
    }

    // GW - platform proto is not set
    // Adapter1 - platform proto is not set
    // Adapter2 - platform proto is not set
    // Subscribe opt - platform proto is set
    //             - name is set to ""
    //
    // Expect call to not be forwarded to all adapters
    {
        StrictMock<MockProtocolGateway> gw(false);
        auto adapter1 = std::make_shared<MockAdapter>("adapter1", false);
        auto adapter2 = std::make_shared<MockAdapter>("adapter2", false);
        gw.Add(adapter1);
        gw.Add(adapter2);

        SubscribeOptions opts{false, ""};
        EXPECT_CALL(*adapter1, SubscribeShared("group", "test/topic", ::testing::_, opts));
        EXPECT_CALL(*adapter2, SubscribeShared("group", "test/topic", ::testing::_, opts));
        gw.SubscribeShared("group", "test/topic", cb, opts);
    }
}
