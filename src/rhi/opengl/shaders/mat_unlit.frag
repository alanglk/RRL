// src/rhi/opengl/shaders/mat_unlit.frag
// Material Shader

#version 450 core

// Inputs from Vertex Shader
in vec2 v_UV;
in vec4 v_Color;
in vec3 v_Normal;
in vec3 v_ViewPos;

// MRT Layout matching RHIRenderAttachmentSemantic
layout(location = 0) out vec4 OutColor;         // COLOR                    (Semantic 0 - UINT8 CH_4)
layout(location = 1) out float OutDepth;        // DEPTH                    (Semantic 1 - FLOAT32 CH_1)
layout(location = 2) out vec3 OutNormal;        // NORMAL_SURFACE           (Semantic 2 - FLOAT32 CH_3)
layout(location = 3) out uint OutPanoptic;      // PANOPTIC_SEGMENTATION    (Semantic 3) (NOT IMPLEMENTED YET)


// UBO mapping directly to C++ MaterialUBOData struct
layout(std140, binding = 0) uniform MaterialBlock {
    vec4 u_BaseColor;
    vec3 u_Emission;
    float u_Roughness;
    float u_Metallic;
};

// Texture mapping (Texture Unit 0)
layout(binding = 0) uniform sampler2D u_AlbedoMap;
uniform int u_HasAlbedo;

void main() {
    // Combine Material Base Color with Geometry Vertex Color
    vec4 final_color = u_BaseColor * v_Color;

    // Apply Albedo Texture if one was bound
    if (u_HasAlbedo == 1) {
        final_color *= texture(u_AlbedoMap, v_UV);
    }

    // Add Emission
    final_color.rgb += u_Emission;
    OutColor = final_color;
    
    // Metric distance from the camera plane in meters
    OutDepth = abs(v_ViewPos.z);

    // Surface Normals in range [-1, 1]
    OutNormal = normalize(v_Normal); 

    // Segmentation (NOT IMPLEMENTED YET)
    OutPanoptic = 0u;
}