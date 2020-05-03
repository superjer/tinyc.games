#version 330 core
out vec4 color;

flat in float tex;
in float illum;
in float glow;
flat in float alpha;
in vec2 uv;
flat in float eyedist;
in vec4 shadow_pos;
in vec4 world_pos;
flat in vec3 normal;

uniform sampler2DArray tarray;
uniform sampler2DShadow shadow_map;
uniform vec3 day_color;
uniform vec3 glo_color;
uniform vec3 fog_color;
uniform vec3 light_pos;
uniform vec3 view_pos;
uniform float sharpness;

void main(void)
{
    float fog = smoothstep(10000, 100000, eyedist);
    float il = illum + 0.1 * smoothstep(1000, 0, eyedist);

    vec3 light_dir = normalize(light_pos - world_pos.xyz);
    float diff = max(dot(light_dir, normal), 0.0);
    vec3 view_dir = normalize(view_pos - world_pos.xyz);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0), 16);

    vec4 s = vec4(shadow_pos.xyz, 1);
    float unshadow = textureProj(shadow_map, s);

    float s0 = 0.6 + 0.4 * sharpness;
    float s1 = 0.3 + 0.7 * (1-sharpness);
    vec3 sky = vec3(s1 * il + s0 * unshadow * (diff + spec)) * day_color;

    vec3 glo = vec3(glow * glo_color);
    vec3 unsky = vec3(1 - sky.r, 1 - sky.g, 1 - sky.b);
    vec4 combined = vec4(sky + glo * unsky, alpha);
    vec4 c = texture(tarray, vec3(uv, tex)) * combined;
    if (c.a < 0.01) discard;
    color = mix(c, vec4(fog_color, 1), fog);
}
