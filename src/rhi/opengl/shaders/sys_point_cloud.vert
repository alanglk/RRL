// src/rhi/opengl/shaders/sys_point_cloud.vert
// System Shader

#version 450 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;   // Ignored
layout(location = 2) in vec2 aUV;       // Ignored
layout(location = 3) in vec4 aColor;

// Outputs to Fragment Shader
out vec4 v_Color;
out vec3 v_ViewPos;

// Uniforms
uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

void main() {
    v_Color = aColor;
    
    vec4 viewPos = u_View * u_Model * vec4(aPos, 1.0);
    v_ViewPos = viewPos.xyz;
    
    gl_Position = u_Projection * viewPos;
    gl_PointSize = 3.0;
}