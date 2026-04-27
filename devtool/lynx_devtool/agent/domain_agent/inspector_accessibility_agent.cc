// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "devtool/lynx_devtool/agent/domain_agent/inspector_accessibility_agent.h"

namespace lynx {
namespace devtool {

InspectorAccessibilityAgent::InspectorAccessibilityAgent(
    const std::shared_ptr<LynxDevToolMediator>& devtool_mediator)
    : devtool_mediator_(devtool_mediator) {
  functions_map_["Accessibility.disable"] =
      &InspectorAccessibilityAgent::Disable;
  functions_map_["Accessibility.enable"] = &InspectorAccessibilityAgent::Enable;
  functions_map_["Accessibility.getAXNodeAndAncestors"] =
      &InspectorAccessibilityAgent::GetAXNodeAndAncestors;
  functions_map_["Accessibility.getChildAXNodes"] =
      &InspectorAccessibilityAgent::GetChildAXNodes;
  functions_map_["Accessibility.getFullAXTree"] =
      &InspectorAccessibilityAgent::GetFullAXTree;
  functions_map_["Accessibility.getPartialAXTree"] =
      &InspectorAccessibilityAgent::GetPartialAXTree;
  functions_map_["Accessibility.getRootAXNode"] =
      &InspectorAccessibilityAgent::GetRootAXNode;
  functions_map_["Accessibility.queryAXTree"] =
      &InspectorAccessibilityAgent::QueryAXTree;
}

void InspectorAccessibilityAgent::CallMethod(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  std::string method = message["method"].asString();
  auto iter = functions_map_.find(method);
  if (iter == functions_map_.end()) {
    sender->SendErrorResponse(message["id"].asInt64(),
                              "Not implemented: " + method);
    return;
  }
  (this->*(iter->second))(sender, message);
}

void InspectorAccessibilityAgent::Enable(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->AccessibilityEnable(sender, message);
}

void InspectorAccessibilityAgent::Disable(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->AccessibilityDisable(sender, message);
}

void InspectorAccessibilityAgent::GetAXNodeAndAncestors(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->GetAXNodeAndAncestors(sender, message);
}

void InspectorAccessibilityAgent::GetChildAXNodes(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->GetChildAXNodes(sender, message);
}

void InspectorAccessibilityAgent::GetFullAXTree(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->GetFullAXTree(sender, message);
}

void InspectorAccessibilityAgent::GetPartialAXTree(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->GetPartialAXTree(sender, message);
}

void InspectorAccessibilityAgent::GetRootAXNode(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->GetRootAXNode(sender, message);
}

void InspectorAccessibilityAgent::QueryAXTree(
    const std::shared_ptr<MessageSender>& sender, const Json::Value& message) {
  devtool_mediator_->QueryAXTree(sender, message);
}

}  // namespace devtool
}  // namespace lynx
