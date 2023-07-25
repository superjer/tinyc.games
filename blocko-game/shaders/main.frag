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
uniform float fog_lo;
uniform float fog_hi;
uniform vec3 light_pos;
uniform vec3 view_pos;
uniform float sharpness;
uniform bool shadow_mapping;

float rand(vec2 co)
{
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453) * 0.0004 - 0.0002;
}

void main(void)
{
    float fog = smoothstep(fog_lo, fog_hi, length(view_pos.xz - world_pos.xz));
    float il = illum + 0.1 * smoothstep(1000, 0, eyedist);

    vec3 sky;
    if (shadow_mapping)
    {
        vec3 light_dir = normalize(light_pos - world_pos.xyz);
        float diff = max(dot(light_dir, normal), 0.0);
        vec3 view_dir = normalize(view_pos - world_pos.xyz);
        vec3 halfway_dir = normalize(light_dir + view_dir);
        float spec = pow(max(dot(normal, halfway_dir), 0), 16);
        vec2 sp0 = shadow_pos.xy + vec2(-0.0005, -0.0005);
        vec2 sp1 = shadow_pos.xy + vec2(-0.0005,  0.0000);
        vec2 sp2 = shadow_pos.xy + vec2(-0.0005, +0.0005);
        vec2 sp3 = shadow_pos.xy + vec2( 0.0000, -0.0005);
        vec2 sp4 = shadow_pos.xy + vec2( 0.0000, +0.0005);
        vec2 sp5 = shadow_pos.xy + vec2(+0.0005, -0.0005);
        vec2 sp6 = shadow_pos.xy + vec2(+0.0005,  0.0000);
        vec2 sp7 = shadow_pos.xy + vec2(+0.0005, +0.0005);
        float unshadow = textureProj(shadow_map, vec4(shadow_pos.xyz, 1))
                       + textureProj(shadow_map, vec4(sp0.x + rand(gl_FragCoord.xy), sp0.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp1.x + rand(gl_FragCoord.xy), sp1.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp2.x + rand(gl_FragCoord.xy), sp2.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp3.x + rand(gl_FragCoord.xy), sp3.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp4.x + rand(gl_FragCoord.xy), sp4.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp5.x + rand(gl_FragCoord.xy), sp5.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp6.x + rand(gl_FragCoord.xy), sp6.y + rand(gl_FragCoord.xy), shadow_pos.z, 1))
                       + textureProj(shadow_map, vec4(sp7.x + rand(gl_FragCoord.xy), sp7.y + rand(gl_FragCoord.xy), shadow_pos.z, 1));
        unshadow /= 9.f;
        float s0 = 0.6 + 0.4 * sharpness;
        float s1 = 0.3 + 0.7 * (1-sharpness);
        sky = vec3(s1 * il + s0 * unshadow * (diff + spec)) * day_color;
    }
    else
    {
        sky = vec3(il) * day_color;
    }

    vec3 glo = vec3(glow * glo_color);
    vec3 unsky = vec3(1 - sky.r, 1 - sky.g, 1 - sky.b);
    vec4 combined = vec4(sky + glo * unsky, alpha);
    vec4 c = texture(tarray, vec3(uv, tex)) * combined;
    if (c.a < 0.01) discard;
    color = mix(c, vec4(fog_color, 1), fog);
}
