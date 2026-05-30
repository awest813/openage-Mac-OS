#version 330

in vec2 tex_pos;
in vec3 world_pos;

layout(location=0) out vec4 out_col;

uniform sampler2D tex;
uniform sampler2D fog_tex;
uniform vec2 fog_map_size;
uniform bool fog_enabled;

void main()
{
    vec4 tex_val = texture(tex, tex_pos);

    if (not fog_enabled) {
        out_col = tex_val;
        return;
    }

    // World space uses se -> x and -ne -> z (see coord::scene3::to_world_space).
    vec2 tile = vec2(floor(world_pos.x + 0.5), floor(-world_pos.z + 0.5));
    vec2 fog_uv = (tile + vec2(0.5)) / fog_map_size;
    float fog = texture(fog_tex, fog_uv).r;

    if (fog < 0.05) {
        // Unexplored: black shroud.
        out_col = vec4(0.0, 0.0, 0.0, 1.0);
    } else if (fog < 0.75) {
        // Explored but not visible: dimmed terrain.
        out_col = tex_val * vec4(0.45, 0.45, 0.45, 1.0);
    } else {
        out_col = tex_val;
    }
}
