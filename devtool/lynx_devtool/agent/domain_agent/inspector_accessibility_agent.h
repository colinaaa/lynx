// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef DEVTOOL_LYNX_DEVTOOL_AGENT_DOMAIN_AGENT_INSPECTOR_ACCESSIBILITY_AGENT_H_
#define DEVTOOL_LYNX_DEVTOOL_AGENT_DOMAIN_AGENT_INSPECTOR_ACCESSIBILITY_AGENT_H_

#include <map>

#include "devtool/base_devtool/native/public/cdp_domain_agent_base.h"
#include "devtool/lynx_devtool/agent/lynx_devtool_mediator.h"

namespace lynx {
namespace devtool {

class InspectorAccessibilityAgent : public CDPDomainAgentBase {
 public:
  explicit InspectorAccessibilityAgent(
      const std::shared_ptr<LynxDevToolMediator>& devtool_mediator);
  ~InspectorAccessibilityAgent() override = default;

  void CallMethod(const std::shared_ptr<MessageSender>& sender,
                  const Json::Value& message) override;

 private:
  typedef void (InspectorAccessibilityAgent::*AccessibilityAgentMethod)(
      const std::shared_ptr<MessageSender>& sender, const Json::Value& message);

  void Enable(const std::shared_ptr<MessageSender>& sender,
              const Json::Value& message);
  void Disable(const std::shared_ptr<MessageSender>& sender,
               const Json::Value& message);
  void GetAXNodeAndAncestors(const std::shared_ptr<MessageSender>& sender,
                             const Json::Value& message);
  void GetChildAXNodes(const std::shared_ptr<MessageSender>& sender,
                       const Json::Value& message);
  void GetFullAXTree(const std::shared_ptr<MessageSender>& sender,
                     const Json::Value& message);
  void GetPartialAXTree(const std::shared_ptr<MessageSender>& sender,
                        const Json::Value& message);
  void GetRootAXNode(const std::shared_ptr<MessageSender>& sender,
                     const Json::Value& message);
  void QueryAXTree(const std::shared_ptr<MessageSender>& sender,
                   const Json::Value& message);

  std::map<std::string, AccessibilityAgentMethod> functions_map_;
  const std::shared_ptr<LynxDevToolMediator> devtool_mediator_;
};

}  // namespace devtool
}  // namespace lynx

#endif  // DEVTOOL_LYNX_DEVTOOL_AGENT_DOMAIN_AGENT_INSPECTOR_ACCESSIBILITY_AGENT_H_
