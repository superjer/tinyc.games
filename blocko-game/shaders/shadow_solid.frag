#version 450

// Depth-only fragment stage with no discard, no outputs, and no texture
// fetch, so the GPU keeps its fast depth path (early-Z + full-rate depth
// writes). Used for cascades that treat leaves as solid (far/extreme);
// near/mid use shadow.frag to alpha-test leaves instead.

void main(void) {}
