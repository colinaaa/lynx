// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define protected public
#define private public

#include "devtool/lynx_devtool/agent/domain_agent/inspector_accessibility_agent.h"

#include <chrono>
#include <string>
#include <thread>

#include "devtool/base_devtool/native/test/message_sender_mock.h"
#include "devtool/base_devtool/native/test/mock_receiver.h"
#include "devtool/lynx_devtool/agent/inspector_default_executor.h"
#include "devtool/testing/mock/lynx_devtool_ng_mock.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"

namespace lynx {
namespace devtool {
namespace testing {

class InspectorAccessibilityAgentTest : public ::testing::Test {
 public:
  InspectorAccessibilityAgentTest() = default;
  ~InspectorAccessibilityAgentTest() override = default;

  void SetUp() override {
    MockReceiver::GetInstance().ResetAll();
    devtool_ = std::make_shared<lynx::testing::LynxDevToolNGMock>();
    const auto& mediator = devtool_->devtool_mediator_;
    agent_ = std::make_shared<InspectorAccessibilityAgent>(mediator);
    mediator->devtool_executor_ =
        std::make_shared<InspectorDefaultExecutor>(mediator);
    mediator->devtool_wp_ = devtool_;
    devtool_->message_sender_ = std::make_shared<MessageSenderMock>();
  }

  void WaitForMessage() {
    for (int i = 0; i < 100; ++i) {
      if (!MockReceiver::GetInstance().received_message_.first.empty()) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

 protected:
  std::shared_ptr<InspectorAccessibilityAgent> agent_;
  std::shared_ptr<lynx::testing::LynxDevToolNGMock> devtool_;
};

TEST_F(InspectorAccessibilityAgentTest, EnableAndDisableReturnSuccess) {
  const std::string methods[] = {
      "Accessibility.disable",
      "Accessibility.enable",
  };

  for (int i = 0; i < 2; ++i) {
    MockReceiver::GetInstance().ResetAll();

    Json::Value message(Json::ValueType::objectValue);
    message["id"] = i + 1;
    message["method"] = methods[i];

    agent_->CallMethod(devtool_->message_sender_, message);
    WaitForMessage();

    EXPECT_EQ(MockReceiver::GetInstance().received_message_.first, "CDP");
    Json::Value response;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(
        MockReceiver::GetInstance().received_message_.second, response));
    EXPECT_EQ(response["id"].asInt(), i + 1);
    EXPECT_TRUE(response["error"].isNull());
    EXPECT_TRUE(response["result"].isObject());
  }
}

TEST_F(InspectorAccessibilityAgentTest, DispatchesMethodsToDefaultFallback) {
  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 100;
  enable_message["method"] = "Accessibility.enable";
  agent_->CallMethod(devtool_->message_sender_, enable_message);
  WaitForMessage();

  const std::string methods[] = {
      "Accessibility.getAXNodeAndAncestors", "Accessibility.getChildAXNodes",
      "Accessibility.getFullAXTree",         "Accessibility.getPartialAXTree",
      "Accessibility.getRootAXNode",         "Accessibility.queryAXTree",
  };

  for (int i = 0; i < 6; ++i) {
    MockReceiver::GetInstance().ResetAll();

    Json::Value message(Json::ValueType::objectValue);
    message["id"] = i + 1;
    message["method"] = methods[i];

    agent_->CallMethod(devtool_->message_sender_, message);
    WaitForMessage();

    EXPECT_EQ(MockReceiver::GetInstance().received_message_.first, "CDP");
    Json::Value response;
    Json::Reader reader;
    ASSERT_TRUE(reader.parse(
        MockReceiver::GetInstance().received_message_.second, response));
    EXPECT_EQ(response["id"].asInt(), i + 1);
    EXPECT_EQ(response["error"]["code"].asInt(), kInspectorErrorCode);
    EXPECT_EQ(response["error"]["message"].asString(),
              "Not implemented: " + methods[i]);
  }
}

TEST_F(InspectorAccessibilityAgentTest, RejectsUnknownMethods) {
  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 9;
  message["method"] = "Accessibility.unknown";

  agent_->CallMethod(devtool_->message_sender_, message);

  EXPECT_EQ(MockReceiver::GetInstance().received_message_.first, "CDP");
  Json::Value response;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(MockReceiver::GetInstance().received_message_.second,
                           response));
  EXPECT_EQ(response["id"].asInt(), 9);
  EXPECT_EQ(response["error"]["code"].asInt(), kInspectorErrorCode);
  EXPECT_EQ(response["error"]["message"].asString(),
            "Not implemented: Accessibility.unknown");
}

}  // namespace testing
}  // namespace devtool
}  // namespace lynx
