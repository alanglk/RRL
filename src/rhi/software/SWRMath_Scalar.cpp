// RRL/src/rhi/software/SWRMath_Scalar.cpp
#include "RRL/rhi/software/SWRMath.hpp"

namespace rrl::rhi::software::math {

#ifndef RRL_SWR_AFFINE_INTERPOLATION
void ProjectVertexBlock(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_inv_w, swr_vec1& out_clip_w) {
#else
void ProjectVertexBlock(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_clip_w) {
#endif

    for (size_t i = 0; i < SWRMesh::BlockSize; ++i) {
        
        // Read scalar vertex
        float x = pos_block.x[i];
        float y = pos_block.y[i];
        float z = pos_block.z[i];

        // Clip Space
        float clip_x = (mvp_mat[0][0] * x) + (mvp_mat[1][0] * y) + (mvp_mat[2][0] * z) + mvp_mat[3][0];
        float clip_y = (mvp_mat[0][1] * x) + (mvp_mat[1][1] * y) + (mvp_mat[2][1] * z) + mvp_mat[3][1];
        float clip_z = (mvp_mat[0][2] * x) + (mvp_mat[1][2] * y) + (mvp_mat[2][2] * z) + mvp_mat[3][2];
        float clip_w = (mvp_mat[0][3] * x) + (mvp_mat[1][3] * y) + (mvp_mat[2][3] * z) + mvp_mat[3][3];

        // NDC Space
        float inv_w = 1.0f / clip_w;
        float ndc_x = clip_x * inv_w;
        float ndc_y = clip_y * inv_w;
        float ndc_z = clip_z * inv_w;


        out_block.x[i] = ndc_x;
        out_block.y[i] = ndc_y;
        out_block.z[i] = ndc_z;
        out_clip_w.x[i] = clip_w;

        #ifndef RRL_SWR_AFFINE_INTERPOLATION
        out_inv_w.x[i] = inv_w;
        #endif
    }
}


} // namespace rrl::rhi::software::math