#include "render/webgpu.h"

#include <SDL3/SDL.h>

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

struct WebGPUState {
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUSurface surface;
    WGPUPipelineLayout pipeline_layout;
    WGPURenderPipeline render_pipeline;
    WGPUBuffer uniform_buffer;
    WGPUBindGroup bind_group;

    WGPUTexture current_texture;
    WGPUTextureView current_view;
    WGPUSurfaceTexture surface_texture;

    u32 surface_width;
    u32 surface_height;
    bool configured;
};

static WGPUInstance get_instance(void) {
    WGPUInstanceDescriptor desc = {};
    return wgpuCreateInstance(&desc);
}

// TODO: When WGPULoggingCallbackInfo/wgpuDeviceSetLoggingCallback become available
// in webgpu.h, migrate to per-device logging callback for finer-grained control.
// Currently only wgpu-native's global wgpuSetLogCallback is available.
// emdawnwebgpu (Emscripten) has no logging callback API at all.
#if !OS_EMSCRIPTEN
static void wgpu_log_callback(
    WGPULogLevel level,
    WGPUStringView message,
    void* userdata
) {
    (void)userdata;
    const char* level_str = "UNKNOWN";
    switch(level) {
        case WGPULogLevel_Error:  level_str = "ERROR"; break;
        case WGPULogLevel_Warn:  level_str = "WARN"; break;
        case WGPULogLevel_Info:  level_str = "INFO"; break;
        case WGPULogLevel_Debug: level_str = "DEBUG"; break;
        case WGPULogLevel_Trace: level_str = "TRACE"; break;
        default: break;
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
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

#if defined(SDL_PLATFORM_WIN32)
    WGPUSurfaceSourceWindowsHWND from_hwnd = {};
    from_hwnd.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
    from_hwnd.hwnd = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr);
    from_hwnd.hinstance = GetModuleHandle(nullptr);

    WGPUSurfaceDescriptor desc = {};
    desc.nextInChain = &from_hwnd.chain;
    return wgpuInstanceCreateSurface(instance, &desc);
#elif defined(SDL_PLATFORM_LINUX)
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
    uint64_t x11_window = (uint64_t)SDL_GetNumberProperty(
        props,
        SDL_PROP_WINDOW_X11_WINDOW_NUMBER,
        0
    );

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
    WGPUEmscriptenSurfaceSourceCanvasHTMLSelector from_canvas = WGPU_EMSCRIPTEN_SURFACE_SOURCE_CANVAS_HTML_SELECTOR_INIT;
    from_canvas.selector = { "#canvas", WGPU_STRLEN };

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
    (void)message;
    (void)userdata2;
    WGPUAdapter* out = (WGPUAdapter*)userdata1;
    if(status == WGPURequestAdapterStatus_Success) {
        *out = adapter;
    } else {
        *out = nullptr;
        LOG_ERROR("Failed to request adapter: %.*s", (int)message.length, message.data);
    }
}

static void wgpu_request_device_callback(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void* userdata1,
    void* userdata2
) {
    (void)message;
    (void)userdata2;
    WGPUDevice* out = (WGPUDevice*)userdata1;
    if(status == WGPURequestDeviceStatus_Success) {
        *out = device;
    } else {
        *out = nullptr;
        LOG_ERROR("Failed to request device: %.*s", (int)message.length, message.data);
    }
}

WebGPURenderer init_webgpu(SDL_Window* window) {
    WebGPURenderer result = {};

    WebGPUState* state = new WebGPUState();
    memset(state, 0, sizeof(WebGPUState));

    state->instance = get_instance();
    if(!state->instance) {
        LOG_FATAL("Failed to create WebGPU instance");
        return result;
    }

#if !OS_EMSCRIPTEN
    // Global log callback (not per-device — wgpuDeviceSetLoggingCallback not yet available)
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

    LOG_INFO("Requesting WebGPU adapter...");
    WGPURequestAdapterCallbackInfo adapter_cb = {};
#if OS_EMSCRIPTEN
    adapter_cb.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    adapter_cb.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    adapter_cb.callback = wgpu_request_adapter_callback;
    adapter_cb.userdata1 = &state->adapter;
    adapter_cb.userdata2 = nullptr;

    wgpuInstanceRequestAdapter(
        state->instance,
        &adapter_opts,
        adapter_cb
    );

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
    device_desc.deviceLostCallbackInfo.userdata1 = nullptr;
    device_desc.deviceLostCallbackInfo.userdata2 = nullptr;
    device_desc.uncapturedErrorCallbackInfo.callback = wgpu_error_callback;
    device_desc.uncapturedErrorCallbackInfo.userdata1 = nullptr;
    device_desc.uncapturedErrorCallbackInfo.userdata2 = nullptr;

    WGPURequestDeviceCallbackInfo device_cb = {};
#if OS_EMSCRIPTEN
    device_cb.mode = WGPUCallbackMode_AllowSpontaneous;
#else
    device_cb.mode = WGPUCallbackMode_AllowProcessEvents;
#endif
    device_cb.callback = wgpu_request_device_callback;
    device_cb.userdata1 = &state->device;
    device_cb.userdata2 = nullptr;

    wgpuAdapterRequestDevice(
        state->adapter,
        &device_desc,
        device_cb
    );

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

    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    state->surface_width = (u32)w;
    state->surface_height = (u32)h;

    result.internal_state = state;
    return result;
}

void cleanup_webgpu(WebGPURenderer* renderer) {
    if(!renderer || !renderer->internal_state) {
        return;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(state->render_pipeline) {
        wgpuRenderPipelineRelease(state->render_pipeline);
    }
    if(state->pipeline_layout) {
        wgpuPipelineLayoutRelease(state->pipeline_layout);
    }
    if(state->bind_group) {
        wgpuBindGroupRelease(state->bind_group);
    }
    if(state->uniform_buffer) {
        wgpuBufferRelease(state->uniform_buffer);
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

    delete state;
    renderer->internal_state = nullptr;
}

static void configure_surface(WebGPUState* state) {
    if(!state->surface || !state->device) {
        return;
    }
#if OS_EMSCRIPTEN
    wgpuSurfaceUnconfigure(state->surface); // Must unconfigure before reconfiguring with new size
#endif

    WGPUSurfaceConfiguration config = {};
    config.device = state->device;
#if OS_EMSCRIPTEN
    config.format = WGPUTextureFormat_RGBA8Unorm;
#else
    config.format = WGPUTextureFormat_BGRA8Unorm;
#endif
    config.usage = WGPUTextureUsage_RenderAttachment;
    config.width = state->surface_width;
    config.height = state->surface_height;
    config.presentMode = WGPUPresentMode_Fifo;
    config.alphaMode = WGPUCompositeAlphaMode_Opaque;

    wgpuSurfaceConfigure(state->surface, &config);
    state->configured = true;
}

bool begin_frame(WebGPURenderer* renderer, u32 width, u32 height) {
    if(!renderer || !renderer->internal_state) {
        return false;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(width != state->surface_width || height != state->surface_height) {
        state->surface_width = width;
        state->surface_height = height;
        state->configured = false;
    }

    if(!state->configured) {
        configure_surface(state);
    }

    wgpuSurfaceGetCurrentTexture(state->surface, &state->surface_texture);

    if(state->surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal &&
       state->surface_texture.status != WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        LOG_ERROR("Failed to get current surface texture: %d", (int)state->surface_texture.status);
        return false;
    }

    state->current_texture = state->surface_texture.texture;

    WGPUTextureViewDescriptor view_desc = {};
#if OS_EMSCRIPTEN
    view_desc.format = WGPUTextureFormat_RGBA8Unorm;
#else
    view_desc.format = WGPUTextureFormat_BGRA8Unorm;
#endif
    view_desc.dimension = WGPUTextureViewDimension_2D;
    view_desc.baseMipLevel = 0;
    view_desc.mipLevelCount = 1;
    view_desc.baseArrayLayer = 0;
    view_desc.arrayLayerCount = 1;
    view_desc.aspect = WGPUTextureAspect_All;

    state->current_view = wgpuTextureCreateView(state->current_texture, &view_desc);
    if(!state->current_view) {
        LOG_ERROR("Failed to create texture view");
        return false;
    }

    return true;
}

void render_submit(WebGPURenderer* renderer, PushCmdBuffer* cmd_buffer) {
    if(!renderer || !renderer->internal_state || !cmd_buffer) {
        return;
    }

    WebGPUState* state = (WebGPUState*)renderer->internal_state;

    if(!state->current_view) {
        return;
    }

    WGPUCommandEncoderDescriptor encoder_desc = {};
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(state->device, &encoder_desc);

    WGPURenderPassDescriptor pass_desc = {};

    WGPURenderPassColorAttachment color_attachment = {};
    color_attachment.view = state->current_view;
    color_attachment.depthSlice = WGPU_DEPTH_SLICE_UNDEFINED;
    color_attachment.loadOp = WGPULoadOp_Clear;
    color_attachment.storeOp = WGPUStoreOp_Store;
    color_attachment.clearValue = (WGPUColor){0.0, 0.0, 0.0, 1.0};

    pass_desc.colorAttachmentCount = 1;
    pass_desc.colorAttachments = &color_attachment;

    WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(encoder, &pass_desc);

    u32 offset = 0;
    for(u32 i = 0; i < cmd_buffer->cmd_count; i++) {
        PushCmd* cmd = (PushCmd*)(cmd_buffer->base + offset);

        switch(cmd->type) {
            case CmdType_Clear: {
                CmdClear* clear = (CmdClear*)cmd;
                color_attachment.clearValue = (WGPUColor){
                    clear->color.r,
                    clear->color.g,
                    clear->color.b,
                    clear->color.a
                };
                pass_desc.colorAttachments = &color_attachment;
                break;
            }
            case CmdType_Rect: {
                break;
            }
            default:
                break;
        }

        offset = (offset + cmd->size + 7) & ~7;
    }

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
