#pragma once

#define NOMINMAX
#include <string>

#include "filesystem/path.h"

#include "util/image.h"
#include "util/mathutil.h"

struct FrameInfo {
  public:
    Buffer2D<Float3> m_beauty;//��Ⱦ���
    Buffer2D<float> m_depth;//ÿ�����ص����ֵ
    Buffer2D<Float3> m_normal;// ÿ�����صķ���
    Buffer2D<Float3> m_position;// ÿ����������������ϵ�µ�λ��
    Buffer2D<float> m_id;// ÿ�����ض�Ӧ�������ţ�������-1
    std::vector<Matrix4x4> m_matrix;// m_matrix[i] ��ʾ���Ϊ i ���������������ϵ����������ϵ�ľ��󡣴��⣬m_matrix �еĵ����� 2 ���͵����� 1 ���ֱ�Ϊ����������ϵ�����������ϵ����������ϵ����Ļ����ϵ ([0, W)��[0,H)) �ľ���
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