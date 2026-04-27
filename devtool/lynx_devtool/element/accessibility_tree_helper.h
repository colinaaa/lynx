// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#ifndef DEVTOOL_LYNX_DEVTOOL_ELEMENT_ACCESSIBILITY_TREE_HELPER_H_
#define DEVTOOL_LYNX_DEVTOOL_ELEMENT_ACCESSIBILITY_TREE_HELPER_H_

#include "core/renderer/dom/element.h"
#include "third_party/jsoncpp/include/json/json.h"

using lynx::tasm::Element;

namespace lynx {
namespace devtool {

class AccessibilityTreeHelper {
 public:
  static Json::Value BuildAXNode(Element* element);
  static Json::Value BuildAXTree(Element* root, int depth = -1);
};

}  // namespace devtool
}  // namespace lynx

#endif  // DEVTOOL_LYNX_DEVTOOL_ELEMENT_ACCESSIBILITY_TREE_HELPER_H_
