
#include <bee/Convert/ConvertError.h>
#include <bee/Convert/SceneConverter.h>
#include <fmt/format.h>

namespace bee {
/// <summary>
/// glTF does not allow sub-meshes have different number of targets.
/// </summary>
class InconsistentTargetsCountError
    : public NodeError<InconsistentTargetsCountError> {
public:
  constexpr static inline std::u8string_view code =
      u8"inconsistent_target_count";

  using NodeError::NodeError;
};

void to_json(nlohmann::json &j_, const InconsistentTargetsCountError &error_) {
  to_json(j_, static_cast<const NodeError<InconsistentTargetsCountError> &>(
                  error_));
}

SceneConverter::FbxBlendShapeData::FbxBlendShapeData(
    fbxsdk::FbxBlendShape &blend_shape_, int blend_shape_index_)
    : _blendShape(&blend_shape_), _blendShapeIndex(blend_shape_index_) {

  const auto addShape = [this](fbxsdk::FbxShape &shape_) {
    const auto r =
        std::find(this->_shapes.begin(), this->_shapes.end(), &shape_);
    if (r != this->_shapes.end()) {
      return decltype(this->_shapes)::size_type(r - this->_shapes.end());
    } else {
      this->_shapes.push_back(&shape_);
      return this->_shapes.size() - 1;
    }
  };

  const auto nChannels = blend_shape_.GetBlendShapeChannelCount();
  for (std::remove_const_t<decltype(nChannels)> iChannel = 0;
       iChannel < nChannels; ++iChannel) {
    const auto blendShapeChannel = blend_shape_.GetBlendShapeChannel(iChannel);
    auto fullWeights = blendShapeChannel->GetTargetShapeFullWeights();
    if (const auto nTargetShapes = blendShapeChannel->GetTargetShapeCount()) {
      decltype(FbxBlendShapeData::Channel::targetShapes) targetShapes(
          nTargetShapes);
      for (std::remove_const_t<decltype(nTargetShapes)> iTargetShape = 0;
           iTargetShape < nTargetShapes; ++iTargetShape) {
        auto targetShape = blendShapeChannel->GetTargetShape(iTargetShape);
        const auto shapeId = addShape(*targetShape);
        targetShapes[iTargetShape] = {shapeId, fullWeights[iTargetShape]};
      }
      this->_channels.push_back(FbxBlendShapeData::Channel{
          iChannel, std::string(blendShapeChannel->GetName()),
          blendShapeChannel->DeformPercent.Get(), std::move(targetShapes)});
    }
  }
}

bool SceneConverter::FbxBlendShapeData::has_same_construct_with(
    const FbxBlendShapeData &that_) const {
  if (this->_channels.size() != that_._channels.size()) {
    return false;
  }
  if (this->_shapes.size() != that_._shapes.size()) {
    return false;
  }
  if (!std::equal(this->_channels.begin(), this->_channels.end(),
                  that_._channels.begin(), that_._channels.end(),
                  [](const FbxBlendShapeData::Channel &lhs_,
                     const FbxBlendShapeData::Channel &rhs_) {
                    return lhs_.name == rhs_.name &&
                           lhs_.targetShapes.size() == rhs_.targetShapes.size();
                  })) {
    return false;
  }
  // if (!std::equal(this->_shapes.begin(), this->_shapes.end(),
  //                 that_._shapes.begin(), that_._shapes.end(),
  //                 [](const auto lhs_, const auto rhs_) {
  //                   return lhs_->name == rhs_->name;
  //                 })) {
  //   return false;
  // }
  return true;
}

std::optional<SceneConverter::FbxBlendShapeData>
SceneConverter::_extractdBlendShapeData(const fbxsdk::FbxMesh &fbx_mesh_) {
  const auto nBlendShape = fbx_mesh_.GetDeformerCount(
      fbxsdk::FbxDeformer::EDeformerType::eBlendShape);

  for (std::remove_const_t<decltype(nBlendShape)> iBlendShape = 0;
       iBlendShape < nBlendShape; ++iBlendShape) {
    const auto fbxBlendShape =
        static_cast<fbxsdk::FbxBlendShape *>(fbx_mesh_.GetDeformer(
            iBlendShape, fbxsdk::FbxDeformer::EDeformerType::eBlendShape));
    FbxBlendShapeData blendShapeData{*fbxBlendShape, iBlendShape};
    if (blendShapeData.empty()) {
      return {};
    }
    return blendShapeData;
  }

  return {};
}

std::optional<SceneConverter::FbxNodeMeshesBumpMeta::BlendShapeDumpMeta>
SceneConverter::_extractNodeMeshesBlendShape(
    const std::vector<fbxsdk::FbxMesh *> &fbx_meshes_) {
  if (fbx_meshes_.empty()) {
    return {};
  }

  std::vector<std::optional<FbxBlendShapeData>> blendShapeDatas;
  blendShapeDatas.reserve(fbx_meshes_.size());
  std::transform(fbx_meshes_.begin(), fbx_meshes_.end(),
                 std::back_inserter(blendShapeDatas),
                 [this](fbxsdk::FbxMesh *fbx_mesh_) {
                   return _extractdBlendShapeData(*fbx_mesh_);
                 });

  const auto &firstBlendShapeData = blendShapeDatas.front();

  const auto hasSameStruct =
      [&](const std::optional<FbxBlendShapeData> &blend_shape_data_) {
        if (blend_shape_data_.has_value() != firstBlendShapeData.has_value()) {
          return false;
        }
        if (!firstBlendShapeData) {
          return true;
        }
        return firstBlendShapeData->has_same_construct_with(*blend_shape_data_);
      };

  if (!std::all_of(std::next(blendShapeDatas.begin()), blendShapeDatas.end(),
                   hasSameStruct)) {
    _log(Logger::Level::warning,
         InconsistentTargetsCountError{
             fbx_meshes_.front()->GetNode()->GetName()});
  }

  if (!firstBlendShapeData) {
    return {};
  }

  FbxNodeMeshesBumpMeta::BlendShapeDumpMeta myMeta;
  myMeta.blendShapeDatas.reserve(blendShapeDatas.size());
  std::transform(blendShapeDatas.begin(), blendShapeDatas.end(),
                 std::back_inserter(myMeta.blendShapeDatas),
                 [](auto &v_) { return std::move(*v_); });

  return myMeta;
}
} // namespace bee