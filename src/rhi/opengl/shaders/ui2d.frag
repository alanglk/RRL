#version 450 core

in vec2 v_UV;
out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_UITexture;

void main() {
    vec4 tex_color = texture(u_UITexture, v_UV);
    
    // Discard completely transparent pixels to save fill-rate
    if(tex_color.a < 0.01) {
        discard;
    }
    
    FragColor = tex_color;
}