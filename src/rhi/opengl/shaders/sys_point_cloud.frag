// src/rhi/opengl/shaders/sys_point_cloud.frag
// System Shader

#version 450 core

// Inputs from Vertex Shader
in vec4 v_Color;
in vec3 v_ViewPos;


// MRT Layout matching RHIRenderAttachmentSemantic
layout(location = 0) out vec4 OutColor;         // COLOR                    (Semantic 0 - UINT8 CH_4)
layout(location = 1) out float OutDepth;        // DEPTH                    (Semantic 1 - FLOAT32 CH_1)
layout(location = 2) out vec3 OutNormal;        // NORMAL_SURFACE           (Semantic 2 - FLOAT32 CH_3)
layout(location = 3) out uint OutPanoptic;      // PANOPTIC_SEGMENTATION    (Semantic 3) (NOT IMPLEMENTED YET)


void main() {
    OutColor    = vec4(0.0, 1.0, 0.0, 1.0); // v_Color;      // Point cloud color
    OutDepth    = -v_ViewPos.z; // Metric distance to LiDAR point
    OutNormal   = vec3(0.0);    // Points don't have surface normals
    OutPanoptic = 0u;           // NOT IMPLEMENTED YET
}