#pragma once

#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "tekki/core/Result.h"

namespace tekki::rust_shaders {

constexpr int32_t KERNEL_RADIUS = 5;
constexpr uint32_t GROUP_WIDTH = 64;
constexpr size_t VBLUR_WINDOW_SIZE = ((GROUP_WIDTH + KERNEL_RADIUS) * 2);

class Blur {
public:
    static float GaussianWt(float dstPx, float srcPx) {
        float pxOff = (dstPx + 0.5f) * 2.0f - (srcPx + 0.5f);
        float sigma = static_cast<float>(KERNEL_RADIUS) * 0.5f;
        return std::exp(-pxOff * pxOff / (sigma * sigma));
    }

    static glm::vec4 VBlur(const std::shared_ptr<void>& inputTex, const glm::ivec2& dstPx, const glm::ivec2& srcPx) {
        glm::vec4 res(0.0f);
        float wtSum = 0.0f;

        int32_t y = 0;
        while (y < KERNEL_RADIUS * 2) {
            float wt = GaussianWt(static_cast<float>(dstPx.y), static_cast<float>(srcPx.y + y));
            glm::vec4 value = FetchTexture(inputTex, srcPx + glm::ivec2(0, y));
            res += value * wt;
            wtSum += wt;
            y += 1;
        }

        return res / wtSum;
    }

    static void VBlurIntoShmem(
        const std::shared_ptr<void>& inputTex,
        std::vector<glm::vec4>& vblurOut,
        const glm::ivec2& dstPx,
        int32_t xfetch,
        const glm::uvec2& groupId
    ) {
        glm::ivec2 srcPx = glm::ivec2(groupId) * glm::ivec2(static_cast<int32_t>(GROUP_WIDTH) * 2, 2)
            + glm::ivec2(xfetch - KERNEL_RADIUS, -KERNEL_RADIUS);
        vblurOut[static_cast<size_t>(xfetch)] = VBlur(inputTex, dstPx, srcPx);
    }

    static void BlurCs(
        const std::shared_ptr<void>& inputTex,
        const std::shared_ptr<void>& outputTex,
        std::vector<glm::vec4>& vblurOut,
        const glm::uvec3& px,
        const glm::uvec3& pxWithinGroup,
        const glm::uvec3& groupId
    ) {
        glm::uvec2 px2D = glm::uvec2(px.x, px.y);
        glm::uvec2 groupId2D = glm::uvec2(groupId.x, groupId.y);

        uint32_t xfetch = pxWithinGroup.x;
        while (xfetch < VBLUR_WINDOW_SIZE) {
            VBlurIntoShmem(inputTex, vblurOut, glm::ivec2(px2D), static_cast<int32_t>(xfetch), groupId2D);
            xfetch += GROUP_WIDTH;
        }

        ControlBarrier();

        glm::vec4 res(0.0f);
        float wtSum = 0.0f;

        for (int32_t x = 0; x <= KERNEL_RADIUS * 2; ++x) {
            float wt = GaussianWt(static_cast<float>(px2D.x), static_cast<float>(static_cast<int32_t>(px2D.x) * 2 + x - KERNEL_RADIUS));
            res += vblurOut[static_cast<size_t>(pxWithinGroup.x * 2 + static_cast<uint32_t>(x))] * wt;
            wtSum += wt;
        }
        res /= wtSum;

        WriteTexture(outputTex, px2D, res);
    }

private:
    static glm::vec4 FetchTexture(const std::shared_ptr<void>& texture, const glm::ivec2& coord) {
        return glm::vec4(0.0f);
    }

    static void WriteTexture(const std::shared_ptr<void>& texture, const glm::uvec2& coord, const glm::vec4& value) {
    }

    static void ControlBarrier() {
    }
};

} // namespace tekki::rust_shaders