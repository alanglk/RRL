// src/rhi/opengl/shaders/sys_ui2d.frag
// System Shader

#version 450 core

// Inputs from Vertex Shader
in vec2 v_UV;


// MRT Layout matching RHIRenderAttachmentSemantic
layout(location = 0) out vec4 OutColor;         // COLOR                    (Semantic 0 - UINT8 CH_4)
layout(location = 1) out float OutDepth;        // DEPTH                    (Semantic 1 - FLOAT32 CH_1)
layout(location = 2) out vec3 OutNormal;        // NORMAL_SURFACE           (Semantic 2 - FLOAT32 CH_3)
layout(location = 3) out uint OutPanoptic;      // PANOPTIC_SEGMENTATION    (Semantic 3) (NOT IMPLEMENTED YET)


// Uniform texture
layout(binding = 0) uniform sampler2D u_UITexture;


void main() {
    vec4 tex_color = texture(u_UITexture, v_UV);
    
    // Discard completely transparent pixels to save fill-rate
    if(tex_color.a < 0.01) {
        discard;
    }
    OutColor = tex_color;

    
    // UI doesn't have depth, normals or panoptic IDs
    OutDepth    = 10000.0; 
    OutNormal   = vec3(0.0);
    OutPanoptic = 0u;
}