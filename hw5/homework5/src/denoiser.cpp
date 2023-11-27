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
            if(cur_m_id < 0 )
            {
                continue;
            }
            Float3 curWorldPos = frameInfo.m_position(x, y);
            Matrix4x4 curWorldToModel = Inverse(frameInfo.m_matrix[cur_m_id]);
            Matrix4x4 preModelToWorld = m_preFrameInfo.m_matrix[m_preFrameInfo.m_id(x, y)];
            Matrix4x4 preWorldToScreen = preWorldToScreen;

            Float3 model = curWorldToModel(curWorldPos, Float3::EType::Point);
            Matrix4x4 preModelToScreen = preWorldToScreen * preModelToWorld;
            Float3 preModelToScreenPos = preModelToScreen(model, Float3::EType::Point);

            float pre_m_id = m_preFrameInfo.m_id(preModelToScreenPos.x, preModelToScreenPos.y);

            if (preModelToScreenPos.x > 0
                && preModelToScreenPos.x < width
                && preModelToScreenPos.y > 0
                && preModelToScreenPos.y < height &&
                pre_m_id == cur_m_id) 
            {

                m_valid(x, y) = true;
            }else {
                m_valid(x, y) = false;
                
            }
            m_misc(x, y) = m_accColor(preModelToScreenPos.x, preModelToScreenPos.y);
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
            Float3 avg_color = Float3(0.0);
            Float3 fc_color = Float3(0.0);
            int x_min = (x - kernelRadius) < 0 ? 0 : x - kernelRadius;
            int x_max = (x + kernelRadius) > width ? width : x + kernelRadius;
            int y_min = (y - kernelRadius) < 0 ? 0 : y - kernelRadius;
            int y_max = (y + kernelRadius) > height ? height : y + kernelRadius;
            for (int h = y_min; h < y_max; h++) 
            {
                for (int w = x_min; w < x_max; w++) 
                {
                    avg_color += curFilteredColor(w, h) / 49;
                }
            }

            for (int h = y_min; h < y_max; h++)
            {
                for (int w = x_min; w < x_max; w++)
                {
                    fc_color += Sqr(curFilteredColor(w, h) - avg_color) / 49;
                }
            }

            color = Clamp(color, avg_color - fc_color * 2, avg_color + fc_color * 2);

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
            
            auto center_postion = frameInfo.m_position(x, y);
            auto center_normal = frameInfo.m_normal(x, y);
            auto center_color = frameInfo.m_beauty(x, y);
            
            Float3 final_color;
            auto total_weight = .0f;

            for (int h = y_min; h < y_max; h++) {
                for (int w = x_min; w < x_max; w++) {

                    auto postion = frameInfo.m_position(w, h);
                    auto normal = frameInfo.m_normal(w, h);
                    auto color = frameInfo.m_beauty(w, h);

                    float item1_norm = SqrDistance(center_postion, postion);
                    float item1_denorm = 2 * Sqr(m_sigmaCoord);
                    float item1 = item1_norm / item1_denorm;

                    float item2_norm = SqrDistance(center_color, color);
                    float item2_denorm = 2 * Sqr(m_sigmaColor);
                    float item2 = item2_norm / item2_denorm;

                    float item3_norm = SafeAcos(Dot(center_normal, normal));
                    item3_norm = Sqr(item3_norm);
                    float item3_denorm = 2 * Sqr(m_sigmaNormal);
                    float item3 = item3_norm / item3_denorm;

                    float item4_norm = Dot(center_normal, Normalize(Float3(center_postion - postion)));
                    item4_norm = Sqr(item4_norm);
                    float item4_denorm = 2 * Sqr(m_sigmaPlane);
                    float item4 = item4_norm / item4_denorm;

                    // 这段结尾还需要看下联合双边滤波
                    float weight = exp(-item1 - item2 - item3 - item4);
                    total_weight += weight;
                    final_color += color * weight;
                }
            }

            filteredImage(x, y) = final_color / total_weight;
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
