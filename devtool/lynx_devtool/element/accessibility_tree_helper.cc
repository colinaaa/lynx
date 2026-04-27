// Copyright 2026 The Lynx Authors. All rights reserved.
// Licensed under the Apache License Version 2.0 that can be found in the
// LICENSE file in the root directory of this source tree.

#include "devtool/lynx_devtool/element/accessibility_tree_helper.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#include "base/include/value/array.h"
#include "devtool/lynx_devtool/element/element_inspector.h"

namespace lynx {
namespace devtool {

namespace {

constexpr const char* kAccessibilityElement = "accessibility-element";
constexpr const char* kAccessibilityActions = "accessibility-actions";
constexpr const char* kAccessibilityElementsHidden =
    "accessibility-elements-hidden";
constexpr const char* kAccessibilityHeading = "accessibility-heading";
constexpr const char* kAccessibilityLabel = "accessibility-label";
constexpr const char* kAccessibilityRoleDescription =
    "accessibility-role-description";
constexpr const char* kAccessibilityTraits = "accessibility-traits";
constexpr const char* kAccessibilityValue = "accessibility-value";
constexpr const char* kText = "text";

Json::Value BuildAXValue(const std::string& type, const std::string& value) {
  Json::Value ax_value(Json::ValueType::objectValue);
  ax_value["type"] = type;
  ax_value["value"] = value;
  return ax_value;
}

Json::Value BuildAXBooleanProperty(const std::string& name, bool value) {
  Json::Value property(Json::ValueType::objectValue);
  property["name"] = name;
  property["value"]["type"] = "boolean";
  property["value"]["value"] = value;
  return property;
}

Json::Value BuildAXStringProperty(const std::string& name,
                                  const std::string& value) {
  Json::Value property(Json::ValueType::objectValue);
  property["name"] = name;
  property["value"] = BuildAXValue("string", value);
  return property;
}

Json::Value BuildAXTokenListProperty(const std::string& name,
                                     const std::vector<std::string>& values) {
  Json::Value property(Json::ValueType::objectValue);
  property["name"] = name;
  property["value"]["type"] = "tokenList";
  Json::Value token_list(Json::ValueType::arrayValue);
  for (const auto& value : values) {
    token_list.append(value);
  }
  property["value"]["value"] = token_list;
  return property;
}

std::string GetAXNodeId(Element* element) {
  return std::to_string(ElementInspector::NodeId(element));
}

std::string ToLower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

std::string GetAttribute(Element* element, const std::string& name) {
  if (!element || !element->inspector_attribute()) {
    return "";
  }

  auto& attr_map = ElementInspector::AttrMap(element);
  auto iter = attr_map.find(name);
  if (iter != attr_map.end()) {
    return iter->second;
  }

  auto holder_attrs = ElementInspector::GetAttrFromAttributeHolder(element);
  auto holder_iter = holder_attrs.second.find(name);
  if (holder_iter != holder_attrs.second.end()) {
    return holder_iter->second;
  }
  return "";
}

bool HasTrait(const std::string& traits, const std::string& trait) {
  std::string normalized = ToLower(traits);
  std::replace(normalized.begin(), normalized.end(), ',', ' ');
  size_t start = 0;
  while (start < normalized.size()) {
    size_t end = normalized.find(' ', start);
    if (end == std::string::npos) {
      end = normalized.size();
    }
    if (normalized.substr(start, end - start) == trait) {
      return true;
    }
    start = end + 1;
  }
  return false;
}

bool IsTrueAttribute(const std::string& value) {
  std::string normalized = ToLower(value);
  return normalized == "true" || normalized == "1";
}

bool IsFalseAttribute(const std::string& value) {
  std::string normalized = ToLower(value);
  return normalized == "false" || normalized == "0";
}

std::string Trim(const std::string& value) {
  size_t start = 0;
  while (start < value.size() &&
         std::isspace(static_cast<unsigned char>(value[start]))) {
    ++start;
  }
  size_t end = value.size();
  while (end > start &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return value.substr(start, end - start);
}

void AppendCommaSeparatedTokens(const std::string& value,
                                std::vector<std::string>& tokens) {
  size_t start = 0;
  while (start <= value.size()) {
    size_t end = value.find(',', start);
    if (end == std::string::npos) {
      end = value.size();
    }
    std::string token = Trim(value.substr(start, end - start));
    if (!token.empty()) {
      tokens.push_back(token);
    }
    if (end == value.size()) {
      break;
    }
    start = end + 1;
  }
}

std::vector<std::string> GetStringArrayAttribute(Element* element,
                                                 const std::string& name) {
  std::vector<std::string> values;
  if (!element) {
    return values;
  }

  std::string string_value = GetAttribute(element, name);
  if (!string_value.empty()) {
    AppendCommaSeparatedTokens(string_value, values);
    return values;
  }

  auto* node = element->data_model();
  if (!node) {
    return values;
  }
  for (const auto& attribute : node->attributes()) {
    if (attribute.first.str() != name || !attribute.second.IsArray()) {
      continue;
    }
    auto array = attribute.second.Array();
    if (!array) {
      return values;
    }
    for (size_t i = 0; i < array->size(); ++i) {
      const auto& item = array->get(i);
      if (item.IsString()) {
        std::string token = Trim(item.StdString());
        if (!token.empty()) {
          values.push_back(token);
        }
      }
    }
    return values;
  }
  return values;
}

std::string GetRole(Element* element) {
  if (element->parent() == nullptr) {
    return "RootWebArea";
  }

  std::string traits = GetAttribute(element, kAccessibilityTraits);
  if (HasTrait(traits, "button")) {
    return "button";
  }
  if (HasTrait(traits, "image")) {
    return "image";
  }
  if (HasTrait(traits, "link")) {
    return "link";
  }
  if (HasTrait(traits, "header")) {
    return "heading";
  }
  if (IsTrueAttribute(GetAttribute(element, kAccessibilityHeading))) {
    return "heading";
  }
  if (HasTrait(traits, "search") || HasTrait(traits, "searchfield")) {
    return "searchBox";
  }
  if (HasTrait(traits, "text")) {
    return "StaticText";
  }

  std::string tag = ElementInspector::LocalName(element);
  if (tag == "text" || tag == "raw-text" || tag == "inline-text") {
    return "StaticText";
  }
  if (tag == "image" || tag == "inline-image") {
    return "image";
  }
  return "generic";
}

std::string GetAccessibleName(Element* element) {
  std::string label = GetAttribute(element, kAccessibilityLabel);
  if (!label.empty()) {
    return label;
  }

  std::string text = GetAttribute(element, kText);
  if (!text.empty()) {
    return text;
  }

  std::string role = GetRole(element);
  if (role == "generic" || role == "RootWebArea") {
    return "";
  }

  std::string child_name;
  for (Element* child : element->GetChildren()) {
    child_name += GetAccessibleName(child);
  }
  return child_name;
}

bool HasAXSemantics(Element* element) {
  return IsTrueAttribute(GetAttribute(element, kAccessibilityElement)) ||
         !GetStringArrayAttribute(element, kAccessibilityActions).empty() ||
         IsTrueAttribute(GetAttribute(element, kAccessibilityHeading)) ||
         !GetAttribute(element, kAccessibilityLabel).empty() ||
         !GetAttribute(element, kAccessibilityRoleDescription).empty() ||
         !GetAttribute(element, kAccessibilityTraits).empty() ||
         !GetAttribute(element, kAccessibilityValue).empty() ||
         !GetAttribute(element, kText).empty();
}

bool IsLayoutOnlyGenericElement(Element* element) {
  if (!element || element->parent() == nullptr) {
    return false;
  }

  return GetRole(element) == "generic" && !HasAXSemantics(element);
}

Json::Value BuildIgnoredReasons(Element* element) {
  Json::Value ignored_reasons(Json::ValueType::arrayValue);

  std::string accessibility_element =
      GetAttribute(element, kAccessibilityElement);
  if (IsFalseAttribute(accessibility_element) ||
      IsLayoutOnlyGenericElement(element)) {
    ignored_reasons.append(BuildAXBooleanProperty("uninteresting", true));
  }

  std::string elements_hidden =
      GetAttribute(element, kAccessibilityElementsHidden);
  if (IsTrueAttribute(elements_hidden)) {
    ignored_reasons.append(BuildAXBooleanProperty("ariaHiddenSubtree", true));
  }

  return ignored_reasons;
}

Json::Value BuildProperties(Element* element) {
  Json::Value properties(Json::ValueType::arrayValue);

  std::string role_description =
      GetAttribute(element, kAccessibilityRoleDescription);
  if (!role_description.empty()) {
    properties.append(
        BuildAXStringProperty("roledescription", role_description));
  }

  std::string traits = GetAttribute(element, kAccessibilityTraits);
  if (HasTrait(traits, "disabled")) {
    properties.append(BuildAXBooleanProperty("disabled", true));
  }
  if (HasTrait(traits, "selected")) {
    properties.append(BuildAXBooleanProperty("selected", true));
  }

  std::vector<std::string> actions =
      GetStringArrayAttribute(element, kAccessibilityActions);
  if (!actions.empty()) {
    properties.append(BuildAXTokenListProperty("actions", actions));
  }

  return properties;
}

void AppendAXTree(Element* element, int depth, Json::Value& nodes) {
  if (!element) {
    return;
  }

  nodes.append(AccessibilityTreeHelper::BuildAXNode(element));
  if (depth == 0) {
    return;
  }

  int next_depth = depth == -1 ? -1 : depth - 1;
  for (Element* child : element->GetChildren()) {
    AppendAXTree(child, next_depth, nodes);
  }
}

}  // namespace

Json::Value AccessibilityTreeHelper::BuildAXNode(Element* element) {
  Json::Value node(Json::ValueType::objectValue);
  if (!element) {
    return node;
  }

  node["nodeId"] = GetAXNodeId(element);
  Json::Value ignored_reasons = BuildIgnoredReasons(element);
  node["ignored"] = !ignored_reasons.empty();
  if (!ignored_reasons.empty()) {
    node["ignoredReasons"] = ignored_reasons;
  }
  node["role"] = BuildAXValue("role", GetRole(element));
  node["name"] = BuildAXValue("computedString", GetAccessibleName(element));
  Json::Value properties = BuildProperties(element);
  if (!properties.empty()) {
    node["properties"] = properties;
  }
  std::string value = GetAttribute(element, kAccessibilityValue);
  if (!value.empty()) {
    node["value"] = BuildAXValue("string", value);
  }
  node["backendDOMNodeId"] = ElementInspector::NodeId(element);

  if (element->parent()) {
    node["parentId"] = GetAXNodeId(element->parent());
  }

  Json::Value child_ids(Json::ValueType::arrayValue);
  for (Element* child : element->GetChildren()) {
    child_ids.append(GetAXNodeId(child));
  }
  node["childIds"] = child_ids;

  return node;
}

Json::Value AccessibilityTreeHelper::BuildAXTree(Element* root, int depth) {
  Json::Value nodes(Json::ValueType::arrayValue);
  AppendAXTree(root, depth, nodes);
  return nodes;
}

}  // namespace devtool
}  // namespace lynx
