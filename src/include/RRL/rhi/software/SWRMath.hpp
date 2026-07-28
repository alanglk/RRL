// RRL/include/rhi/software/SWRMath.hpp
#pragma once
#include <glm/glm.hpp>
#include "RRL/rhi/software/SWRTypes.hpp"


namespace rrl::rhi::software::math {

/**
 * @brief Transforms a single SIMD block of positions by an MVP matrix, 
 * performing perspective division and outputting the NDC coordinates.
 */
#ifndef RRL_SWR_AFFINE_INTERPOLATION
void ProjectVertexBlock(
    const swr_vec3& pos_block, 
    const glm::mat4& mvp_mat, 
    swr_vec3& out_block, 
    swr_vec1& out_inv_w, 
    swr_vec1& out_clip_w
);
#else
void ProjectVertexBlock(
    const swr_vec3& pos_block, 
    const glm::mat4& mvp_mat, 
    swr_vec3& out_block, 
    swr_vec1& out_clip_w
);
#endif


} // namespace rrl::rhi::software::math