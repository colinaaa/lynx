// Copyright 2021 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef DEVTOOL_LYNX_DEVTOOL_AGENT_INSPECTOR_DEFAULT_EXECUTOR_H_
#define DEVTOOL_LYNX_DEVTOOL_AGENT_INSPECTOR_DEFAULT_EXECUTOR_H_

#include <memory>

#include "base/include/fml/task_runner.h"
#include "base/include/fml/thread.h"
#include "devtool/base_devtool/native/public/message_sender.h"
#include "devtool/lynx_devtool/agent/agent_defines.h"
#include "devtool/lynx_devtool/agent/console_message_manager.h"
#include "devtool/lynx_devtool/agent/devtool_platform_facade.h"

namespace lynx {
namespace runtime {
namespace js {
struct ConsoleMessage;
}
}  // namespace runtime
namespace devtool {

class LynxDevToolMediator;

class InspectorDefaultExecutor
    : public std::enable_shared_from_this<InspectorDefaultExecutor> {
 public:
  explicit InspectorDefaultExecutor(
      const std::shared_ptr<LynxDevToolMediator>& devtool_mediator);

  // clear some status when lynx view reload
  // todo(yanghuiwen): mark Reset() as override
  void Reset();
  void SetDevToolPlatformFacade(
      const std::shared_ptr<DevToolPlatformFacade>& devtool_platform_facade);
  // events for Log domain
  void SendLogEntryAddedEvent(const lynx::runtime::js::ConsoleMessage& message);

  // Log domain
  DECLARE_DEVTOOL_METHOD(LogEnable)
  DECLARE_DEVTOOL_METHOD(LogDisable)
  DECLARE_DEVTOOL_METHOD(LogClear)

  DECLARE_DEVTOOL_METHOD(InspectorEnable)
  DECLARE_DEVTOOL_METHOD(InspectorDetached)

  DECLARE_DEVTOOL_METHOD(AccessibilityEnable)
  DECLARE_DEVTOOL_METHOD(AccessibilityDisable)
  DECLARE_DEVTOOL_METHOD(GetAXNodeAndAncestors)
  DECLARE_DEVTOOL_METHOD(GetChildAXNodes)
  DECLARE_DEVTOOL_METHOD(GetFullAXTree)
  DECLARE_DEVTOOL_METHOD(GetPartialAXTree)
  DECLARE_DEVTOOL_METHOD(GetRootAXNode)
  DECLARE_DEVTOOL_METHOD(QueryAXTree)

  DECLARE_DEVTOOL_METHOD(LynxSetTraceMode)
  DECLARE_DEVTOOL_METHOD(LynxGetVersion)

 private:
  void SendNotImplementedResponse(
      const std::shared_ptr<lynx::devtool::MessageSender>& sender,
      const Json::Value& message);

  std::weak_ptr<LynxDevToolMediator> devtool_mediator_wp_;
  std::shared_ptr<DevToolPlatformFacade> devtool_platform_facade_;
  std::unique_ptr<ConsoleMessageManager> console_msg_manager_;
};

}  // namespace devtool
}  // namespace lynx

#endif  // DEVTOOL_LYNX_DEVTOOL_AGENT_INSPECTOR_DEFAULT_EXECUTOR_H_
