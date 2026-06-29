// RRL/src/rhi/software/SWRMath_Highway.cpp

#ifndef RRL_ENABLE_HIGHWAY
    #error "This file (SWRMath_Highway.cpp) requires Google Highway support."
#endif

#include "RRL/rhi/software/SWRMath.hpp"
#include <hwy/highway.h>

namespace rrl::rhi::software::math {
namespace HWY_NAMESPACE {
namespace hn = hwy::HWY_NAMESPACE;


#ifndef RRL_SWR_AFFINE_INTERPOLATION
void ProjectVertexBlockImpl(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_inv_w, swr_vec1& out_clip_w) {
#else
void ProjectVertexBlockImpl(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_clip_w) {
#endif

    // Floating-point SIMD tag (used for acutal math)
    hn::FixedTag<float, RRL_SWR_SIMD_NUM_32BIT_ELMS> df;
    
#ifdef RRL_SWR_FIXED_POINT
    // Integer SIMD tag (used for fixed point conversion / simulate fixed point math)
    hn::FixedTag<int32_t, RRL_SWR_SIMD_NUM_32BIT_ELMS> di;

    // Load integers
    auto v_x_i32 = hn::Load(di, pos_block.x);
    auto v_y_i32 = hn::Load(di, pos_block.y);
    auto v_z_i32 = hn::Load(di, pos_block.z);

    // Convert to float and reverse the fixed-point scale factor
    auto inv_scale = hn::Set(df, 1.0f / 65536.0f);
    auto v_x = hn::Mul(hn::ConvertTo(df, v_x_i32), inv_scale);
    auto v_y = hn::Mul(hn::ConvertTo(df, v_y_i32), inv_scale);
    auto v_z = hn::Mul(hn::ConvertTo(df, v_z_i32), inv_scale);
#else
    // Direct float loading
    auto v_x = hn::Load(df, pos_block.x);
    auto v_y = hn::Load(df, pos_block.y);
    auto v_z = hn::Load(df, pos_block.z);
#endif


    // Clip Space X -> px = ([0][0]*X) + ([1][0]*Y) + ([2][0]*Z) + ([3][0]*1.0)
    auto clip_x = hn::Set(df, mvp_mat[3][0]);
    clip_x = hn::MulAdd(hn::Set(df, mvp_mat[0][0]), v_x, clip_x);
    clip_x = hn::MulAdd(hn::Set(df, mvp_mat[1][0]), v_y, clip_x);
    clip_x = hn::MulAdd(hn::Set(df, mvp_mat[2][0]), v_z, clip_x);
    
    // Clip Space Y -> py =([0][1]*X) + ([1][1]*Y) + ([2][1]*Z) + ([3][1]*1.0) 
    auto clip_y = hn::Set(df, mvp_mat[3][1]);
    clip_y = hn::MulAdd(hn::Set(df, mvp_mat[0][1]), v_x, clip_y);
    clip_y = hn::MulAdd(hn::Set(df, mvp_mat[1][1]), v_y, clip_y);
    clip_y = hn::MulAdd(hn::Set(df, mvp_mat[2][1]), v_z, clip_y);

    // Clip Space Z -> pz = ([0][2]*X) + ([1][2]*Y) + ([2][2]*Z) + ([3][2]*1.0)
    auto clip_z = hn::Set(df, mvp_mat[3][2]);
    clip_z = hn::MulAdd(hn::Set(df, mvp_mat[0][2]), v_x, clip_z);
    clip_z = hn::MulAdd(hn::Set(df, mvp_mat[1][2]), v_y, clip_z);
    clip_z = hn::MulAdd(hn::Set(df, mvp_mat[2][2]), v_z, clip_z);

    // Clip Space W -> pz = ([0][2]*X) + ([1][2]*Y) + ([2][2]*Z) + ([3][2]*1.0)
    auto clip_w = hn::Set(df, mvp_mat[3][3]);
    clip_w = hn::MulAdd(hn::Set(df, mvp_mat[0][3]), v_x, clip_w);
    clip_w = hn::MulAdd(hn::Set(df, mvp_mat[1][3]), v_y, clip_w);
    clip_w = hn::MulAdd(hn::Set(df, mvp_mat[2][3]), v_z, clip_w);
    
    // NDC -> Perspective division
    auto inv_w = hn::Div(hn::Set(df, 1.0f), clip_w);
    auto ndc_x = hn::Mul(clip_x, inv_w);
    auto ndc_y = hn::Mul(clip_y, inv_w);
    auto ndc_z = hn::Mul(clip_z, inv_w);


    #ifdef RRL_SWR_FIXED_POINT

    // Convert back to fixed-point integers before storing
    auto scale = hn::Set(df, 65536.0f);
    
    auto ndc_x_i32 = hn::ConvertTo(di, hn::Mul(ndc_x, scale));
    auto ndc_y_i32 = hn::ConvertTo(di, hn::Mul(ndc_y, scale));
    auto ndc_z_i32 = hn::ConvertTo(di, hn::Mul(ndc_z, scale));
    auto clip_w_i32 = hn::ConvertTo(di, hn::Mul(clip_w, scale));

    hn::Store(ndc_x_i32, di, out_block.x);
    hn::Store(ndc_y_i32, di, out_block.y);
    hn::Store(ndc_z_i32, di, out_block.z);
    hn::Store(clip_w_i32, di, out_clip_w.x);

    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    auto inv_w_i32 = hn::ConvertTo(di, hn::Mul(inv_w, scale));
    hn::Store(inv_w_i32, di, out_inv_w.x);
    #endif

    #else

    // float storing
    hn::Store(ndc_x, df, out_block.x);
    hn::Store(ndc_y, df, out_block.y);
    hn::Store(ndc_z, df, out_block.z);
    hn::Store(clip_w, df, out_clip_w.x);

    #ifndef RRL_SWR_AFFINE_INTERPOLATION
    hn::Store(inv_w, df, out_inv_w.x);
    #endif
    #endif
}



} // namespace HWY_NAMESPACE



// Expose the internal implementation to the generic interface
#ifndef RRL_SWR_AFFINE_INTERPOLATION
void ProjectVertexBlock(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_inv_w, swr_vec1& out_clip_w) {
    HWY_NAMESPACE::ProjectVertexBlockImpl(pos_block, mvp_mat, out_block, out_inv_w, out_clip_w);
}
#else
void ProjectVertexBlock(const swr_vec3& pos_block, const glm::mat4& mvp_mat, swr_vec3& out_block, swr_vec1& out_clip_w) {
    HWY_NAMESPACE::ProjectVertexBlockImpl(pos_block, mvp_mat, out_block, out_clip_w);
}
#endif



} // namespace rrl::rhi::software::math