#version 450 core

uniform float u_X;
uniform float u_Y;
uniform float u_W;
uniform float u_H;

out vec2 v_UV;

void main() {
    // Generate a perfect unit quad (0.0 to 1.0) using only the gl_VertexID
    vec2 pos[6] = vec2[6](
        vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(0.0, 1.0), // Triangle 1
        vec2(1.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)  // Triangle 2
    );
    
    vec2 p = pos[gl_VertexID];
    
    // UVs map exactly to the quad corners
    v_UV = p; 

    // Convert Normalized Screen Space (0 to 1, Y-Down) into OpenGL NDC Space (-1 to 1, Y-Up)
    float ndc_x = (u_X + p.x * u_W) * 2.0 - 1.0;
    float ndc_y = 1.0 - (u_Y + p.y * u_H) * 2.0; 
    
    gl_Position = vec4(ndc_x, ndc_y, 0.0, 1.0);
}