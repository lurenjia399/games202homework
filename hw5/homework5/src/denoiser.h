#pragma once

#define NOMINMAX
#include <string>

#include "filesystem/path.h"

#include "util/image.h"
#include "util/mathutil.h"

struct FrameInfo {
  public:
    Buffer2D<Float3> m_beauty;//渲染结果
    Buffer2D<float> m_depth;//每个像素的深度值
    Buffer2D<Float3> m_normal;// 每个像素的法线
    Buffer2D<Float3> m_position;// 每个像素在世界坐标系下的位置
    Buffer2D<float> m_id;// 每个像素对应的物体标号，背景是-1
    std::vector<Matrix4x4> m_matrix;// m_matrix[i] 表示标号为 i 的物体从物体坐标系到世界坐标系的矩阵。此外，m_matrix 中的倒数第 2 个和倒数第 1 个分别为，世界坐标系到摄像机坐标系和世界坐标系到屏幕坐标系 ([0, W)×[0,H)) 的矩阵
};

class Denoiser {
  public:
    Denoiser();

    void Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor);
    void Maintain(const FrameInfo &frameInfo);

    void Reprojection(const FrameInfo &frameInfo);
    void TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor);
    Buffer2D<Float3> Filter(const FrameInfo &frameInfo);

    Buffer2D<Float3> ProcessFrame(const FrameInfo &frameInfo);

  public:
    FrameInfo m_preFrameInfo;
    Buffer2D<Float3> m_accColor;
    Buffer2D<Float3> m_misc;
    Buffer2D<bool> m_valid;
    bool m_useTemportal;

    float m_alpha = 0.2f;
    float m_sigmaPlane = 0.1f;
    float m_sigmaColor = 0.6f;
    float m_sigmaNormal = 0.1f;
    float m_sigmaCoord = 32.0f;
    float m_colorBoxK = 1.0f;
};