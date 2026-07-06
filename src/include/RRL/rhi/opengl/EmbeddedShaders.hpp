// src/include/rhi/opengl/EmbeddedShaders.hpp
#pragma once



namespace rrl::rhi::opengl::shaders {


// --- UNLIT Shader-------------------------------------------------
static constexpr const char* unlit_vert = R"(
#version 450 core

// VBO Inputs mapping to CreateMesh locations
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // Ignored
layout(location = 2) in vec2 aUV;       // Ignored
layout(location = 3) in vec4 aColor;

// Outputs to Fragment Shader
out vec2 v_UV;
out vec4 v_Color;

uniform mat4 u_MVP;

void main() {
    v_UV = aUV;
    v_Color = aColor;
    
    gl_Position = u_MVP * vec4(aPos, 1.0);
}
)";
static constexpr const char* unlit_frag = R"(
#version 450 core

// Inputs from Vertex Shader
in vec2 v_UV;
in vec4 v_Color;

out vec4 FragColor;

// UBO mapping directly to your C++ MaterialUBOData struct
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
    // 1. Combine Material Base Color with Geometry Vertex Color
    vec4 final_color = u_BaseColor * v_Color;

    // 2. Apply Albedo Texture if one was bound
    if (u_HasAlbedo == 1) {
        final_color *= texture(u_AlbedoMap, v_UV);
    }

    // 3. Add Emission
    final_color.rgb += u_Emission;

    FragColor = final_color;
}
)";


// --- POINT_CLOUD Shader-------------------------------------------
// Under development (see src/rhi/opengl/shaders folder)
static constexpr const char* point_cloud_vert = R"(
)";
static constexpr const char* point_cloud_frag = R"(
)";


// --- 2D_UI Shader-------------------------------------------------
// Under development (see src/rhi/opengl/shaders folder)
static constexpr const char* ui2d_vert = R"(
)";
static constexpr const char* ui2d_frag = R"(
)";


} // namespace rrl::rhi::opengl::shaders