#include "denoiser.h"

Denoiser::Denoiser() : m_useTemportal(false) {}

void Denoiser::Reprojection(const FrameInfo &frameInfo) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    Matrix4x4 preWorldToScreen =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 1];
    Matrix4x4 preWorldToCamera =
        m_preFrameInfo.m_matrix[m_preFrameInfo.m_matrix.size() - 2];
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // TODO: Reproject
            
            

            float cur_m_id = frameInfo.m_id(x, y);
            Float3 curWorldPos = frameInfo.m_position(x, y);
            Matrix4x4 curModelToWorld = frameInfo.m_matrix[cur_m_id];
            Matrix4x4 preModelToWorld = m_preFrameInfo.m_matrix[m_preFrameInfo.m_id(x, y)];
            Matrix4x4 preWorldToScreen = preWorldToScreen;
            Float3 preFrameScreenPos = preWorldToScreen * preModelToWorld * Inverse(curModelToWorld) * curWorldPos;
            float pre_m_id = m_preFrameInfo.m_id(preFrameScreenPos.x, preFrameScreenPos.y);

            if (preFrameScreenPos.x > 0 
                && preFrameScreenPos.x < width 
                && preFrameScreenPos.y > 0 
                && preFrameScreenPos.y < height &&
                pre_m_id == cur_m_id) 
            {

                m_valid(x, y) = true;
            }else {
                m_valid(x, y) = false;
                
            }
            m_misc(x, y) = m_accColor(preFrameScreenPos.x, preFrameScreenPos.y);
        }
    }
    std::swap(m_misc, m_accColor);
}

void Denoiser::TemporalAccumulation(const Buffer2D<Float3> &curFilteredColor) {
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    int kernelRadius = 3;
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // TODO: Temporal clamp
            Float3 color = m_accColor(x, y);
            // TODO: Exponential moving average
            float alpha = 1.0f;
            m_misc(x, y) = Lerp(color, curFilteredColor(x, y), alpha);
        }
    }
    std::swap(m_misc, m_accColor);
}

Buffer2D<Float3> Denoiser::Filter(const FrameInfo &frameInfo) {
    int height = frameInfo.m_beauty.m_height;
    int width = frameInfo.m_beauty.m_width;
    Buffer2D<Float3> filteredImage = CreateBuffer2D<Float3>(width, height);
    int kernelRadius = 16;
#pragma omp parallel for
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            // TODO: Joint bilateral filter

            int x_min = (x - kernelRadius) < 0 ? 0 : x - kernelRadius;
            int x_max = (x + kernelRadius) > width ? width : x + kernelRadius;
            int y_min = (y - kernelRadius) < 0 ? 0 : y - kernelRadius;
            int y_max = (y + kernelRadius) > height ? height : y + kernelRadius;
            
            
            for (int h = y_min; h < y_max; h++) {
                for (int w = x_min; w < x_max; w++) {
                    float item1_norm = SqrLength(Float3(w - h, h - y, 0));
                    float item1_denorm = 2 * Sqr(m_sigmaPlane);
                    float item1 = item1_norm / item1_denorm;

                    float item2_norm = 0;
                    float item2_denorm = 2 * Sqr(m_sigmaColor);
                    float item2 = item2_norm / item2_denorm;

                    float item3_norm = SafeAcos(frameInfo.m_normal(x, y), frameInfo.m_normal(w, h));
                    item3_norm = Sqr(item3_norm);
                    float item3_denorm = 2 * Sqr(m_sigmaNormal);
                    float item3 = item3_norm / item3_denorm;

                    float item4_norm = frameInfo.m_normal(x, y) * Normalize(Float3(frameInfo.m_position(w, h) - frameInfo.m_position(x, y)));
                    item4_norm = Sqr(item4_norm);
                    float item4_denorm = 2 * Sqr(m_alpha);
                    float item4 = item4_norm / item4_denorm;

                    frameInfo.m_beauty(x, y) += exp(- item1 - item2 - item3 - item4);
                }
            }

            filteredImage(x, y) = frameInfo.m_beauty(x, y);
        }
    }
    return filteredImage;
}

void Denoiser::Init(const FrameInfo &frameInfo, const Buffer2D<Float3> &filteredColor) {
    m_accColor.Copy(filteredColor);
    int height = m_accColor.m_height;
    int width = m_accColor.m_width;
    m_misc = CreateBuffer2D<Float3>(width, height);
    m_valid = CreateBuffer2D<bool>(width, height);
}

void Denoiser::Maintain(const FrameInfo &frameInfo) { m_preFrameInfo = frameInfo; }

Buffer2D<Float3> Denoiser::ProcessFrame(const FrameInfo &frameInfo) {
    // Filter current frame
    Buffer2D<Float3> filteredColor;
    filteredColor = Filter(frameInfo);

    // Reproject previous frame color to current
    if (m_useTemportal) {
        Reprojection(frameInfo);
        TemporalAccumulation(filteredColor);
    } else {
        Init(frameInfo, filteredColor);
    }

    // Maintain
    Maintain(frameInfo);
    if (!m_useTemportal) {
        m_useTemportal = true;
    }
    return m_accColor;
}
