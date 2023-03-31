#pragma once

#include "./AnimRange.h"
#include <bee/polyfills/json.h>
#include <cstdint>
#include <fbxsdk.h>
#include <fx/gltf.h>
#include <glm/glm.hpp>
#include <string>
#include <string_view>
#include <variant>

namespace bee {
enum class neutral_interpolation_mode {
  constant,

  linear,

  cubic,
};

enum class neutral_tangent_weight_mode {
  none,

  left,

  right,

  both,
};

struct neutral_keyframe {
  constexpr neutral_keyframe(
      std::integral_constant<neutral_interpolation_mode,
                             neutral_interpolation_mode::constant> mode_,
      float time_,
      float value_)
      : interpolation_mode(neutral_interpolation_mode::constant), time(time_),
        value(value_) {
  }

  constexpr neutral_keyframe(
      std::integral_constant<neutral_interpolation_mode,
                             neutral_interpolation_mode::linear> mode_,
      float time_,
      float value_)
      : interpolation_mode(neutral_interpolation_mode::linear), time(time_),
        value(value_) {
  }

  constexpr neutral_keyframe(
      std::integral_constant<neutral_interpolation_mode,
                             neutral_interpolation_mode::cubic> mode_,
      float time_,
      float value_,
      neutral_tangent_weight_mode tangent_weight_mode_,
      float left_tangent_,
      float left_tangent_weight_,
      float right_tangent_,
      float right_tangent_weight_)
      : interpolation_mode(neutral_interpolation_mode::cubic), time(time_),
        value(value_), tangent_weight_mode(tangent_weight_mode_),
        left_tangent(left_tangent_), left_tangent_weight(left_tangent_weight_),
        right_tangent(right_tangent_),
        right_tangent_weight(right_tangent_weight_) {
  }

  neutral_interpolation_mode interpolation_mode;
  double time;
  double value;
  neutral_tangent_weight_mode tangent_weight_mode =
      neutral_tangent_weight_mode::none;
  double left_tangent = 0.0;
  double left_tangent_weight = 0.0;
  double right_tangent = 0.0;
  double right_tangent_weight = 0.0;
};

struct neutral_curve {
  std::vector<neutral_keyframe> keyframes;
};

struct componentwise_channel {
  fx::gltf::Animation::Channel::Target target;
  std::string componentName;
  neutral_curve sampler;
};

void to_json(nlohmann::json &j_, const neutral_keyframe &keyframe_) {
  j_["interpolationMode"] = keyframe_.interpolation_mode;
  j_["value"] = keyframe_.value;
  if (keyframe_.interpolation_mode == neutral_interpolation_mode::cubic) {
    if (keyframe_.left_tangent != 0.0) {
      j_["inTangent"] = keyframe_.left_tangent;
    }
    if (keyframe_.right_tangent != 0.0) {
      j_["outTangent"] = keyframe_.right_tangent;
    }
    if (keyframe_.tangent_weight_mode != neutral_tangent_weight_mode::none) {
      j_["tangentWeightMode"] = keyframe_.tangent_weight_mode;
    }
    if (keyframe_.tangent_weight_mode == neutral_tangent_weight_mode::left ||
        keyframe_.tangent_weight_mode == neutral_tangent_weight_mode::both) {
      if (keyframe_.left_tangent_weight != (1.0f / 3.0f)) {
        j_["inTangentWeight"] = keyframe_.left_tangent_weight;
      }
    }
    if (keyframe_.tangent_weight_mode == neutral_tangent_weight_mode::right ||
        keyframe_.tangent_weight_mode == neutral_tangent_weight_mode::both) {
      if (keyframe_.right_tangent_weight != (1.0f / 3.0f)) {
        j_["outTangentWeight"] = keyframe_.right_tangent_weight;
      }
    }
  }
}

void to_json(nlohmann::json &j_, const componentwise_channel &channel_) {
  nlohmann::json sampleJson = {{"keyframes", channel_.sampler.keyframes}};

  j_ = {{"target", channel_.target},
        {"componentName", channel_.componentName},
        {"sampler", sampleJson},
        {"keyframes", channel_.sampler.keyframes}};
}

neutral_curve convert_fbx_curve(fbxsdk::FbxAnimCurve &fbx_curve_) {
  neutral_curve curve;

  const auto nKeys = fbx_curve_.KeyGetCount();
  for (decltype(fbx_curve_.KeyGetCount()) iKey = 0; iKey < nKeys; ++iKey) {
    auto fbxKey = fbx_curve_.KeyGet(iKey);
    const auto time = static_cast<float>(fbxKey.GetTime().GetSecondDouble());
    const auto value = fbxKey.GetValue();
    const auto fbxInterpolationMode = fbxKey.GetInterpolation();
    switch (fbxInterpolationMode) {
    case fbxsdk::FbxAnimCurveDef::EInterpolationType::eInterpolationConstant: {
      const auto constantValue =
          fbxKey.GetConstantMode() == fbxsdk::FbxAnimCurveDef::EConstantMode::
                                          eConstantStandard &&
                  iKey != nKeys - 1
              ? value
              : fbx_curve_.KeyGet(iKey + 1).GetValue();
      curve.keyframes.push_back(neutral_keyframe{
          std::integral_constant<neutral_interpolation_mode,
                                 neutral_interpolation_mode::constant>{},
          time, constantValue});
      break;
    }
    case fbxsdk::FbxAnimCurveDef::EInterpolationType::eInterpolationLinear:
      curve.keyframes.push_back(neutral_keyframe{
          std::integral_constant<neutral_interpolation_mode,
                                 neutral_interpolation_mode::linear>{},
          time, value});
      break;
    case fbxsdk::FbxAnimCurveDef::EInterpolationType::eInterpolationCubic: {
      const auto fbxTangentMode = fbxKey.GetTangentMode();
      auto tangentWeightMode = neutral_tangent_weight_mode::none;
      switch (fbxKey.GetTangentWeightMode()) {
      default:
      case fbxsdk::FbxAnimCurveDef::EWeightedMode::eWeightedNone:
        tangentWeightMode = neutral_tangent_weight_mode::none;
        break;
      case fbxsdk::FbxAnimCurveDef::EWeightedMode::eWeightedNextLeft:
        tangentWeightMode = neutral_tangent_weight_mode::left;
        break;
      case fbxsdk::FbxAnimCurveDef::EWeightedMode::eWeightedRight:
        tangentWeightMode = neutral_tangent_weight_mode::right;
        break;
      case fbxsdk::FbxAnimCurveDef::EWeightedMode::eWeightedAll:
        tangentWeightMode = neutral_tangent_weight_mode::both;
        break;
      }

      curve.keyframes.push_back(neutral_keyframe{
          std::integral_constant<neutral_interpolation_mode,
                                 neutral_interpolation_mode::cubic>{},
          time,
          value,
          tangentWeightMode,
          fbxKey.GetDataFloat(
              fbxsdk::FbxAnimCurveDef::EDataIndex::eNextLeftSlope),
          fbxKey.GetDataFloat(
              fbxsdk::FbxAnimCurveDef::EDataIndex::eNextLeftWeight),
          fbxKey.GetDataFloat(fbxsdk::FbxAnimCurveDef::EDataIndex::eRightSlope),
          fbxKey.GetDataFloat(
              fbxsdk::FbxAnimCurveDef::EDataIndex::eRightWeight),

      });
      break;
    }
    default:
      break;
    }
  }

  return curve;
}

void extract_original_curves(fx::gltf::Animation &glTF_animation_,
                             std::int32_t glTF_node_index_,
                             fbxsdk::FbxAnimLayer &fbx_anim_layer_,
                             fbxsdk::FbxNode &fbx_node_,
                             const AnimRange &anim_range_) {
  const auto extract =
      [&glTF_animation_, glTF_node_index_, &fbx_node_, &fbx_anim_layer_,
       &anim_range_](fbxsdk::FbxAnimCurve &fbx_curve_,
                     std::string_view glTF_property_path_,
                     std::string_view glTF_component_name_) {
        const auto curve = convert_fbx_curve(fbx_curve_);

        componentwise_channel channel;
        channel.target.node = glTF_node_index_;
        channel.target.path = glTF_property_path_;
        channel.componentName = glTF_component_name_;
        channel.sampler = curve;

        glTF_animation_
            .extensionsAndExtras["extras"]["FBX-glTF-conv"]
                                ["componentwise_channels"]
            .push_back(channel);
      };

  if (fbx_node_.LclTranslation.IsAnimated(&fbx_anim_layer_)) {
    extract(
        *fbx_node_.LclTranslation.GetCurve(&fbx_anim_layer_,
                                           FBXSDK_CURVENODE_COMPONENT_X),
        "transition", "x");
    extract(
        *fbx_node_.LclTranslation.GetCurve(&fbx_anim_layer_,
                                           FBXSDK_CURVENODE_COMPONENT_Y),
        "transition", "y");
    extract(
        *fbx_node_.LclTranslation.GetCurve(&fbx_anim_layer_,
                                           FBXSDK_CURVENODE_COMPONENT_Z),
        "transition", "z");
  }

  if (fbx_node_.LclRotation.IsAnimated(&fbx_anim_layer_)) {
    extract(
        *fbx_node_.LclRotation.GetCurve(&fbx_anim_layer_,
                                        FBXSDK_CURVENODE_COMPONENT_X),
        "eularAngles", "x");
    extract(
        *fbx_node_.LclRotation.GetCurve(&fbx_anim_layer_,
                                        FBXSDK_CURVENODE_COMPONENT_Y),
        "eularAngles", "y");
    extract(
        *fbx_node_.LclRotation.GetCurve(&fbx_anim_layer_,
                                        FBXSDK_CURVENODE_COMPONENT_Z),
        "eularAngles", "z");
  }

  if (fbx_node_.LclScaling.IsAnimated(&fbx_anim_layer_)) {
    extract(
        *fbx_node_.LclScaling.GetCurve(&fbx_anim_layer_,
                                       FBXSDK_CURVENODE_COMPONENT_X),
        "scale", "x");
    extract(
        *fbx_node_.LclScaling.GetCurve(&fbx_anim_layer_,
                                       FBXSDK_CURVENODE_COMPONENT_Y),
        "scale", "y");
    extract(
        *fbx_node_.LclScaling.GetCurve(&fbx_anim_layer_,
                                       FBXSDK_CURVENODE_COMPONENT_Z),
        "scale", "z");
  }
}
} // namespace bee