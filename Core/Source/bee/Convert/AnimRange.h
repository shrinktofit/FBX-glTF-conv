#pragma once

#include <fbxsdk.h>

namespace bee {
struct AnimRange {
private:
  fbxsdk::FbxTime::EMode timeMode;
  fbxsdk::FbxLongLong firstFrame;
  /// <summary>
  /// Last frame(inclusive).
  /// </summary>
  fbxsdk::FbxLongLong lastFrame;

public:
  AnimRange(fbxsdk::FbxTime::EMode time_mode_,
            fbxsdk::FbxLongLong first_frame_,
            fbxsdk::FbxLongLong last_frame_)
      : timeMode(time_mode_), firstFrame(first_frame_), lastFrame(last_frame_) {
  }

  fbxsdk::FbxLongLong frames_count() const {
    return lastFrame - firstFrame + 1;
  }

  fbxsdk::FbxDouble first_frame_seconds() const {
    return at(0).GetSecondDouble();
  }

  fbxsdk::FbxTime at(fbxsdk::FbxLongLong frame_index_) const {
    const auto fbxFrame = firstFrame + frame_index_;
    fbxsdk::FbxTime fbxTime;
    fbxTime.SetFrame(fbxFrame, timeMode);
    return fbxTime;
  }
};
} // namespace bee