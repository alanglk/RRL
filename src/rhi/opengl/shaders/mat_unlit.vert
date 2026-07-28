// src/rhi/opengl/shaders/mat_unlit.vert
// Material Shader

#version 450 core

// VBO Inputs mapping to CreateMesh locations
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // Ignored
layout(location = 2) in vec2 aUV;       // Ignored
layout(location = 3) in vec4 aColor;

// Outputs to Fragment Shader
out vec2 v_UV;
out vec4 v_Color;
out vec3 v_Normal;
out vec3 v_ViewPos;

// Binded uniforms
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;


void main() {
    v_UV = aUV;
    v_Color = aColor;
    
    // Normal Matrix (inverse transpose of the 3x3 model matrix to handle non-uniform scaling)
    mat3 normalMatrix = transpose(inverse(mat3(u_Model)));
    v_Normal = normalMatrix * aNormal;
    
    // View Space Position
    vec4 viewPos = u_View * u_Model * vec4(aPos, 1.0);
    v_ViewPos = viewPos.xyz;
    
    // Final Screen Space Position
    gl_Position = u_Projection * viewPos;
}