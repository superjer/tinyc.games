#version 450

layout(points) in;
layout(triangle_strip, max_vertices = 3) out;

layout(location=0) in vec3 vColor[];
layout(location=0) out vec3 fragColor;

void main() {
  vec4 pos = gl_in[0].gl_Position;

  fragColor = vec3(vColor[0].r, vColor[0].g+.5, vColor[0].b-.5);
  gl_Position = pos + vec4(-0.1, -0.1, 0.0, 0.0);
  EmitVertex();

  fragColor = vec3(vColor[0].r+.5, vColor[0].g-.5, vColor[0].b);
  gl_Position = pos + vec4(0.1, -0.1, 0.0, 0.0);
  EmitVertex();

  fragColor = vec3(vColor[0].r-.5, vColor[0].g, vColor[0].b+.5);
  gl_Position = pos + vec4(0.0, 0.1, 0.0, 0.0);
  EmitVertex();

  EndPrimitive();  // Finish this primitive
}
