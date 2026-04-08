#include <string.h>

#include "render/webgpu.h"

#pragma push_macro("internal")
#undef internal
#include <SDL3/SDL.h>
#pragma pop_macro("internal")

#if defined(SDL_PLATFORM_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#if OS_EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <webgpu/webgpu.h>
#else
#include "webgpu/webgpu.h"
#include "webgpu/wgpu.h"
#endif

#include "os/os_mod.h"

struct WebGPUState {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;

    WGPUShaderModule shader_module;
    WGPUPipelineLayout pipeline_layout;
    WGPURenderPipeline render_pipeline;
    WGPUBindGroupLayout bind_group_layout;
    WGPUBindGroup bind_group;

    WGPUBuffer vertex_buffer;
    WGPUBuffer index_buffer;
    WGPUBuffer uniform_buffer;
    WGPUTexture atlas_texture;
    WGPUTextureView atlas_view;
    WGPUSampler atlas_sampler;

    WGPUBuffer instance_buffer;
    u32 instance_buffer_capacity;

    WGPUTexture depth_texture;
    WGPUTextureView depth_view;

    WGPUTexture current_texture;
    WGPUTextureView current_view;
    WGPUSurfaceTexture surface_texture;
    u32 surface_width;
    u32 surface_height;
    bool configured;

    WGPUTextureFormat surface_format;
};

struct CameraUniform {
    f32 mvp[16];
};
static_assert(sizeof(CameraUniform) == 64, "Camera uniform must be 64 bytes");

static WGPUInstance get_instance(void) {
    WGPUInstanceDescriptor desc = {};
    return wgpuCreateInstance(&desc);
}

#if !OS_EMSCRIPTEN
static void wgpu_log_callback(
    WGPULogLevel level,
    WGPUStringView message,
    void* userdata
) {
    (void)userdata;
    const char* level_str = "UNKNOWN";
    switch(level) {
        case WGPULogLevel_Error:
            level_str = "ERROR";
            break;
        case WGPULogLevel_Warn:
            level_str = "WARN";
            break;
        case WGPULogLevel_Info:
            level_str = "INFO";
            break;
        case WGPULogLevel_Debug:
            level_str = "DEBUG";
            break;
        case WGPULogLevel_Trace:
            level_str = "TRACE";
            break;
        default:
            break;
    }
    LOG_INFO("[WebGPU %s] %.*s", level_str, (int)message.length, message.data);
}
#endif

static void wgpu_error_callback(
    WGPUDevice const* device,
    WGPUErrorType type,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
    (void)device;
    (void)userdata1;
    (void)userdata2;

    char const* type_str = "UNKNOWN";
    switch(type) {
        case WGPUErrorType_Validation:
            type_str = "VALIDATION";
            break;
        case WGPUErrorType_OutOfMemory:
            type_str = "OUT_OF_MEMORY";
            break;
        case WGPUErrorType_Internal:
            type_str = "INTERNAL";
            break;
        case WGPUErrorType_Unknown:
            type_str = "UNKNOWN";
            break;
        default:
            break;
    }
    LOG_ERROR("[WebGPU %s] %.*s", type_str, (int)message.length, message.data);
}

static void wgpu_device_lost_callback(
    WGPUDevice const* device,
    WGPUDeviceLostReason reason,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
    (void)device;
    (void)reason;
    (void)userdata1;
    (void)userdata2;
    LOG_ERROR("[WebGPU DEVICE LOST] %.*s", (int)message.length, message.data);
}

static WGPUSurface create_surface(WGPUInstance instance, SDL_Window* window) {
#if defined(SDL_PLATFORM_WIN32)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    WGPUSurfaceSourceWindowsHWND from_hwnd = {};
    from_hwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    from_hwnd.hwnd = SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_WIN32_HWND_POINTER,
        nullptr
    );
    from_hwnd.hinstance = GetModuleHandle(nullptr);

    WGPUSurfaceDescriptor desc = {};
    desc.nextInChain = &from_hwnd.chain;
    return wgpuInstanceCreateSurface(instance, &desc);
#elif defined(SDL_PLATFORM_LINUX)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    void* wayland_display = SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_WAYLAND_DISPLAY_POINTER,
        nullptr
    );
    void* wayland_surface = SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER,
        nullptr
    );

    if(wayland_display && wayland_surface) {
        WGPUSurfaceSourceWaylandSurface from_wayland = {};
        from_wayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
        from_wayland.display = wayland_display;
        from_wayland.surface = wayland_surface;

        WGPUSurfaceDescriptor desc = {};
        desc.nextInChain = &from_wayland.chain;
        return wgpuInstanceCreateSurface(instance, &desc);
    }

    void* x11_display = SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_X11_DISPLAY_POINTER,
        nullptr
    );
    uint64_t x11_window = (uint64_t)
        SDL_GetNumberProperty(props, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);

    if(x11_display) {
        WGPUSurfaceSourceXlibWindow from_xlib = {};
        from_xlib.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
        from_xlib.display = x11_display;
        from_xlib.window = x11_window;

        WGPUSurfaceDescriptor desc = {};
        desc.nextInChain = &from_xlib.chain;
        return wgpuInstanceCreateSurface(instance, &desc);
    }
#elif defined(SDL_PLATFORM_MACOS)
    SDL_PropertiesID props = SDL_GetWindowProperties(window);
    WGPUSurfaceSourceMetalLayer from_metal = {};
    from_metal.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
    from_metal.layer = SDL_GetPointerProperty(
        props,
        SDL_PROP_WINDOW_COCOA_WINDOW_POINTER,
        nullptr
    );

    if(from_metal.layer) {
        WGPUSurfaceDescriptor desc = {};
        desc.nextInChain = &from_metal.chain;
        return wgpuInstanceCreateSurface(instance, &desc);
    }
#elif OS_EMSCRIPTEN
    (void)window;
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector from_canvas =
        WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    from_canvas.selector = {"#canvas", WGPU_STRLEN};

    WGPUSurfaceDescriptor desc = WGPU_SURFACE_DESCRIPTOR_INIT;
    desc.nextInChain = &from_canvas.chain;
    WGPUSurface surface = wgpuInstanceCreateSurface(instance, &desc);
    if(!surface) {
        LOG_ERROR("Failed to create Emscripten surface for canvas '#canvas'");
    }
    return surface;
#endif

    LOG_FATAL("Failed to create WebGPU surface: unsupported platform");
    return nullptr;
}

static void wgpu_request_adapter_callback(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
    (void)userdata2;
    WGPUAdapter* out = (WGPUAdapter*)userdata1;
    if(status == WGPURequestAdapterStatus_Success) {
        *out = adapter;
    } else {
        *out = nullptr;
        LOG_ERROR(
            "Failed to request adapter: %.*s",
            (int)message.length,
            message.data
        );
    }
}

static void wgpu_request_device_callback(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
    (void)userdata2;
    WGPUDevice* out = (WGPUDevice*)userdata1;
    if(status == WGPURequestDeviceStatus_Success) {
        *out = device;
    } else {
        *out = nullptr;
        LOG_ERROR(
            "Failed to request device: %.*s",
            (int)message.length,
            message.data
        );
    }
}

static WGPUBuffer create_buffer(
    WGPUDevice device,
    u64 size,
    WGPUBufferUsage usage
) {
    WGPUBufferDescriptor desc = {};
    desc.usage = usage;
    desc.size = size;
    desc.mappedAtCreation = false;
    return wgpuDeviceCreateBuffer(device, &desc);
}

static void create_depth_texture(WebGPUState* state) {
    if(state->depth_view) {
        wgpuTextureViewRelease(state->depth_view);
        state->depth_view = nullptr;
    }
    if(state->depth_texture) {
        wgpuTextureRelease(state->depth_texture);
        state->depth_texture = nullptr;
    }

    if(state->surface_width == 0 || state->surface_height == 0) {
        return;
    }

    WGPUTextureDescriptor depth_desc = {};
    depth_desc.usage = WGPUTextureUsage_RenderAttachment;
    depth_desc.dimension = WGPUTextureDimension_2D;
    depth_desc.size = {state->surface_width, state->surface_height, 1};
    depth_desc.format = WGPUTextureFormat_Depth24Plus;
    depth_desc.mipLevelCount = 1;
    depth_desc.sampleCount = 1;
    state->depth_texture = wgpuDeviceCreateTexture(state->device, &depth_desc);
    state->depth_view = wgpuTextureCreateView(state->depth_texture, nullptr);
}

static u32 next_pow2_u32(u32 value) {
    u32 result = 1;
    while(result < value) {
        result <<= 1;
    }
    return result;
}

static void ensure_instance_buffer(WebGPUState* state, u32 required) {
    if(required <= state->instance_buffer_capacity) {
        return;
    }

    u32 new_capacity = next_pow2_u32(required);
    if(state->instance_buffer) {
        wgpuBufferRelease(state->instance_buffer);
    }

    state->instance_buffer = create_buffer(
        state->device,
        (u64)sizeof(SpriteInstance) * (u64)new_capacity,
        WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    );
    state->instance_buffer_capacity = new_capacity;
}

static void write_world_camera_uniform(
    WebGPUState* state,
    WorldCamera const* camera,
    u32 surface_width,
    u32 surface_height
) {
    f32 hw = (f32)surface_width * 0.5f;
    f32 hh = (f32)surface_height * 0.5f;
    f32 z = camera->zoom;
    f32 l = camera->position.x - hw / z;
    f32 r = camera->position.x + hw / z;
    f32 b = camera->position.y - hh / z;
    f32 t = camera->position.y + hh / z;

    CameraUniform uniform = {};
    uniform.mvp[0] = 2.0f / (r - l);
    uniform.mvp[5] = 2.0f / (t - b);
    uniform.mvp[10] = 1.0f;
    uniform.mvp[12] = -(r + l) / (r - l);
    uniform.mvp[13] = -(t + b) / (t - b);
    uniform.mvp[15] = 1.0f;

    wgpuQueueWriteBuffer(
        state->queue,
        state->uniform_buffer,
        0,
        &uniform,
        sizeof(uniform)
    );
}

static void upload_world_sprites(
    WebGPUState* state,
    WorldSpritePass const* sprite_pass
) {
    if(sprite_pass->sprite_count == 0) {
        return;
    }

    ensure_instance_buffer(state, sprite_pass->sprite_count);
    wgpuQueueWriteBuffer(
        state->queue,
        state->instance_buffer,
        0,
        sprite_pass->sprites,
        (size_t)((u64)sizeof(SpriteInstance) * (u64)sprite_pass->sprite_count)
    );
}

static void render_backgrounds(
    WebGPUState* state,
    WGPURenderPassEncoder pass,
    BackgroundPass const* backgrounds
) {
    (void)state;
    (void)pass;
    (void)backgrounds;
}

static void render_world_sprites(
    WebGPUState* state,
    WGPURenderPassEncoder pass,
    WorldSpritePass const* sprite_pass
) {
    if(sprite_pass->sprite_count == 0) {
        return;
    }

    write_world_camera_uniform(
        state,
        &sprite_pass->camera,
        state->surface_width,
        state->surface_height
    );
    upload_world_sprites(state, sprite_pass);

    wgpuRenderPassEncoderSetPipeline(pass, state->render_pipeline);
    wgpuRenderPassEncoderSetBindGroup(pass, 0, state->bind_group, 0, nullptr);
    wgpuRenderPassEncoderSetVertexBuffer(
        pass,
        0,
        state->vertex_buffer,
        0,
        sizeof(f32) * 8
    );
    wgpuRenderPassEncoderSetVertexBuffer(
        pass,
        1,
        state->instance_buffer,
        0,
        (u64)sizeof(SpriteInstance) * (u64)sprite_pass->sprite_count
    );
    wgpuRenderPassEncoderSetIndexBuffer(
        pass,
        state->index_buffer,
        WGPUIndexFormat_Uint16,
        0,
        sizeof(u16) * 6
    );
    wgpuRenderPassEncoderDrawIndexed(
        pass,
        6,
        sprite_pass->sprite_count,
        0,
        0,
        0
    );
}

static void render_text(
    WebGPUState* state,
    WGPURenderPassEncoder pass,
    TextPass const* text
) {
    (void)state;
    (void)pass;
    (void)text;
}

static void render_ui(
    WebGPUState* state,
    WGPURenderPassEncoder pass,
    UiPass const* ui
) {
    (void)state;
    (void)pass;
    (void)ui;
}

static void render_debug(
    WebGPUState* state,
    WGPURenderPassEncoder pass,
    DebugPass const* debug
) {
    (void)state;
    (void)pass;
    (void)debug;
}

static void configure_surface(WebGPUState* state) {
    if(!state->surface || !state->device) {
        return;
    }

#if OS_EMSCRIPTEN
    wgpuSurfaceUnconfigure(state->surface);
#endif

    WGPUSurfaceConfiguration config = {};
    config.device = state->device;
    config.format = state->surface_format;
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = state->surface_width;
    config.height = state->surface_height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;

    wgpuSurfaceConfigure(state->surface, &config);
    create_depth_texture(state);
    state->configured = true;
}

WebGPURenderer init_webgpu(SDL_Window* window, Arena* arena, Atlas* atlas) {
    WebGPURenderer result = {};

    ASSERT(window != nullptr, "Window must not be null!");
    ASSERT(arena != nullptr, "Arena must not be null!");
    ASSERT(atlas != nullptr, "Atlas must not be null!");
    ASSERT(atlas->image_path != nullptr, "Atlas image path must not be null!");

    WebGPUState* state = push_struct(arena, WebGPUState);

    state->instance = get_instance();
    if(!state->instance) {
        LOG_FATAL("Failed to create WebGPU instance");
        return result;
    }

#if !OS_EMSCRIPTEN
    wgpuSetLogCallback(wgpu_log_callback, nullptr);
    wgpuSetLogLevel(WGPULogLevel_Warn);
#endif

    state->surface = create_surface(state->instance, window);
    if(!state->surface) {
        LOG_FATAL("Failed to create WebGPU surface");
        return result;
    }

    WGPURequestAdapterOptions adapter_opts = {};
#if !OS_EMSCRIPTEN
    adapter_opts.compatibleSurface = state->surface;
#endif
    adapter_opts.powerPreference = WGPUPowerPreference_HighPerformance;

    WGPURequestAdapterCallbackInfo adapter_cb = {};
#if OS_EMSCRIPTEN
    adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    adapter_cb.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    adapter_cb.callback = wgpu_request_adapter_callback;
    adapter_cb.userdata1 = &state->adapter;

    wgpuInstanceRequestAdapter(state->instance, &adapter_opts, adapter_cb);

#if OS_EMSCRIPTEN
    while(!state->adapter) {
        emscripten_sleep(0);
    }
#else
    wgpuInstanceProcessEvents(state->instance);
#endif

    if(!state->adapter) {
        LOG_FATAL("Failed to get WebGPU adapter");
        return result;
    }

    WGPUDeviceDescriptor device_desc = {};
    device_desc.deviceLostCallbackInfo.mode = WGPUCallbackMode_AllowSpontaneous;
    device_desc.deviceLostCallbackInfo.callback = wgpu_device_lost_callback;
    device_desc.uncapturedErrorCallbackInfo.callback = wgpu_error_callback;

    WGPURequestDeviceCallbackInfo device_cb = {};
#if OS_EMSCRIPTEN
    device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    device_cb.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    device_cb.callback = wgpu_request_device_callback;
    device_cb.userdata1 = &state->device;

    wgpuAdapterRequestDevice(state->adapter, &device_desc, device_cb);

#if OS_EMSCRIPTEN
    while(!state->device) {
        emscripten_sleep(0);
    }
#else
    wgpuInstanceProcessEvents(state->instance);
#endif

    if(!state->device) {
        LOG_FATAL("Failed to get WebGPU device");
        return result;
    }

    state->queue = wgpuDeviceGetQueue(state->device);

    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window, &w, &h);
    state->surface_width = (u32)w;
    state->surface_height = (u32)h;
#if OS_EMSCRIPTEN
    state->surface_format = WGPUTextureFormat_RGBA8Unorm;
#else
    state->surface_format = WGPUTextureFormat_BGRA8Unorm;
#endif

    FileData shader_source = os_read_file(arena, "assets/shaders/sprite.wgsl");
    if(shader_source.data == nullptr || shader_source.size == 0) {
        LOG_FATAL("Failed to load sprite shader");
        return result;
    }

    WGPUShaderSourceWGSL wgsl_source = {};
    wgsl_source.chain.sType = WGPUSType_ShaderSourceWGSL;
    wgsl_source.code.data = (char const*)shader_source.data;
    wgsl_source.code.length = (size_t)shader_source.size;

    WGPUShaderModuleDescriptor shader_desc = {};
    shader_desc.nextInChain = &wgsl_source.chain;
    state->shader_module =
        wgpuDeviceCreateShaderModule(state->device, &shader_desc);

    Temp atlas_upload_temp = temp_begin(arena);
    AtlasImage atlas_image = atlas_load_image(arena, atlas->image_path);
    if(atlas_image.pixels == nullptr || atlas_image.width == 0 ||
       atlas_image.height == 0) {
        temp_end(atlas_upload_temp);
        LOG_FATAL("Failed to load atlas image for GPU upload");
        return result;
    }

    if(atlas->atlas_width != atlas_image.width ||
       atlas->atlas_height != atlas_image.height) {
        LOG_WARN(
            "Atlas JSON size %ux%u does not match image size %ux%u; using "
            "image size",
            atlas->atlas_width,
            atlas->atlas_height,
            atlas_image.width,
            atlas_image.height
        );
        atlas->atlas_width = atlas_image.width;
        atlas->atlas_height = atlas_image.height;
    }

    WGPUTextureDescriptor atlas_desc = {};
    atlas_desc.usage =
        WGPUTextureUsage_TextureBinding | WGPUTextureUsage_CopyDst;
    atlas_desc.dimension = WGPUTextureDimension_2D;
    atlas_desc.size = {atlas->atlas_width, atlas->atlas_height, 1};
    atlas_desc.format = WGPUTextureFormat_RGBA8Unorm;
    atlas_desc.mipLevelCount = 1;
    atlas_desc.sampleCount = 1;
    state->atlas_texture = wgpuDeviceCreateTexture(state->device, &atlas_desc);

    WGPUTexelCopyTextureInfo atlas_dst = {};
    atlas_dst.texture = state->atlas_texture;
    atlas_dst.mipLevel = 0;
    atlas_dst.origin = {0, 0, 0};
    atlas_dst.aspect = WGPUTextureAspect_All;

    WGPUTexelCopyBufferLayout atlas_layout = {};
    atlas_layout.offset = 0;
    atlas_layout.bytesPerRow = atlas->atlas_width * 4;
    atlas_layout.rowsPerImage = atlas->atlas_height;

    WGPUExtent3D atlas_size = {atlas->atlas_width, atlas->atlas_height, 1};
    wgpuQueueWriteTexture(
        state->queue,
        &atlas_dst,
        atlas_image.pixels,
        (size_t)((u64)atlas->atlas_width * (u64)atlas->atlas_height * 4ULL),
        &atlas_layout,
        &atlas_size
    );
    temp_end(atlas_upload_temp);

    state->atlas_view = wgpuTextureCreateView(state->atlas_texture, nullptr);

    WGPUSamplerDescriptor sampler_desc = {};
    sampler_desc.addressModeU = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeV = WGPUAddressMode_ClampToEdge;
    sampler_desc.addressModeW = WGPUAddressMode_ClampToEdge;
    sampler_desc.magFilter = WGPUFilterMode_Nearest;
    sampler_desc.minFilter = WGPUFilterMode_Nearest;
    sampler_desc.mipmapFilter = WGPUMipmapFilterMode_Nearest;
    sampler_desc.lodMaxClamp = 32.0f;
    sampler_desc.maxAnisotropy = 1;
    state->atlas_sampler =
        wgpuDeviceCreateSampler(state->device, &sampler_desc);

    state->uniform_buffer = create_buffer(
        state->device,
        sizeof(CameraUniform),
        WGPUBufferUsage_Uniform | WGPUBufferUsage_CopyDst
    );

    f32 quad_vertices[] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    u16 quad_indices[] = {0, 1, 2, 0, 2, 3};

    state->vertex_buffer = create_buffer(
        state->device,
        sizeof(quad_vertices),
        WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    );
    wgpuQueueWriteBuffer(
        state->queue,
        state->vertex_buffer,
        0,
        quad_vertices,
        sizeof(quad_vertices)
    );

    state->index_buffer = create_buffer(
        state->device,
        sizeof(quad_indices),
        WGPUBufferUsage_Index | WGPUBufferUsage_CopyDst
    );
    wgpuQueueWriteBuffer(
        state->queue,
        state->index_buffer,
        0,
        quad_indices,
        sizeof(quad_indices)
    );

    state->instance_buffer_capacity = 1024;
    state->instance_buffer = create_buffer(
        state->device,
        (u64)sizeof(SpriteInstance) * 1024ULL,
        WGPUBufferUsage_Vertex | WGPUBufferUsage_CopyDst
    );

    WGPUBindGroupLayoutEntry bind_layout_entries[3] = {};
    bind_layout_entries[0].binding = 0;
    bind_layout_entries[0].visibility = WGPUShaderStage_Vertex;
    bind_layout_entries[0].buffer.type = WGPUBufferBindingType_Uniform;
    bind_layout_entries[0].buffer.minBindingSize = sizeof(CameraUniform);

    bind_layout_entries[1].binding = 1;
    bind_layout_entries[1].visibility = WGPUShaderStage_Fragment;
    bind_layout_entries[1].texture.sampleType = WGPUTextureSampleType_Float;
    bind_layout_entries[1].texture.viewDimension = WGPUTextureViewDimension_2D;
    bind_layout_entries[1].texture.multisampled = false;

    bind_layout_entries[2].binding = 2;
    bind_layout_entries[2].visibility = WGPUShaderStage_Fragment;
    bind_layout_entries[2].sampler.type = WGPUSamplerBindingType_Filtering;

    WGPUBindGroupLayoutDescriptor bind_layout_desc = {};
    bind_layout_desc.entryCount = ARRAY_COUNT(bind_layout_entries);
    bind_layout_desc.entries = bind_layout_entries;
    state->bind_group_layout =
        wgpuDeviceCreateBindGroupLayout(state->device, &bind_layout_desc);

    WGPUBindGroupEntry bind_entries[3] = {};
    bind_entries[0].binding = 0;
    bind_entries[0].buffer = state->uniform_buffer;
    bind_entries[0].offset = 0;
    bind_entries[0].size = sizeof(CameraUniform);
    bind_entries[1].binding = 1;
    bind_entries[1].textureView = state->atlas_view;
    bind_entries[2].binding = 2;
    bind_entries[2].sampler = state->atlas_sampler;

    WGPUBindGroupDescriptor bind_group_desc = {};
    bind_group_desc.layout = state->bind_group_layout;
    bind_group_desc.entryCount = ARRAY_COUNT(bind_entries);
    bind_group_desc.entries = bind_entries;
    state->bind_group =
        wgpuDeviceCreateBindGroup(state->device, &bind_group_desc);

    WGPUPipelineLayoutDescriptor pipeline_layout_desc = {};
    pipeline_layout_desc.bindGroupLayoutCount = 1;
    pipeline_layout_desc.bindGroupLayouts = &state->bind_group_layout;
    state->pipeline_layout =
        wgpuDeviceCreatePipelineLayout(state->device, &pipeline_layout_desc);

    WGPUVertexAttribute vertex_attributes[1] = {};
    vertex_attributes[0].shaderLocation = 0;
    vertex_attributes[0].format = WGPUVertexFormat_Float32x2;
    vertex_attributes[0].offset = 0;

    WGPUVertexAttribute instance_attributes[6] = {};
    instance_attributes[0].shaderLocation = 1;
    instance_attributes[0].format = WGPUVertexFormat_Float32x2;
    instance_attributes[0].offset = 0;
    instance_attributes[1].shaderLocation = 2;
    instance_attributes[1].format = WGPUVertexFormat_Float32x2;
    instance_attributes[1].offset = 8;
    instance_attributes[2].shaderLocation = 3;
    instance_attributes[2].format = WGPUVertexFormat_Float32x2;
    instance_attributes[2].offset = 16;
    instance_attributes[3].shaderLocation = 4;
    instance_attributes[3].format = WGPUVertexFormat_Float32x2;
    instance_attributes[3].offset = 24;
    instance_attributes[4].shaderLocation = 5;
    instance_attributes[4].format = WGPUVertexFormat_Float32;
    instance_attributes[4].offset = 32;
    instance_attributes[5].shaderLocation = 6;
    instance_attributes[5].format = WGPUVertexFormat_Float32x4;
    instance_attributes[5].offset = 36;

    WGPUVertexBufferLayout buffer_layouts[2] = {};
    buffer_layouts[0].stepMode = WGPUVertexStepMode_Vertex;
    buffer_layouts[0].arrayStride = sizeof(f32) * 2;
    buffer_layouts[0].attributeCount = ARRAY_COUNT(vertex_attributes);
    buffer_layouts[0].attributes = vertex_attributes;
    buffer_layouts[1].stepMode = WGPUVertexStepMode_Instance;
    buffer_layouts[1].arrayStride = sizeof(SpriteInstance);
    buffer_layouts[1].attributeCount = ARRAY_COUNT(instance_attributes);
    buffer_layouts[1].attributes = instance_attributes;

    WGPUVertexState vertex_state = {};
    vertex_state.module = state->shader_module;
    vertex_state.entryPoint = {"vs_main", WGPU_STRLEN};
    vertex_state.bufferCount = ARRAY_COUNT(buffer_layouts);
    vertex_state.buffers = buffer_layouts;

    WGPUBlendComponent color_blend = {};
    color_blend.operation = WGPUBlendOperation_Add;
    color_blend.srcFactor = WGPUBlendFactor_SrcAlpha;
    color_blend.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    WGPUBlendComponent alpha_blend = {};
    alpha_blend.operation = WGPUBlendOperation_Add;
    alpha_blend.srcFactor = WGPUBlendFactor_One;
    alpha_blend.dstFactor = WGPUBlendFactor_OneMinusSrcAlpha;

    WGPUBlendState blend_state = {};
    blend_state.color = color_blend;
    blend_state.alpha = alpha_blend;

    WGPUColorTargetState color_target = {};
    color_target.format = state->surface_format;
    color_target.blend = &blend_state;
    color_target.writeMask = WGPUColorWriteMask_All;

    WGPUFragmentState fragment_state = {};
    fragment_state.module = state->shader_module;
    fragment_state.entryPoint = {"fs_main", WGPU_STRLEN};
    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

    WGPUPrimitiveState primitive_state = {};
    primitive_state.topology = WGPUPrimitiveTopology_TriangleList;
    primitive_state.frontFace = WGPUFrontFace_CCW;
    primitive_state.cullMode = WGPUCullMode_None;

    WGPUDepthStencilState depth_state = {};
    depth_state.format = WGPUTextureFormat_Depth24Plus;
    depth_state.depthWriteEnabled = WGPUOptionalBool_True;
    depth_state.depthCompare = WGPUCompareFunction_LessEqual;

    WGPUMultisampleState multisample = {};
    multisample.count = 1;
    multisample.mask = ~0u;
    multisample.alphaToCoverageEnabled = false;

    WGPURenderPipelineDescriptor pipeline_desc = {};
    pipeline_desc.layout = state->pipeline_layout;
    pipeline_desc.vertex = vertex_state;
    pipeline_desc.primitive = primitive_state;
    pipeline_desc.depthStencil = &depth_state;
    pipeline_desc.multisample = multisample;
    pipeline_desc.fragment = &fragment_state;
    state->render_pipeline =
        wgpuDeviceCreateRenderPipeline(state->device, &pipeline_desc);

    result.internal_state = state;
    return result;
}

void cleanup_webgpu(WebGPURenderer* renderer) {
    if(!renderer || !renderer->internal_state) {
        return;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(state->current_view) {
        wgpuTextureViewRelease(state->current_view);
    }
    if(state->current_texture) {
        wgpuTextureRelease(state->current_texture);
    }
    if(state->depth_view) {
        wgpuTextureViewRelease(state->depth_view);
    }
    if(state->depth_texture) {
        wgpuTextureRelease(state->depth_texture);
    }
    if(state->render_pipeline) {
        wgpuRenderPipelineRelease(state->render_pipeline);
    }
    if(state->pipeline_layout) {
        wgpuPipelineLayoutRelease(state->pipeline_layout);
    }
    if(state->bind_group) {
        wgpuBindGroupRelease(state->bind_group);
    }
    if(state->bind_group_layout) {
        wgpuBindGroupLayoutRelease(state->bind_group_layout);
    }
    if(state->instance_buffer) {
        wgpuBufferRelease(state->instance_buffer);
    }
    if(state->vertex_buffer) {
        wgpuBufferRelease(state->vertex_buffer);
    }
    if(state->index_buffer) {
        wgpuBufferRelease(state->index_buffer);
    }
    if(state->uniform_buffer) {
        wgpuBufferRelease(state->uniform_buffer);
    }
    if(state->atlas_sampler) {
        wgpuSamplerRelease(state->atlas_sampler);
    }
    if(state->atlas_view) {
        wgpuTextureViewRelease(state->atlas_view);
    }
    if(state->atlas_texture) {
        wgpuTextureRelease(state->atlas_texture);
    }
    if(state->shader_module) {
        wgpuShaderModuleRelease(state->shader_module);
    }
    if(state->surface) {
        wgpuSurfaceRelease(state->surface);
    }
    if(state->queue) {
        wgpuQueueRelease(state->queue);
    }
    if(state->device) {
        wgpuDeviceRelease(state->device);
    }
    if(state->adapter) {
        wgpuAdapterRelease(state->adapter);
    }
    if(state->instance) {
        wgpuInstanceRelease(state->instance);
    }

    renderer->internal_state = nullptr;
}

bool begin_frame(WebGPURenderer* renderer, u32 width, u32 height) {
    if(!renderer || !renderer->internal_state) {
        return false;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(width == 0 || height == 0) {
        return false;
    }

    if(width != state->surface_width || height != state->surface_height) {
        state->surface_width = width;
        state->surface_height = height;
        state->configured = false;
    }

    if(!state->configured) {
        configure_surface(state);
    }

    wgpuSurfaceGetCurrentTexture(state->surface, &state->surface_texture);

    if(state->surface_texture.status !=
           WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
       state->surface_texture.status !=
           WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        LOG_ERROR(
            "Failed to get current surface texture: %d",
            (int)state->surface_texture.status
        );
        return false;
    }

    state->current_texture = state->surface_texture.texture;

    WGPUTextureViewDescriptor view_desc = {};
    view_desc.format = state->surface_format;
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    state->current_view =
        wgpuTextureCreateView(state->current_texture, &view_desc);
    if(!state->current_view) {
        LOG_ERROR("Failed to create surface texture view");
        return false;
    }

    return true;
}

void render_submit(WebGPURenderer* renderer, RenderFrame* frame) {
    if(!renderer || !renderer->internal_state || !frame) {
        return;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;
    if(!state->current_view || !state->depth_view) {
        return;
    }

    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder =
        wgpuDeviceCreateCommandEncoder(state->device, &encoder_desc);

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = state->current_view;
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = {
        frame->clear_color.r,
        frame->clear_color.g,
        frame->clear_color.b,
        frame->clear_color.a,
    };

    WGPURenderPassDepthStencilAttachment depth_attachment = {};
    depth_attachment.view = state->depth_view;
    depth_attachment.depthLoadOp = WGPULoadOp_Clear;
    depth_attachment.depthStoreOp = WGPUStoreOp_Store;
    depth_attachment.depthClearValue = 1.0f;
    depth_attachment.depthReadOnly = false;
    depth_attachment.stencilLoadOp = WGPULoadOp_Undefined;
    depth_attachment.stencilStoreOp = WGPUStoreOp_Undefined;
    depth_attachment.stencilReadOnly = true;

    WGPURenderPassDescriptor pass_desc = {};
    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;
    pass_desc.depthStencilAttachment = &depth_attachment;

    WGPURenderPassEncoder pass =
        wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);

    render_backgrounds(state, pass, &frame->backgrounds);
    render_world_sprites(state, pass, &frame->world_sprites);
    render_text(state, pass, &frame->text);
    render_ui(state, pass, &frame->ui);
    render_debug(state, pass, &frame->debug);

    wgpuRenderPassEncoderEnd(pass);
    wgpuRenderPassEncoderRelease(pass);

    WGPUCommandBufferDescriptor cmd_buffer_desc = {};
    WGPUCommandBuffer cmd = wgpuCommandEncoderFinish(encoder, &cmd_buffer_desc);
    wgpuCommandEncoderRelease(encoder);

    wgpuQueueSubmit(state->queue, 1, &cmd);
    wgpuCommandBufferRelease(cmd);
}

void end_frame(WebGPURenderer* renderer) {
    if(!renderer || !renderer->internal_state) {
        return;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(state->current_view) {
        wgpuTextureViewRelease(state->current_view);
        state->current_view = nullptr;
    }

    if(state->current_texture) {
#if !OS_EMSCRIPTEN
        wgpuSurfacePresent(state->surface);
#endif
        wgpuTextureRelease(state->current_texture);
        state->current_texture = nullptr;
    }
}
