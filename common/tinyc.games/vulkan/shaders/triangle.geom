#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in vec3 vColor[];
layout(location=0) out vec3 fragColor;

void main() {
  vec4 pos = gl_in[0].gl_Position;  // Get input point position

  fragColor = vColor[0];

  // Emit 3 vertices forming a small triangle
  gl_Position = pos + vec4(-0.1, -0.1, 0.0, 0.0);
  EmitVertex();

  gl_Position = pos + vec4(0.1, -0.1, 0.0, 0.0);
  EmitVertex();

  gl_Position = pos + vec4(0.0, 0.1, 0.0, 0.0);
  EmitVertex();

  EndPrimitive();  // Finish this primitive
}
