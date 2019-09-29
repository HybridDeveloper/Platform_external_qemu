// Copyright 2015 The Android Open Source Project
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include "android/emulation/control/interceptor/LoggingInterceptor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <stdlib.h>
#include "waterfall.pb.h"

namespace android {
namespace control {
namespace interceptor {

using namespace testing;
using namespace grpc;
using namespace grpc::experimental;

class MockInterceptorBatchMethods : public InterceptorBatchMethods {
public:
    MOCK_METHOD(bool,
                QueryInterceptionHookPoint,
                (InterceptionHookPoints),
                (override));
    MOCK_METHOD(void, Proceed, (), (override));
    MOCK_METHOD(void, Hijack, (), (override));
    MOCK_METHOD(ByteBuffer*, GetSerializedSendMessage, (), (override));
    MOCK_METHOD(const void*, GetSendMessage, (), (override));
    MOCK_METHOD(void, ModifySendMessage, (const void*), (override));
    MOCK_METHOD(bool, GetSendMessageStatus, (), (override));
    MOCK_METHOD((std::multimap<grpc::string, grpc::string>*),
                GetSendInitialMetadata,
                (),
                (override));
    MOCK_METHOD(Status, GetSendStatus, (), (override));
    MOCK_METHOD(void, ModifySendStatus, (const Status&), (override));
    MOCK_METHOD((std::multimap<grpc::string, grpc::string>*),
                GetSendTrailingMetadata,
                (),
                (override));
    MOCK_METHOD(void*, GetRecvMessage, (), (override));
    MOCK_METHOD((std::multimap<grpc::string_ref, grpc::string_ref>*),
                GetRecvInitialMetadata,
                (),
                (override));
    MOCK_METHOD(Status*, GetRecvStatus, (), (override));
    MOCK_METHOD((std::multimap<grpc::string_ref, grpc::string_ref>*),
                GetRecvTrailingMetadata,
                (),
                (override));
    MOCK_METHOD((std::unique_ptr<ChannelInterface>),
                GetInterceptedChannel,
                (),
                (override));
    MOCK_METHOD(void, FailHijackedRecvMessage, (), (override));
    MOCK_METHOD(void, FailHijackedSendMessage, (), (override));
};

TEST(LoggingInterceptor, LoggerForwardsTheCall) {
    MockInterceptorBatchMethods batchMethods;
    EXPECT_CALL(batchMethods, Proceed());
    EXPECT_CALL(batchMethods, QueryInterceptionHookPoint(_)).Times(AtLeast(1));

    InvocationRecord record;
    ReportingFunction report = [&record](const InvocationRecord& invocation) {
        record = invocation;
    };

    auto factory = std::make_unique<LoggingInterceptorFactory>(report);
    auto interceptor = std::unique_ptr<Interceptor>(
            factory->CreateServerInterceptor(nullptr));
    interceptor->Intercept(&batchMethods);
}

TEST(LoggingInterceptor, LoggerRecordsData) {
    waterfall::Transfer msg;
    msg.set_path("/a/b/c/d");
    msg.set_success(true);

    MockInterceptorBatchMethods batchMethods;

    EXPECT_CALL(batchMethods, Proceed());
    EXPECT_CALL(batchMethods, GetSendStatus())
            .WillOnce(Return(Status::CANCELLED));
    EXPECT_CALL(batchMethods, GetRecvMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, GetSendMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, QueryInterceptionHookPoint(_))
            .WillRepeatedly(Return(true));

    InvocationRecord record;
    ReportingFunction report = [&record](const InvocationRecord& invocation) {
        record = invocation;
    };
    {
        auto factory = std::make_unique<LoggingInterceptorFactory>(report);
        auto interceptor = std::unique_ptr<Interceptor>(
                factory->CreateServerInterceptor(nullptr));
        interceptor->Intercept(&batchMethods);
    }

    EXPECT_TRUE(record.duration > 0);
    EXPECT_STREQ(record.method.c_str(), "unknown");
    EXPECT_EQ(record.status.error_code(), Status::CANCELLED.error_code());
    EXPECT_EQ(record.rcvBytes, msg.SpaceUsed());
    EXPECT_EQ(record.sndBytes, msg.SpaceUsed());
    EXPECT_EQ(record.response, msg.ShortDebugString());
}

TEST(LoggingInterceptor, LoggerDoesNotLogLargeMessages) {
    waterfall::Transfer msg;
    msg.set_path("/a/b/c/d");
    msg.set_payload(std::string(8192, 'a'));
    msg.set_success(true);

    MockInterceptorBatchMethods batchMethods;

    EXPECT_CALL(batchMethods, Proceed());
    EXPECT_CALL(batchMethods, GetSendStatus())
            .WillOnce(Return(Status::CANCELLED));
    EXPECT_CALL(batchMethods, GetRecvMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, GetSendMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, QueryInterceptionHookPoint(_))
            .WillRepeatedly(Return(true));

    InvocationRecord record;
    ReportingFunction report = [&record](const InvocationRecord& invocation) {
        record = invocation;
    };
    {
        auto factory = std::make_unique<LoggingInterceptorFactory>(report);
        auto interceptor = std::unique_ptr<Interceptor>(
                factory->CreateServerInterceptor(nullptr));
        interceptor->Intercept(&batchMethods);
    }

    EXPECT_EQ(record.response, "...");
}

TEST(LoggingInterceptor, LoggerSnipsOutLongParameters) {
    waterfall::Transfer msg;
    msg.set_path("/a/b/c/d");
    msg.set_payload(std::string(512, 'a'));
    msg.set_success(true);

    MockInterceptorBatchMethods batchMethods;

    EXPECT_CALL(batchMethods, Proceed());
    EXPECT_CALL(batchMethods, GetSendStatus())
            .WillOnce(Return(Status::CANCELLED));
    EXPECT_CALL(batchMethods, GetRecvMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, GetSendMessage()).WillOnce(Return(&msg));
    EXPECT_CALL(batchMethods, QueryInterceptionHookPoint(_))
            .WillRepeatedly(Return(true));

    InvocationRecord record;
    ReportingFunction report = [&record](const InvocationRecord& invocation) {
        record = invocation;
    };
    {
        auto factory = std::make_unique<LoggingInterceptorFactory>(report);
        auto interceptor = std::unique_ptr<Interceptor>(
                factory->CreateServerInterceptor(nullptr));
        interceptor->Intercept(&batchMethods);
    }

    EXPECT_EQ(record.response,
              "path: \"/a/b/c/d\" payload: "
              "\"aaaaaaaaaaaaaaaaaaaa...<truncated>...\" success: true");
}

TEST(LoggingInterceptor, LoggerOnlyLogsFirstMsg) {
    waterfall::Transfer msg;
    msg.set_path("a/b");

    waterfall::Transfer msg2;
    msg2.set_path("c/d");

    waterfall::Transfer msg3;
    msg3.set_path("e/f");

    MockInterceptorBatchMethods batchMethods;

    EXPECT_CALL(batchMethods, Proceed()).Times(3);
    EXPECT_CALL(batchMethods, GetSendStatus()).Times(3);

    EXPECT_CALL(batchMethods, GetRecvMessage())
            .WillOnce(Return(&msg))
            .WillOnce(Return(&msg2))
            .WillOnce(Return(&msg3));
    EXPECT_CALL(batchMethods, GetSendMessage())
            .WillOnce(Return(&msg))
            .WillOnce(Return(&msg2))
            .WillOnce(Return(&msg3));
    EXPECT_CALL(batchMethods, QueryInterceptionHookPoint(_))
            .WillRepeatedly(Return(true));

    InvocationRecord record;
    ReportingFunction report = [&record](const InvocationRecord& invocation) {
        record = invocation;
    };

    EXPECT_TRUE(record.rcvBytes == 0);
    {
        auto factory = std::make_unique<LoggingInterceptorFactory>(report);
        auto interceptor = std::unique_ptr<Interceptor>(
                factory->CreateServerInterceptor(nullptr));
        interceptor->Intercept(&batchMethods);

        // Streaming, 2nd & 3rd message comes in.
        interceptor->Intercept(&batchMethods);
        interceptor->Intercept(&batchMethods);
    }

    // We only recorded the first incoming/response, not the 2nd/3rd.
    EXPECT_TRUE(record.rcvBytes > 0);
    EXPECT_EQ(record.response, msg.ShortDebugString());
    EXPECT_EQ(record.incoming, msg.ShortDebugString());
}

}  // namespace interceptor
}  // namespace control
}  // namespace android
