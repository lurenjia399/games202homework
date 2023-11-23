#include <iostream>
#include <vector>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <fstream>
#include <random>
#include "vec.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "stb_image_write.h"

const int resolution = 128;

typedef struct samplePoints {
    std::vector<Vec3f> directions;
	std::vector<float> PDFs;
}samplePoints;

samplePoints squareToCosineHemisphere(int sample_count){
    samplePoints samlpeList;
    const int sample_side = static_cast<int>(floor(sqrt(sample_count)));

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<> rng(0.0, 1.0);
    for (int t = 0; t < sample_side; t++) {
        for (int p = 0; p < sample_side; p++) {
            double samplex = (t + rng(gen)) / sample_side;
            double sampley = (p + rng(gen)) / sample_side;
            
            double theta = 0.5f * acos(1 - 2*samplex);
            double phi =  2 * std::_Pi * sampley;
            Vec3f wi = Vec3f(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float pdf = wi.z / PI;
            
            samlpeList.directions.push_back(wi);
            samlpeList.PDFs.push_back(pdf);
        }
    }
    return samlpeList;
}

float DistributionGGX(Vec3f N, Vec3f H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = std::max(dot(N, H), 0.0f);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / std::max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float a = roughness;
    float k = (a * a) / 2.0f;

    float nom = NdotV;
    float denom = NdotV * (1.0f - k) + k;

    return nom / denom;
}

float GeometrySmith(float roughness, float NoV, float NoL) {
    float ggx2 = GeometrySchlickGGX(NoV, roughness);
    float ggx1 = GeometrySchlickGGX(NoL, roughness);

    return ggx1 * ggx2;
}

Vec3f IntegrateBRDF(Vec3f V, float roughness, float NdotV) {
    float A = 0.0;
    float B = 0.0;
    float C = 0.0;
    const int sample_count = 1024;
    Vec3f N = Vec3f(0.0, 0.0, 1.0);
    // 注意这里反射率取 1.0 了
    // 我猜测是因为，我们要算的东西是，在BRDF下反射出的能量，材质自己本身不吸收
    float R0 = 1.0;

    samplePoints sampleList = squareToCosineHemisphere(sample_count);
    for (int i = 0; i < sample_count; i++) {
      // TODO: To calculate (fr * ni) / p_o here
        Vec3f L = sampleList.directions[i];
        L = L / std::sqrt(L.x * L.x + L.y * L.y + L.z * L.z);
        Vec3f H = (V + L);
        H = H / std::sqrt(H.x * H.x + H.y * H.y + H.z * H.z);
        float F =  R0 + (1.0 - R0) * std::pow((1 - NdotV), 5);
        float D = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(roughness, dot(N, V), dot(N, L));//这里求G的时候，传的需要是法线，而不是半程向量
        float BRDF = F * D * G / (4 * dot(N, V) * dot(N, L));

        float NdotL = std::fmax(dot(N, L), 0.0);
        // 注意这个地方，求得cos值，不是sin值，也不是cos * sin
        /*
        * 我们渲染方程可以写成三种形式
        * 1 对立体角积分，正就是我们正常说的那种形式，
        * 2 对theta和phi积分，也就是把d立体角展开成dtheta * dphi
        * 3 在第二种的情况下进行合并，将cos * dtheta 变成 dsin(theta)
        *总结:1 里面，被积函数是cos(theta)
        *     2 里面，被积函数是cos(theta) * sin(theta)
        *     3 里面，被积函数是sin(theta)
        */
        // 在这里的情况下，因为我们采样的时候的积分项是立体角，所以就选用第一种情况，也就是被积函数里面是cos(theta)
        float ni = NdotL;

        A += BRDF * ni / sampleList.PDFs[i];
        B += BRDF * ni / sampleList.PDFs[i];
        C += BRDF * ni / sampleList.PDFs[i];
      
    }

    return {A / sample_count, B / sample_count, C / sample_count};
}

int main() {
    uint8_t data[resolution * resolution * 3];
    float step = 1.0 / resolution;
    for (int i = 0; i < resolution; i++) {
        for (int j = 0; j < resolution; j++) {
            float roughness = step * (static_cast<float>(i) + 0.5f);
            float NdotV = step * (static_cast<float>(j) + 0.5f);
            Vec3f V = Vec3f(std::sqrt(1.f - NdotV * NdotV), 0.f, NdotV);

            Vec3f irr = IntegrateBRDF(V, roughness, NdotV);

            data[(i * resolution + j) * 3 + 0] = uint8_t(irr.x * 255.0);
            data[(i * resolution + j) * 3 + 1] = uint8_t(irr.y * 255.0);
            data[(i * resolution + j) * 3 + 2] = uint8_t(irr.z * 255.0);
        }
    }
    stbi_flip_vertically_on_write(true);
    stbi_write_png("GGX_E_MC_LUT.png", resolution, resolution, 3, data, resolution * 3);
    
    std::cout << "Finished precomputed!" << std::endl;
    return 0;
}