struct Camera {
    mvp: mat4x4<f32>,
};

@group(0) @binding(0) var<uniform> camera: Camera;
@group(0) @binding(1) var atlas_tex: texture_2d<f32>;
@group(0) @binding(2) var atlas_samp: sampler;

struct VertexInput {
    @location(0) quad_pos: vec2<f32>,
    @location(1) uv_min: vec2<f32>,
    @location(2) uv_max: vec2<f32>,
    @location(3) sprite_pos: vec2<f32>,
    @location(4) sprite_size: vec2<f32>,
    @location(5) depth: f32,
    @location(6) tint: vec4<f32>,
};

struct VertexOutput {
    @builtin(position) clip_pos: vec4<f32>,
    @location(0) uv: vec2<f32>,
    @location(1) color: vec4<f32>,
};

@vertex
fn vs_main(in: VertexInput) -> VertexOutput {
    var out: VertexOutput;
    let world_pos = in.sprite_pos + in.quad_pos * in.sprite_size;
    out.clip_pos = camera.mvp * vec4<f32>(world_pos, in.depth, 1.0);
    out.uv = vec2<f32>(
        mix(in.uv_min.x, in.uv_max.x, in.quad_pos.x),
        mix(in.uv_max.y, in.uv_min.y, in.quad_pos.y)
    );
    out.color = in.tint;
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    let tex_color = textureSample(atlas_tex, atlas_samp, in.uv);
    let final_color = tex_color * in.color;
    if (final_color.a < 0.01) {
        discard;
    }
    return final_color;
}
