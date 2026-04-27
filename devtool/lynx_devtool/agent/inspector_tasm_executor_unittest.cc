// Copyright 2025 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#define private public
#define protected public

#include "devtool/lynx_devtool/agent/inspector_tasm_executor.h"

#include <sys/wait.h>

#include <cstddef>
#include <future>
#include <memory>

#include "core/renderer/dom/element.h"
#include "core/renderer/dom/element_manager.h"
#include "core/renderer/tasm/react/testing/mock_painting_context.h"
#include "core/shell/testing/mock_tasm_delegate.h"
#include "devtool/base_devtool/native/test/message_sender_mock.h"
#include "devtool/base_devtool/native/test/mock_receiver.h"
#include "devtool/lynx_devtool/agent/lynx_devtool_mediator.h"
#include "devtool/lynx_devtool/element/element_inspector.h"
#include "devtool/testing/mock/devtool_platform_facade_mock.h"
#include "devtool/testing/mock/lynx_devtool_ng_mock.h"
#include "third_party/googletest/googletest/include/gtest/gtest.h"
#include "third_party/jsoncpp/include/json/value.h"

namespace lynx {
namespace testing {

static constexpr int32_t kWidth = 1080;
static constexpr int32_t kHeight = 1920;
static constexpr float kDefaultLayoutsUnitPerPx = 1.f;
static constexpr double kDefaultPhysicalPixelsPerLayoutUnit = 1.f;

class InspectorTasmExecutorTest : public ::testing::Test {
 public:
  InspectorTasmExecutorTest() = default;
  ~InspectorTasmExecutorTest() override {}

  void SetUp() override {
    lynx::tasm::LynxEnvConfig lynx_env_config(
        kWidth, kHeight, kDefaultLayoutsUnitPerPx,
        kDefaultPhysicalPixelsPerLayoutUnit);
    tasm_mediator_ = std::make_shared<
        ::testing::NiceMock<lynx::tasm::test::MockTasmDelegate>>();
    manager_ = std::make_unique<lynx::tasm::ElementManager>(
        std::make_unique<lynx::tasm::MockPaintingContext>(),
        tasm_mediator_.get(), lynx_env_config);
    devtool::MockReceiver::GetInstance().ResetAll();
    devtool_mediator_ = std::make_shared<lynx::devtool::LynxDevToolMediator>();
    devtools_ng_ = std::make_shared<lynx::testing::LynxDevToolNGMock>();
    message_sender_ = std::make_shared<devtool::MessageSenderMock>();
    devtools_ng_->message_sender_ = message_sender_;
    devtool_mediator_->devtool_wp_ = devtools_ng_;
    element_executor_ = std::make_shared<devtool::InspectorTasmExecutor>(
        devtool_mediator_, nullptr, 1);
    ui_thread_ = std::make_unique<fml::Thread>("ui");
    devtool_mediator_->ui_task_runner_ = ui_thread_->GetTaskRunner();
  }

  void FlushDevtoolTasks() {
    std::promise<void> p;
    auto f = p.get_future();
    devtool_mediator_->RunOnDevToolThread([&p]() { p.set_value(); }, true);
    f.wait();
  }

 private:
  std::shared_ptr<devtool::InspectorTasmExecutor> element_executor_;
  std::shared_ptr<devtool::LynxDevToolMediator> devtool_mediator_;
  std::shared_ptr<devtool::MessageSender> message_sender_;
  std::shared_ptr<testing::LynxDevToolNGMock> devtools_ng_;
  std::shared_ptr<lynx::tasm::ElementManager> manager_;
  std::unique_ptr<fml::Thread> ui_thread_;
  std::shared_ptr<::testing::NiceMock<lynx::tasm::test::MockTasmDelegate>>
      tasm_mediator_;
};

TEST_F(InspectorTasmExecutorTest, SetDevtoolPlatformAbilityCase) {
  LOGI("InspectorTasmExecutorTest SetDevtoolPlatformAbilityCase start");

  std::shared_ptr<testing::DevToolPlatformFacadeMock> facade =
      std::make_shared<testing::DevToolPlatformFacadeMock>();
  element_executor_->SetDevToolPlatformFacade(facade);
  EXPECT_EQ(element_executor_->devtool_platform_facade_.get(), facade.get());
}

TEST_F(InspectorTasmExecutorTest, LayerTreeEnableCase) {
  LOGI("InspectorTasmExecutorTest LayerTreeEnableCase start");
  Json::Value message;
  message["id"] = 2;
  element_executor_->LayerTreeEnable(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  bool is_valid_json = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res);

  EXPECT_TRUE(is_valid_json);
  EXPECT_TRUE(element_executor_->layer_tree_enabled_);
}

TEST_F(InspectorTasmExecutorTest, LayerTreeDisableCase) {
  LOGI("InspectorTasmExecutorTest LayerTreeDisableCase start");
  Json::Value message;
  message["id"] = 6;
  element_executor_->LayerTreeDisable(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  bool is_valid_json = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res);

  EXPECT_TRUE(is_valid_json);
  EXPECT_EQ(res["id"], 6);
  EXPECT_FALSE(element_executor_->layer_tree_enabled_);
}

TEST_F(InspectorTasmExecutorTest, SendLayerTreeDidChangeEventCase) {
  element_executor_->layer_tree_enabled_ = true;

  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  auto element_container = element->element_container_impl();

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  element->AddChildAt(child, 0);
  EXPECT_EQ(child->parent(), element.get());

  child->CreateElementContainer(false);
  auto child_container = child->element_container_impl();
  child_container->InsertSelf();
  EXPECT_EQ(child_container->parent(), element_container);
  EXPECT_EQ(element_container->children().size(), static_cast<size_t>(1));
  element_executor_->element_root_ = element.get();

  element_executor_->SendLayerTreeDidChangeEvent();

  std::string expected_str = R"({
  "method": "LayerTree.layerTreeDidChange",
  "params": {
    "layers": [
      {
        "backendNodeId": 10,
        "drawsContent": true,
        "height": null,
        "invisible": true,
        "layerId": "10",
        "name": "view",
        "offsetX": null,
        "offsetY": null,
        "paintCount": 1,
        "width": null
      },
      {
        "backendNodeId": 11,
        "drawsContent": true,
        "height": null,
        "invisible": true,
        "layerId": "11",
        "name": "view",
        "offsetX": null,
        "offsetY": null,
        "paintCount": 1,
        "parentLayerId": "10",
        "width": null
      }
    ]
  }
})";

  Json::Reader reader;
  Json::Value expected_json;
  bool success = reader.parse(expected_str, expected_json);
  EXPECT_TRUE(success);
  Json::Value res;
  reader.parse(devtool::MockReceiver::GetInstance().received_message_.second,
               res);
  EXPECT_EQ(expected_json, res);
}

TEST_F(InspectorTasmExecutorTest, BuildLayerTreeFromElement) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  auto element_container = element->element_container_impl();

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  element->AddChildAt(child, 0);
  EXPECT_EQ(child->parent(), element.get());

  child->CreateElementContainer(false);
  auto child_container = child->element_container_impl();
  child_container->InsertSelf();
  EXPECT_EQ(child_container->parent(), element_container);
  EXPECT_EQ(element_container->children().size(), static_cast<size_t>(1));

  auto res = element_executor_->BuildLayerTreeFromElement(element.get());
  std::string layer_str = R"([
  {
    "backendNodeId": 10,
    "drawsContent": true,
    "height": null,
    "invisible": true,
    "layerId": "10",
    "name": "view",
    "offsetX": null,
    "offsetY": null,
    "paintCount": 1,
    "width": null
  },
  {
    "backendNodeId": 11,
    "drawsContent": true,
    "height": null,
    "invisible": true,
    "layerId": "11",
    "name": "view",
    "offsetX": null,
    "offsetY": null,
    "paintCount": 1,
    "parentLayerId": "10",
    "width": null
  }
])";
  Json::Reader reader;
  Json::Value layer;
  reader.parse(layer_str, layer);
  EXPECT_EQ(res, layer);
}

TEST_F(InspectorTasmExecutorTest, GetLayerContentFromElementCase) {
  LOGI("InspectorTasmExecutorTest GetLayerContentFromElementCase start");
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(std::make_tuple(element));
  auto res = element_executor_->GetLayerContentFromElement(element.get());
  Json::Value layer(Json::ValueType::objectValue);

  layer["layerId"] =
      std::to_string(devtool::ElementInspector::NodeId(element.get()));
  layer["backendNodeId"] = devtool::ElementInspector::NodeId(element.get());
  layer["paintCount"] = 1;
  layer["drawsContent"] = true;
  layer["invisible"] = true;
  layer["name"] = "view";
  Json::Value layout(Json::ValueType::objectValue);
  layer["offsetX"] = layout["offsetX"];
  layer["offsetY"] = layout["offsetY"];
  layer["width"] = layout["width"];
  layer["height"] = layout["height"];

  EXPECT_EQ(layer, res);
}

TEST_F(InspectorTasmExecutorTest, GetDocumentWithDepthCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  auto grandchild = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(grandchild.get()));

  root->AddChildAt(child, 0);
  child->AddChildAt(grandchild, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 7;
  message["params"]["depth"] = 1;
  element_executor_->GetDocument(message_sender_, message);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 7);
  EXPECT_TRUE(res["error"].isNull());
  EXPECT_EQ(res["result"]["root"]["nodeId"],
            devtool::ElementInspector::NodeId(root.get()));
  ASSERT_TRUE(res["result"]["root"]["children"].isArray());
  ASSERT_EQ(res["result"]["root"]["children"].size(), 1U);
  EXPECT_EQ(res["result"]["root"]["children"][0]["nodeId"],
            devtool::ElementInspector::NodeId(child.get()));
  EXPECT_TRUE(res["result"]["root"]["children"][0]["children"].isNull());
}

TEST_F(InspectorTasmExecutorTest, GetDocumentDefaultDepthReturnsFullTreeCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  auto grandchild = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(grandchild.get()));

  root->AddChildAt(child, 0);
  child->AddChildAt(grandchild, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 8;
  element_executor_->GetDocument(message_sender_, message);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 8);
  ASSERT_TRUE(res["result"]["root"]["children"].isArray());
  ASSERT_EQ(res["result"]["root"]["children"].size(), 1U);
  ASSERT_TRUE(res["result"]["root"]["children"][0]["children"].isArray());
  ASSERT_EQ(res["result"]["root"]["children"][0]["children"].size(), 1U);
  EXPECT_EQ(res["result"]["root"]["children"][0]["children"][0]["nodeId"],
            devtool::ElementInspector::NodeId(grandchild.get()));
}

TEST_F(InspectorTasmExecutorTest, GetRootAXNodeReturnsRootNodeCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));
  lynx::devtool::ElementInspector::UpdateAttr(root.get(), "accessibility-label",
                                              "Root label");
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 9;
  element_executor_->GetRootAXNode(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 9);
  EXPECT_TRUE(res["error"].isNull());
  EXPECT_EQ(res["result"]["node"]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(res["result"]["node"]["role"]["value"], "RootWebArea");
  EXPECT_EQ(res["result"]["node"]["name"]["value"], "Root label");
  EXPECT_EQ(res["result"]["node"]["backendDOMNodeId"],
            devtool::ElementInspector::NodeId(root.get()));
}

TEST_F(InspectorTasmExecutorTest, GetFullAXTreeReturnsDepthLimitedTreeCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));
  lynx::devtool::ElementInspector::UpdateAttr(root.get(), "accessibility-label",
                                              "Root label");

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  lynx::devtool::ElementInspector::UpdateAttr(child.get(),
                                              "accessibility-label", "Submit");
  lynx::devtool::ElementInspector::UpdateAttr(child.get(),
                                              "accessibility-traits", "button");

  auto grandchild = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(grandchild.get()));
  lynx::devtool::ElementInspector::UpdateAttr(grandchild.get(), "text",
                                              "Ignored by depth");

  root->AddChildAt(child, 0);
  child->AddChildAt(grandchild, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 10;
  message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 10);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);

  const Json::Value& root_node = res["result"]["nodes"][0];
  const Json::Value& child_node = res["result"]["nodes"][1];
  EXPECT_EQ(root_node["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  ASSERT_TRUE(root_node["childIds"].isArray());
  ASSERT_EQ(root_node["childIds"].size(), 1U);
  EXPECT_EQ(root_node["childIds"][0],
            std::to_string(devtool::ElementInspector::NodeId(child.get())));
  EXPECT_EQ(child_node["parentId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(child_node["role"]["value"], "button");
  EXPECT_EQ(child_node["name"]["value"], "Submit");
}

TEST_F(InspectorTasmExecutorTest, GetFullAXTreeReturnsIgnoredReasonsCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto hidden = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(hidden.get()));
  lynx::devtool::ElementInspector::UpdateAttr(hidden.get(),
                                              "accessibility-element", "false");
  lynx::devtool::ElementInspector::UpdateAttr(
      hidden.get(), "accessibility-elements-hidden", "true");

  root->AddChildAt(hidden, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 17;
  message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 17);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);
  EXPECT_FALSE(res["result"]["nodes"][0]["ignored"].asBool());
  EXPECT_TRUE(res["result"]["nodes"][0]["ignoredReasons"].isNull());

  const Json::Value& hidden_node = res["result"]["nodes"][1];
  EXPECT_TRUE(hidden_node["ignored"].asBool());
  ASSERT_TRUE(hidden_node["ignoredReasons"].isArray());
  ASSERT_EQ(hidden_node["ignoredReasons"].size(), 2U);
  EXPECT_EQ(hidden_node["ignoredReasons"][0]["name"], "uninteresting");
  EXPECT_EQ(hidden_node["ignoredReasons"][0]["value"]["type"], "boolean");
  EXPECT_TRUE(hidden_node["ignoredReasons"][0]["value"]["value"].asBool());
  EXPECT_EQ(hidden_node["ignoredReasons"][1]["name"], "ariaHiddenSubtree");
  EXPECT_EQ(hidden_node["ignoredReasons"][1]["value"]["type"], "boolean");
  EXPECT_TRUE(hidden_node["ignoredReasons"][1]["value"]["value"].asBool());
}

TEST_F(InspectorTasmExecutorTest, GetFullAXTreeReturnsAccessibilityValueCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto slider = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(slider.get()));
  lynx::devtool::ElementInspector::UpdateAttr(slider.get(),
                                              "accessibility-label", "Volume");
  lynx::devtool::ElementInspector::UpdateAttr(slider.get(),
                                              "accessibility-value", "50%");

  root->AddChildAt(slider, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 18;
  message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 18);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);
  EXPECT_TRUE(res["result"]["nodes"][0]["value"].isNull());

  const Json::Value& slider_node = res["result"]["nodes"][1];
  EXPECT_EQ(slider_node["name"]["value"], "Volume");
  EXPECT_EQ(slider_node["value"]["type"], "string");
  EXPECT_EQ(slider_node["value"]["value"], "50%");
}

TEST_F(InspectorTasmExecutorTest,
       GetFullAXTreeReturnsRoleDescriptionPropertyCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto tab = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(tab.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      tab.get(), "accessibility-label", "Inbox");
  lynx::devtool::ElementInspector::UpdateAttr(
      tab.get(), "accessibility-role-description", "tab");

  root->AddChildAt(tab, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 33;
  message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 33);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);
  EXPECT_TRUE(res["result"]["nodes"][0]["properties"].isNull());

  const Json::Value& tab_node = res["result"]["nodes"][1];
  ASSERT_TRUE(tab_node["properties"].isArray());
  ASSERT_EQ(tab_node["properties"].size(), 1U);
  EXPECT_EQ(tab_node["properties"][0]["name"], "roledescription");
  EXPECT_EQ(tab_node["properties"][0]["value"]["type"], "string");
  EXPECT_EQ(tab_node["properties"][0]["value"]["value"], "tab");
}

TEST_F(InspectorTasmExecutorTest, GetFullAXTreeReturnsHeadingRoleCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto heading = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(heading.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      heading.get(), "accessibility-label", "Section");
  lynx::devtool::ElementInspector::UpdateAttr(
      heading.get(), "accessibility-heading", "true");

  root->AddChildAt(heading, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 36;
  message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 36);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);
  EXPECT_EQ(res["result"]["nodes"][1]["role"]["value"], "heading");
  EXPECT_EQ(res["result"]["nodes"][1]["name"]["value"], "Section");
}

TEST_F(InspectorTasmExecutorTest, GetChildAXNodesReturnsDirectChildrenCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));

  auto grandchild = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(grandchild.get()));
  lynx::devtool::ElementInspector::UpdateAttr(grandchild.get(), "text",
                                              "Child label");

  root->AddChildAt(child, 0);
  child->AddChildAt(grandchild, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 11;
  message["params"]["id"] =
      std::to_string(devtool::ElementInspector::NodeId(child.get()));
  element_executor_->GetChildAXNodes(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 11);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 1U);
  EXPECT_EQ(
      res["result"]["nodes"][0]["nodeId"],
      std::to_string(devtool::ElementInspector::NodeId(grandchild.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["parentId"],
            std::to_string(devtool::ElementInspector::NodeId(child.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["name"]["value"], "Child label");
}

TEST_F(InspectorTasmExecutorTest,
       GetPartialAXTreeReturnsTargetOnlyWhenRelativesDisabledCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto target = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(target.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      target.get(), "accessibility-label", "Target label");

  root->AddChildAt(target, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 12;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(target.get());
  message["params"]["fetchRelatives"] = false;
  element_executor_->GetPartialAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 12);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 1U);
  EXPECT_EQ(res["result"]["nodes"][0]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(target.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["parentId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["name"]["value"], "Target label");
}

TEST_F(InspectorTasmExecutorTest,
       GetPartialAXTreeReturnsRelativesByDefaultCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto target = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(target.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      target.get(), "accessibility-label", "Target label");

  auto sibling = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(sibling.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      sibling.get(), "accessibility-label", "Sibling label");

  auto child = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  lynx::devtool::ElementInspector::UpdateAttr(child.get(), "text",
                                              "Child label");

  root->AddChildAt(target, 0);
  root->AddChildAt(sibling, 1);
  target->AddChildAt(child, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 13;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(target.get());
  element_executor_->GetPartialAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 13);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 4U);
  EXPECT_EQ(res["result"]["nodes"][0]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(target.get())));
  EXPECT_EQ(res["result"]["nodes"][1]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(res["result"]["nodes"][2]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(sibling.get())));
  EXPECT_EQ(res["result"]["nodes"][3]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(child.get())));
}

TEST_F(InspectorTasmExecutorTest, GetAXNodeAndAncestorsReturnsParentChainCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto parent = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(parent.get()));
  lynx::devtool::ElementInspector::UpdateAttr(
      parent.get(), "accessibility-label", "Parent label");

  auto target = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(target.get()));
  lynx::devtool::ElementInspector::UpdateAttr(target.get(), "text",
                                              "Target label");

  root->AddChildAt(parent, 0);
  parent->AddChildAt(target, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 14;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(target.get());
  element_executor_->GetAXNodeAndAncestors(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 14);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 3U);
  EXPECT_EQ(res["result"]["nodes"][0]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(target.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["parentId"],
            std::to_string(devtool::ElementInspector::NodeId(parent.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["name"]["value"], "Target label");
  EXPECT_EQ(res["result"]["nodes"][1]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(parent.get())));
  EXPECT_EQ(res["result"]["nodes"][1]["parentId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(res["result"]["nodes"][2]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
}

TEST_F(InspectorTasmExecutorTest, QueryAXTreeFiltersByNameAndRoleCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto button = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(button.get()));
  lynx::devtool::ElementInspector::UpdateAttr(button.get(),
                                              "accessibility-label", "Submit");
  lynx::devtool::ElementInspector::UpdateAttr(button.get(),
                                              "accessibility-traits", "button");

  auto text = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(text.get()));
  lynx::devtool::ElementInspector::UpdateAttr(text.get(), "text", "Submit");

  root->AddChildAt(button, 0);
  root->AddChildAt(text, 1);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 15;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(root.get());
  message["params"]["accessibleName"] = "Submit";
  message["params"]["role"] = "button";
  element_executor_->QueryAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 15);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 1U);
  EXPECT_EQ(res["result"]["nodes"][0]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(button.get())));
  EXPECT_EQ(res["result"]["nodes"][0]["name"]["value"], "Submit");
  EXPECT_EQ(res["result"]["nodes"][0]["role"]["value"], "button");
}

TEST_F(InspectorTasmExecutorTest, QueryAXTreeReturnsSubtreeWithoutFiltersCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto parent = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(parent.get()));
  auto child = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  lynx::devtool::ElementInspector::UpdateAttr(child.get(), "text",
                                              "Child label");

  root->AddChildAt(parent, 0);
  parent->AddChildAt(child, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 16;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(parent.get());
  element_executor_->QueryAXTree(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 16);
  ASSERT_TRUE(res["result"]["nodes"].isArray());
  ASSERT_EQ(res["result"]["nodes"].size(), 2U);
  EXPECT_EQ(res["result"]["nodes"][0]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(parent.get())));
  EXPECT_EQ(res["result"]["nodes"][1]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(child.get())));
  EXPECT_EQ(res["result"]["nodes"][1]["name"]["value"], "Child label");
}

TEST_F(InspectorTasmExecutorTest, DescribeNodeByNodeIdCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));
  root->SetAttribute("id", lepus::Value("root"));

  auto child = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  child->SetAttribute("text", lepus::Value("hello"));
  root->AddChildAt(child, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 1;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(root.get());
  element_executor_->DescribeNode(message_sender_, message);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 1);
  EXPECT_TRUE(res["error"].isNull());
  EXPECT_EQ(res["result"]["node"]["nodeId"],
            devtool::ElementInspector::NodeId(root.get()));
  EXPECT_EQ(res["result"]["node"]["backendNodeId"],
            devtool::ElementInspector::NodeId(root.get()));
  EXPECT_EQ(res["result"]["node"]["childNodeCount"], 1);
  ASSERT_TRUE(res["result"]["node"]["children"].isArray());
  ASSERT_EQ(res["result"]["node"]["children"].size(), 1U);
  EXPECT_EQ(res["result"]["node"]["children"][0]["nodeId"],
            devtool::ElementInspector::NodeId(child.get()));
}

TEST_F(InspectorTasmExecutorTest, DescribeNodeDepthZeroCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  root->AddChildAt(child, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 2;
  message["params"]["nodeId"] = devtool::ElementInspector::NodeId(root.get());
  message["params"]["depth"] = 0;
  element_executor_->DescribeNode(message_sender_, message);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["result"]["node"]["childNodeCount"], 1);
  EXPECT_TRUE(res["result"]["node"]["children"].isNull());
}

TEST_F(InspectorTasmExecutorTest, DescribeNodeWithFullDepthCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto child = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(child.get()));
  auto grandchild = manager_->CreateFiberElement("text");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(grandchild.get()));

  root->AddChildAt(child, 0);
  child->AddChildAt(grandchild, 0);
  element_executor_->element_root_ = root.get();

  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 3;
  message["params"]["backendNodeId"] =
      devtool::ElementInspector::NodeId(child.get());
  message["params"]["depth"] = -1;
  element_executor_->DescribeNode(message_sender_, message);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["result"]["node"]["nodeId"],
            devtool::ElementInspector::NodeId(child.get()));
  ASSERT_TRUE(res["result"]["node"]["children"].isArray());
  ASSERT_EQ(res["result"]["node"]["children"].size(), 1U);
  EXPECT_EQ(res["result"]["node"]["children"][0]["nodeId"],
            devtool::ElementInspector::NodeId(grandchild.get()));
}

TEST_F(InspectorTasmExecutorTest, DescribeNodeMissingNodeCase) {
  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 4;
  message["params"]["nodeId"] = 99999;
  element_executor_->DescribeNode(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 4);
  EXPECT_TRUE(res["error"].isNull());
  EXPECT_TRUE(res["result"]["node"].isNull());
}

TEST_F(InspectorTasmExecutorTest, DescribeNodeUnsupportedObjectIdCase) {
  Json::Value message(Json::ValueType::objectValue);
  message["id"] = 5;
  message["params"]["objectId"] = "remote-object-id";
  element_executor_->DescribeNode(message_sender_, message);

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["id"], 5);
  EXPECT_TRUE(res["error"].isNull());
  EXPECT_TRUE(res["result"]["node"].isNull());
}
TEST_F(InspectorTasmExecutorTest, SendDOMEventMsgCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  int node_id = devtool::ElementInspector::NodeId(element.get());
  element_executor_->element_root_ = element.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();

  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "style", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  bool is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res);
  EXPECT_TRUE(is_valid);
  EXPECT_EQ(res["method"], "DOM.attributeModified");
  EXPECT_EQ(res["params"]["nodeId"], node_id);
  EXPECT_EQ(res["params"]["name"], "style");
  EXPECT_TRUE(res["params"]["value"].isString());

  std::string prev =
      devtool::MockReceiver::GetInstance().received_message_.second;
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, -1,
      "style", -1);
  FlushDevtoolTasks();
  EXPECT_EQ(devtool::MockReceiver::GetInstance().received_message_.second,
            prev);

  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::CHILD_NODE_REMOVED, node_id,
      "", node_id);
  FlushDevtoolTasks();
  is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res);
  EXPECT_TRUE(is_valid);
  EXPECT_EQ(res["method"], "DOM.childNodeRemoved");
  EXPECT_EQ(res["params"]["nodeId"], node_id);
  EXPECT_EQ(res["params"]["parentNodeId"], node_id);

  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::DOCUMENT_UPDATED, -1, "",
      -1);
  FlushDevtoolTasks();
  is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res);
  EXPECT_TRUE(is_valid);
  EXPECT_EQ(res["method"], "DOM.documentUpdated");
  EXPECT_TRUE(res["params"].isObject());
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityLoadCompleteFollowsEnableStateCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));
  lynx::devtool::ElementInspector::UpdateAttr(root.get(), "accessibility-label",
                                              "Root label");
  element_executor_->element_root_ = root.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Reader reader;
  Json::Value res;
  element_executor_->OnDocumentUpdated();
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.documentUpdated");

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 19;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  element_executor_->OnDocumentUpdated();
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.loadComplete");
  EXPECT_EQ(res["params"]["root"]["nodeId"],
            std::to_string(devtool::ElementInspector::NodeId(root.get())));
  EXPECT_EQ(res["params"]["root"]["name"]["value"], "Root label");

  Json::Value disable_message(Json::ValueType::objectValue);
  disable_message["id"] = 20;
  devtool_mediator_->AccessibilityDisable(message_sender_, disable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  element_executor_->OnDocumentUpdated();
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.documentUpdated");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityNodesUpdatedFollowsEnableStateCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  element_executor_->element_root_ = element.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  int node_id = devtool::ElementInspector::NodeId(element.get());
  Json::Reader reader;
  Json::Value res;
  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Disabled label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 21;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  Json::Value root_message(Json::ValueType::objectValue);
  root_message["id"] = 23;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Enabled label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.nodesUpdated");
  ASSERT_TRUE(res["params"]["nodes"].isArray());
  ASSERT_EQ(res["params"]["nodes"].size(), 1U);
  EXPECT_EQ(res["params"]["nodes"][0]["nodeId"], std::to_string(node_id));
  EXPECT_EQ(res["params"]["nodes"][0]["name"]["value"], "Enabled label");

  Json::Value disable_message(Json::ValueType::objectValue);
  disable_message["id"] = 22;
  devtool_mediator_->AccessibilityDisable(message_sender_, disable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Disabled again");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityNodesUpdatedRequiresRequestedNodeCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  element_executor_->element_root_ = element.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 24;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  int node_id = devtool::ElementInspector::NodeId(element.get());
  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Unrequested label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");

  Json::Value root_message(Json::ValueType::objectValue);
  root_message["id"] = 25;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Requested label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();

  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.nodesUpdated");
  ASSERT_TRUE(res["params"]["nodes"].isArray());
  ASSERT_EQ(res["params"]["nodes"].size(), 1U);
  EXPECT_EQ(res["params"]["nodes"][0]["nodeId"], std::to_string(node_id));
  EXPECT_EQ(res["params"]["nodes"][0]["name"]["value"], "Requested label");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityNodesUpdatedFollowsAXAttributesCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  element_executor_->element_root_ = element.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 26;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();

  Json::Value root_message(Json::ValueType::objectValue);
  root_message["id"] = 27;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  int node_id = devtool::ElementInspector::NodeId(element.get());
  lynx::devtool::ElementInspector::UpdateAttr(element.get(), "data-test",
                                              "debug-only");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "data-test", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");

  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "Removed label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  lynx::devtool::ElementInspector::DeleteAttr(element.get(),
                                              "accessibility-label");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_REMOVED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();

  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.nodesUpdated");
  ASSERT_TRUE(res["params"]["nodes"].isArray());
  ASSERT_EQ(res["params"]["nodes"].size(), 1U);
  EXPECT_EQ(res["params"]["nodes"][0]["nodeId"], std::to_string(node_id));
  EXPECT_EQ(res["params"]["nodes"][0]["name"]["value"], "");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityRequestedNodesResetWithSessionStateCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  element_executor_->element_root_ = element.get();
  devtool_mediator_->element_executor_ = element_executor_;
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 28;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();

  Json::Value root_message(Json::ValueType::objectValue);
  root_message["id"] = 29;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  Json::Value disable_message(Json::ValueType::objectValue);
  disable_message["id"] = 30;
  devtool_mediator_->AccessibilityDisable(message_sender_, disable_message);
  FlushDevtoolTasks();

  enable_message["id"] = 31;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  int node_id = devtool::ElementInspector::NodeId(element.get());
  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "After reenable");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");

  root_message["id"] = 32;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  element_executor_->OnDocumentUpdated();
  FlushDevtoolTasks();
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-label", "After document update");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-label", -1);
  FlushDevtoolTasks();

  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "DOM.attributeModified");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityNodesUpdatedFollowsRoleDescriptionCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  element_executor_->element_root_ = element.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 34;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();

  Json::Value root_message(Json::ValueType::objectValue);
  root_message["id"] = 35;
  element_executor_->GetRootAXNode(message_sender_, root_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  int node_id = devtool::ElementInspector::NodeId(element.get());
  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-role-description", "tab");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-role-description", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.nodesUpdated");
  ASSERT_TRUE(res["params"]["nodes"].isArray());
  ASSERT_EQ(res["params"]["nodes"].size(), 1U);
  ASSERT_TRUE(res["params"]["nodes"][0]["properties"].isArray());
  ASSERT_EQ(res["params"]["nodes"][0]["properties"].size(), 1U);
  EXPECT_EQ(res["params"]["nodes"][0]["properties"][0]["name"],
            "roledescription");
  EXPECT_EQ(res["params"]["nodes"][0]["properties"][0]["value"]["value"],
            "tab");
}

TEST_F(InspectorTasmExecutorTest,
       AccessibilityNodesUpdatedFollowsHeadingCase) {
  auto root = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(root.get()));

  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element->CreateElementContainer(false);
  root->AddChildAt(element, 0);
  element_executor_->element_root_ = root.get();
  devtool_mediator_->default_task_runner_ = ui_thread_->GetTaskRunner();
  devtool_mediator_->devtool_executor_ =
      std::make_shared<devtool::InspectorDefaultExecutor>(devtool_mediator_);

  Json::Value enable_message(Json::ValueType::objectValue);
  enable_message["id"] = 37;
  devtool_mediator_->AccessibilityEnable(message_sender_, enable_message);
  FlushDevtoolTasks();

  Json::Value tree_message(Json::ValueType::objectValue);
  tree_message["id"] = 38;
  tree_message["params"]["depth"] = 1;
  element_executor_->GetFullAXTree(message_sender_, tree_message);
  devtool::MockReceiver::GetInstance().received_message_ = {"", ""};

  int node_id = devtool::ElementInspector::NodeId(element.get());
  lynx::devtool::ElementInspector::UpdateAttr(
      element.get(), "accessibility-heading", "true");
  element_executor_->SendDOMEventMsg(
      devtool::InspectorTasmExecutor::DomCdpEvent::ATTRIBUTE_MODIFIED, node_id,
      "accessibility-heading", -1);
  FlushDevtoolTasks();

  Json::Value res;
  Json::Reader reader;
  ASSERT_TRUE(reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second, res));
  EXPECT_EQ(res["method"], "Accessibility.nodesUpdated");
  ASSERT_TRUE(res["params"]["nodes"].isArray());
  ASSERT_EQ(res["params"]["nodes"].size(), 1U);
  EXPECT_EQ(res["params"]["nodes"][0]["role"]["value"], "heading");
}

TEST_F(InspectorTasmExecutorTest, SearchProtocolUsesStringSearchIdCase) {
  auto element = manager_->CreateFiberElement("view");
  lynx::devtool::ElementInspector::InitForInspector(
      std::make_tuple(element.get()));
  element_executor_->element_root_ = element.get();

  Json::Value perform_search_message(Json::ValueType::objectValue);
  perform_search_message["id"] = 1;
  perform_search_message["params"]["query"] = "view";
  element_executor_->PerformSearch(message_sender_, perform_search_message);

  Json::Reader reader;
  Json::Value perform_search_response;
  bool is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second,
      perform_search_response);
  EXPECT_TRUE(is_valid);
  EXPECT_TRUE(perform_search_response["result"]["searchId"].isString());
  std::string search_id =
      perform_search_response["result"]["searchId"].asString();
  EXPECT_FALSE(search_id.empty());

  Json::Value get_search_results_message(Json::ValueType::objectValue);
  get_search_results_message["id"] = 2;
  get_search_results_message["params"]["searchId"] = search_id;
  get_search_results_message["params"]["fromIndex"] = 0;
  get_search_results_message["params"]["toIndex"] = 1;
  element_executor_->GetSearchResults(message_sender_,
                                      get_search_results_message);

  Json::Value get_search_results_response;
  is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second,
      get_search_results_response);
  EXPECT_TRUE(is_valid);
  EXPECT_TRUE(get_search_results_response.isMember("result"));
  EXPECT_TRUE(get_search_results_response["result"]["nodeIds"].isArray());

  Json::Value discard_search_results_message(Json::ValueType::objectValue);
  discard_search_results_message["id"] = 3;
  discard_search_results_message["params"]["searchId"] = search_id;
  element_executor_->DiscardSearchResults(message_sender_,
                                          discard_search_results_message);

  Json::Value discard_search_results_response;
  is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second,
      discard_search_results_response);
  EXPECT_TRUE(is_valid);
  EXPECT_TRUE(discard_search_results_response.isMember("result"));

  get_search_results_message["id"] = 4;
  element_executor_->GetSearchResults(message_sender_,
                                      get_search_results_message);
  Json::Value get_after_discard_response;
  is_valid = reader.parse(
      devtool::MockReceiver::GetInstance().received_message_.second,
      get_after_discard_response);
  EXPECT_TRUE(is_valid);
  EXPECT_TRUE(get_after_discard_response.isMember("error"));
  EXPECT_EQ(get_after_discard_response["error"]["code"], 32000);
}

}  // namespace testing
}  // namespace lynx
