// WGSL Triangle Shader
// Renders a hardcoded colored triangle without vertex buffers

struct VertexOutput {
    @builtin(position) position: vec4<f32>,
    @location(0) color: vec4<f32>,
};

@vertex
fn vs_main(@builtin(vertex_index) vertex_index: u32) -> VertexOutput {
    var out: VertexOutput;
    
    // Triangle vertices (NDC space: -1 to 1)
    let pos = array<vec2<f32>, 3>(
        vec2<f32>(0.0, 0.5),   // top
        vec2<f32>(-0.5, -0.5), // bottom left
        vec2<f32>(0.5, -0.5)   // bottom right
    );
    
    // Vertex colors
    let colors = array<vec4<f32>, 3>(
        vec4<f32>(1.0, 0.0, 0.0, 1.0), // red (top)
        vec4<f32>(0.0, 1.0, 0.0, 1.0), // green (bottom left)
        vec4<f32>(0.0, 0.0, 1.0, 1.0)  // blue (bottom right)
    );
    
    out.position = vec4<f32>(pos[vertex_index], 0.0, 1.0);
    out.color = colors[vertex_index];
    
    return out;
}

@fragment
fn fs_main(in: VertexOutput) -> @location(0) vec4<f32> {
    return in.color;
}
