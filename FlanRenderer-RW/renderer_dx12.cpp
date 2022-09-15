#include "renderer.h"
#define GLFW_EXPOSE_NATIVE_WIN32

#include <iostream>
#include <d3d12.h>
#include <d3d12sdklayers.h>
#include <dxgi1_6.h>
#include <glfw/glfw3.h>
#include <glfw/glfw3native.h>
#include <vector>
#include "glm/vec3.hpp"
#include <fstream>
#include <chrono>
#include <numeric>
#include <wrl.h>

using Microsoft::WRL::ComPtr;
static constexpr UINT backbuffer_count = 2;

void throw_if_failed(const HRESULT hr) {
    if (FAILED(hr)) {
        throw std::exception();
    }
}

ShaderGPU Renderer::load_shader(std::string path)
{
}

bool Renderer::is_running()
{
}

void Renderer::update_camera_view(glm::mat4 view_matrix)
{
}

void Renderer::update_camera_proj(glm::mat4 proj_matrix)
{
}

void* Renderer::get_window()
{
}

TextureGPU Renderer::get_framebuffer_texture()
{
}

void Renderer::bind_texture(int slot, TextureGPU texture)
{
}

void Renderer::bind_mesh(MeshGPU mesh)
{
}

void Renderer::bind_shader(ShaderGPU shader)
{
}

void Renderer::bind_material_pbr(MaterialGPU material)
{
}

void Renderer::create_render_context()
{
    // Create command queue
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    throw_if_failed(render_ctx.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&render_ctx.command_queue)));

    ComPtr<ID3D12CommandAllocator> command_allocator;
    throw_if_failed(render_ctx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    // Create fence
    UINT frame_index;
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    UINT64 fence_values[backbuffer_count];
    for (unsigned i = 0u; i < backbuffer_count; ++i) {
        throw_if_failed(render_ctx.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&render_ctx.fences[i])));
        fence_values[i] = 0;
    }

    // Declare variables we need for swapchain
    UINT render_target_view_descriptor_size;
    D3D12_VIEWPORT viewport;
    D3D12_RECT surface_size;

    // Define surface size
    surface_size.left = 0;
    surface_size.top = 0;
    surface_size.right = render_ctx.resolution.x;
    surface_size.bottom = render_ctx.resolution.y;

    // Define viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(render_ctx.resolution.x);
    viewport.Height = static_cast<float>(render_ctx.resolution.y);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Create swapchain description
    render_ctx.swapchain_desc.BufferCount = backbuffer_count;
    render_ctx.swapchain_desc.Width = render_ctx.resolution.x;
    render_ctx.swapchain_desc.Height = render_ctx.resolution.y;
    render_ctx.swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    render_ctx.swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    render_ctx.swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    render_ctx.swapchain_desc.SampleDesc.Count = 1;

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    render_ctx.factory->CreateSwapChainForHwnd(render_ctx.command_queue.Get(), render_ctx.hwnd, &render_ctx.swapchain_desc,
        nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
    if (SUCCEEDED(swapchain_support)) {
        render_ctx.swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
    }

    if (!render_ctx.swapchain) {
        throw std::exception();
    }

    render_ctx.frame_index = render_ctx.swapchain->GetCurrentBackBufferIndex();

}

void Renderer::init_framebuffer()
{
}

TextureGPU Renderer::upload_texture_to_gpu(const ResourceHandle texture_handle, bool is_srgb, bool unload_resource_afterwards)
{
}

TextureGPU Renderer::upload_cubemap_to_gpu(std::vector<ResourceHandle> texture_handle, bool unload_resource_afterwards)
{
}

ModelGPU Renderer::upload_mesh_to_gpu(ResourceHandle model_handle, bool unload_resources)
{
}

void Renderer::clear_framebuffer()
{
}

void Renderer::platform_specific_begin_frame()
{
}

void Renderer::platform_specific_end_frame()
{
}

template <typename T>
void Renderer::init_or_update_constant_buffer(int slot, ConstantBufferGPU& const_buffer, T*& buffer_data)
{
    //If the buffer_data pointer == nullptr, create the buffer_data object
    if (buffer_data == nullptr)
    {
        ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "constant buffer";
        buffer_data = static_cast<T*>(dynamic_allocate(sizeof(T)));
        ResourceManager::get_allocator_instance()->curr_memory_chunk_label = "untitled";
        memset(buffer_data, 0, sizeof(T));
    }
    //If the buffer doesn't already exist on GPU, create it 
    if (const_buffer.data == nullptr)
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_DESCRIPTOR_HEAP_DESC heap_desc{
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            1,
            D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE,
            0
        };

        throw_if_failed(render_ctx.device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&const_buffer.heap)));

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            (sizeof(T) | 0xFF) + 1, // Constant buffers must be 256-byte aligned
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(render_ctx.device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            &const_buffer.resource));

        assert(const_buffer_heap != nullptr);
        const_buffer.heap->SetName(L"Constant Buffer Upload Resource Heap");

        // Create the buffer view
        const_buffer.view_desc = {
            const_buffer.resource->GetGPUVirtualAddress(),
            (sizeof(T) | 0xFF) + 1
        };

        D3D12_CPU_DESCRIPTOR_HANDLE const_buffer_view_handle(const_buffer.heap->GetCPUDescriptorHandleForHeapStart());
        render_ctx.device->CreateConstantBufferView(&const_buffer.view_desc, const_buffer_view_handle);

    }

    // Bind the constant buffer, copy the data to it, then unbind the constant buffer
    throw_if_failed(const_buffer.resource->Map(0, &const_buffer.range, &const_buffer.data));
    memcpy_s(const_buffer.data, sizeof(T), &buffer_data, sizeof(T));
    const_buffer.resource->Unmap(0, nullptr);
}

void Renderer::platform_specific_init()
{    // Create window - use GLFW_NO_API, since we're not using OpenGL
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(render_ctx.resolution.x, render_ctx.resolution.y, "Hello Triangle (DirectX 12)", nullptr, nullptr);
    HWND hwnd = glfwGetWin32Window(window);

    // Declare DirectX 12 handles
    ComPtr<IDXGIFactory4> factory;
    [[maybe_unused]] ComPtr<ID3D12Debug1> debug_interface;// If we're in release mode, this variable will be unused

    // Create factory
    UINT dxgi_factory_flags = 0;

#if _DEBUG
    // If we're in debug mode, create a debug layer for proper error tracking
    // Note: Errors will be printed in the Visual Studio output tab, and not in the console!
    ComPtr<ID3D12Debug> debug_layer;
    throw_if_failed(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_layer)));
    throw_if_failed(debug_layer->QueryInterface(IID_PPV_ARGS(&debug_interface)));
    debug_interface->EnableDebugLayer();
    debug_interface->SetEnableGPUBasedValidation(true);
    dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
    debug_layer->Release();
#endif

    // This is for debugging purposes, so it may be unused
    [[maybe_unused]] HRESULT result = CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(&factory));

    // Create the device handle
    ComPtr<ID3D12Device> device = nullptr;

    // Create adapter
    ComPtr<IDXGIAdapter1> adapter;
    UINT adapter_index = 0;
    while (factory->EnumAdapters1(adapter_index, &adapter) != DXGI_ERROR_NOT_FOUND) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Ignore software renderer - we want a hardware adapter
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // We should have a hardware adapter now, but does it support Direct3D 12.0?
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device)))) {
            // Yes it does! We use this one.
            break;
        }

        // It doesn't? Unfortunate, let it go and try another adapter
        device = nullptr;
        adapter->Release();
        adapter_index++;
    }

    if (device == nullptr) {
        throw std::exception();
    }

#if _DEBUG
    // If we're in debug mode, create the debug device handle
    ComPtr<ID3D12DebugDevice> device_debug;
    throw_if_failed(device->QueryInterface(device_debug.GetAddressOf()));
#endif

    // Create command queue
    ComPtr<ID3D12CommandQueue> command_queue;
    D3D12_COMMAND_QUEUE_DESC queue_desc = {};
    queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

    throw_if_failed(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&command_queue)));

    // Create command allocator
    ComPtr<ID3D12CommandAllocator> command_allocator;
    throw_if_failed(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&command_allocator)));

    // Create fence
    UINT frame_index;
    HANDLE fence_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    ComPtr<ID3D12Fence> fences[backbuffer_count];
    UINT64 fence_values[backbuffer_count];
    for (unsigned i = 0u; i < backbuffer_count; ++i) {
        throw_if_failed(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fences[i])));
        fence_values[i] = 0;
    }

    // Declare variables we need for swapchain
    ComPtr<ID3D12DescriptorHeap> render_target_view_heap;
    ComPtr<IDXGISwapChain3> swapchain = nullptr;
    ComPtr<ID3D12Resource> render_targets[backbuffer_count];
    UINT render_target_view_descriptor_size;
    D3D12_VIEWPORT viewport;
    D3D12_RECT surface_size;

    // Define surface size
    surface_size.left = 0;
    surface_size.top = 0;
    surface_size.right = render_ctx.resolution.x;
    surface_size.bottom = render_ctx.resolution.y;

    // Define viewport
    viewport.TopLeftX = 0.0f;
    viewport.TopLeftY = 0.0f;
    viewport.Width = static_cast<float>(render_ctx.resolution.x);
    viewport.Height = static_cast<float>(render_ctx.resolution.y);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    // Create swapchain description
    DXGI_SWAP_CHAIN_DESC1 swapchain_desc{};
    swapchain_desc.BufferCount = backbuffer_count;
    swapchain_desc.Width = render_ctx.resolution.x;
    swapchain_desc.Height = render_ctx.resolution.y;
    swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapchain_desc.SampleDesc.Count = 1;

    // Create swapchain
    IDXGISwapChain1* new_swapchain;
    result = factory->CreateSwapChainForHwnd(command_queue.Get(), hwnd, &swapchain_desc,
        nullptr, nullptr, &new_swapchain);
    HRESULT swapchain_support = new_swapchain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&new_swapchain));
    if (SUCCEEDED(swapchain_support)) {
        swapchain = static_cast<IDXGISwapChain3*>(new_swapchain);
    }

    if (!swapchain) {
        throw std::exception();
    }

    frame_index = swapchain->GetCurrentBackBufferIndex();

    // Create render target view heap descriptor
    D3D12_DESCRIPTOR_HEAP_DESC render_target_view_heap_desc{};
    render_target_view_heap_desc.NumDescriptors = backbuffer_count;
    render_target_view_heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    render_target_view_heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    throw_if_failed(device->CreateDescriptorHeap(&render_target_view_heap_desc, IID_PPV_ARGS(&render_target_view_heap)));

    render_target_view_descriptor_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Create frame resources
    {
        D3D12_CPU_DESCRIPTOR_HANDLE render_target_view_handle(render_target_view_heap->GetCPUDescriptorHandleForHeapStart());

        // Create RTV for each frame
        for (UINT i = 0; i < backbuffer_count; i++) {
            throw_if_failed(swapchain->GetBuffer(i, IID_PPV_ARGS(&render_targets[i])));
            device->CreateRenderTargetView(render_targets[i].Get(), nullptr, render_target_view_handle);
            render_target_view_handle.ptr += render_target_view_descriptor_size;
        }
    }

    ComPtr<ID3D12RootSignature> root_signature = nullptr;
    D3D12_FEATURE_DATA_ROOT_SIGNATURE feature_data{};
    feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;

    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &feature_data, sizeof(feature_data)))) {
        feature_data.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
    }

    // We want one constant buffer resource on the GPU, so add it to the descriptor ranges
    D3D12_DESCRIPTOR_RANGE1 ranges[1];
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    ranges[0].NumDescriptors = 1;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = 0;
    ranges[0].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

    // Bind the descriptor ranges to the descriptor table of the root signature
    D3D12_ROOT_PARAMETER1 root_parameters[1];
    root_parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
    root_parameters[0].DescriptorTable.NumDescriptorRanges = 1;
    root_parameters[0].DescriptorTable.pDescriptorRanges = ranges;

    // Create a root signature description
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc;
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    root_signature_desc.Desc_1_1.NumParameters = 1;
    root_signature_desc.Desc_1_1.pParameters = root_parameters;
    root_signature_desc.Desc_1_1.NumStaticSamplers = 0;
    root_signature_desc.Desc_1_1.pStaticSamplers = nullptr;

    // Now let's create a root signature
    ComPtr<ID3DBlob> signature;
    ComPtr<ID3DBlob> error;
    try {
        throw_if_failed(D3D12SerializeVersionedRootSignature(&root_signature_desc, &signature, &error));
        throw_if_failed(device->CreateRootSignature(0, signature->GetBufferPointer(),
            signature->GetBufferSize(), IID_PPV_ARGS(&root_signature)));
        root_signature->SetName(L"Hello Triangle Root Signature");
    }
    catch ([[maybe_unused]] std::exception& e) {
        auto err_str = static_cast<const char*>(error->GetBufferPointer());
        std::cout << err_str;
        error->Release();
        error = nullptr;
    }

    if (signature) {
        signature->Release();
        signature = nullptr;
    }
}

MeshGPU Renderer::init_vertex_buffer(Vertex* vertices, int n_vertices)
{
    // Create MeshGPU
    MeshGPU out;
    out.vert_count = n_vertices;

    // Generate index buffer
    std::vector<uint32_t> indices(n_vertices);
    std::iota(indices.begin(), indices.end(), 0);
    
    // Only the GPU needs this data, the CPU won't need this
    D3D12_RANGE vertex_range{ 0, 0 };
    uint8_t* vertex_data_begin;
    const size_t vert_size = sizeof(Vertex) * n_vertices;

    // Upload vertex buffer to GPU
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            vert_size,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(render_ctx.device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource), &out.vertex_buffer));

        // Bind the vertex buffer, copy the data to it, then unbind the vertex buffer
        throw_if_failed(out.vertex_buffer->Map(0, &vertex_range, reinterpret_cast<void**>(&vertex_data_begin)));
        memcpy_s(vertex_data_begin, vert_size, vertices, vert_size);
        out.vertex_buffer->Unmap(0, nullptr);

        // Init the buffer view
        out.vertex_buffer_view = D3D12_VERTEX_BUFFER_VIEW{
            out.vertex_buffer->GetGPUVirtualAddress(),
            static_cast<UINT>(vert_size),
            sizeof(Vertex),
        };
    }

    /* INDEX BUFFER VIEW
    * Same idea as vertex buffer view, except the data integers
    */
    

    // Only the GPU needs this data, the CPU won't need this
    D3D12_RANGE index_range{ 0, 0 };
    uint8_t* index_data_begin = nullptr;
    const size_t triangle_index_size = sizeof(indices[0]) * indices.size();

    // Upload index buffer to GPU
    {
        D3D12_HEAP_PROPERTIES upload_heap_props = {
            D3D12_HEAP_TYPE_UPLOAD, // The heap will be used to upload data to the GPU
            D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
            D3D12_MEMORY_POOL_UNKNOWN, 1, 1 };

        D3D12_RESOURCE_DESC upload_buffer_desc = {
            D3D12_RESOURCE_DIMENSION_BUFFER, // Can either be texture or buffer, we want a buffer
            0,
            triangle_index_size,
            1,
            1,
            1,
            DXGI_FORMAT_UNKNOWN, // This is only really useful for textures, so for buffer this is unknown
            {1, 0}, // Texture sampling quality settings, not important for non-textures, so set it to lowest
            D3D12_TEXTURE_LAYOUT_ROW_MAJOR, // First left to right, then top to bottom
            D3D12_RESOURCE_FLAG_NONE,
        };


        throw_if_failed(render_ctx.device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, __uuidof(ID3D12Resource),
            &out.index_buffer));

        // Bind the index buffer, copy the data to it, then unbind the index buffer
        throw_if_failed(out.index_buffer->Map(0, &index_range, reinterpret_cast<void**>(&index_data_begin)));
        memcpy_s(index_data_begin, triangle_index_size, indices.data(), triangle_index_size);
        out.index_buffer->Unmap(0, nullptr);

        // Init the buffer view
        out.index_buffer_view = D3D12_INDEX_BUFFER_VIEW{
            out.index_buffer->GetGPUVirtualAddress(),
            static_cast<UINT>(triangle_index_size),
            DXGI_FORMAT_R32_UINT,
        };
    }
    return out;
}

//Tries to load a shader stage. If the shader part can not be found, it will be skipped silently
bool Renderer::load_shader_part(const std::string& path, ShaderType type, const ShaderGPU& program)
{
}

void Renderer::issue_draw_call(int draw_count)
{
}

void Renderer::flip_buffers()
{
}

void Renderer::bind_constant_buffer(int slot, const ConstantBufferGPU& constant_buffer_gpu)
{
}