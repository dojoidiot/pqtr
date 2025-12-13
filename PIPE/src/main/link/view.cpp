// view.cpp - PNG snapshot pushed to BASE
//
// Generates PNG on GPU and pushes to BASE server.
// Runs in browser WASM - no filesystem access.
//
// The fidelity rule: view quantizes to 8-bit for POST only.
// Pipeline data (Page) passes through unchanged at full precision.

#include <pipe.hpp>
#include <wgpu.hpp>
#include <cstring>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/fetch.h>
#endif

// Embedded WGSL shader for PNG encoding (v1: uncompressed)
static const char* POST_WGSL = R"(
struct Params {
    width: u32,
    height: u32,
    row_bytes: u32,
    idat_size: u32,
}

@group(0) @binding(0) var<storage, read> pixels: array<vec4f>;
@group(0) @binding(1) var<storage, read_write> output: array<u32>;
@group(0) @binding(2) var<uniform> params: Params;

fn crc32_byte(crc: u32, byte: u32) -> u32 {
    var c = crc ^ byte;
    for (var i = 0u; i < 8u; i++) {
        if ((c & 1u) != 0u) {
            c = (c >> 1u) ^ 0xEDB88320u;
        } else {
            c = c >> 1u;
        }
    }
    return c;
}

fn write_byte(offset: u32, byte: u32) {
    let word_idx = offset / 4u;
    let byte_idx = offset % 4u;
    let shift = byte_idx * 8u;
    let mask = ~(0xFFu << shift);
    let old = output[word_idx];
    output[word_idx] = (old & mask) | ((byte & 0xFFu) << shift);
}

fn write_u32_be(offset: u32, value: u32) {
    write_byte(offset, (value >> 24u) & 0xFFu);
    write_byte(offset + 1u, (value >> 16u) & 0xFFu);
    write_byte(offset + 2u, (value >> 8u) & 0xFFu);
    write_byte(offset + 3u, value & 0xFFu);
}

fn write_u16_le(offset: u32, value: u32) {
    write_byte(offset, value & 0xFFu);
    write_byte(offset + 1u, (value >> 8u) & 0xFFu);
}

@compute @workgroup_size(1)
fn write_header() {
    // PNG Signature
    write_byte(0u, 0x89u);
    write_byte(1u, 0x50u);
    write_byte(2u, 0x4Eu);
    write_byte(3u, 0x47u);
    write_byte(4u, 0x0Du);
    write_byte(5u, 0x0Au);
    write_byte(6u, 0x1Au);
    write_byte(7u, 0x0Au);

    // IHDR chunk
    write_u32_be(8u, 13u);
    write_byte(12u, 0x49u);
    write_byte(13u, 0x48u);
    write_byte(14u, 0x44u);
    write_byte(15u, 0x52u);
    write_u32_be(16u, params.width);
    write_u32_be(20u, params.height);
    write_byte(24u, 8u);
    write_byte(25u, 2u);
    write_byte(26u, 0u);
    write_byte(27u, 0u);
    write_byte(28u, 0u);

    // IHDR CRC
    var crc = 0xFFFFFFFFu;
    crc = crc32_byte(crc, 0x49u);
    crc = crc32_byte(crc, 0x48u);
    crc = crc32_byte(crc, 0x44u);
    crc = crc32_byte(crc, 0x52u);
    crc = crc32_byte(crc, (params.width >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (params.width >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (params.width >> 8u) & 0xFFu);
    crc = crc32_byte(crc, params.width & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (params.height >> 8u) & 0xFFu);
    crc = crc32_byte(crc, params.height & 0xFFu);
    crc = crc32_byte(crc, 8u);
    crc = crc32_byte(crc, 2u);
    crc = crc32_byte(crc, 0u);
    crc = crc32_byte(crc, 0u);
    crc = crc32_byte(crc, 0u);
    crc = crc ^ 0xFFFFFFFFu;
    write_u32_be(29u, crc);

    // IDAT header
    write_u32_be(33u, params.idat_size);
    write_byte(37u, 0x49u);
    write_byte(38u, 0x44u);
    write_byte(39u, 0x41u);
    write_byte(40u, 0x54u);

    // zlib header
    write_byte(41u, 0x78u);
    write_byte(42u, 0x01u);
}

@compute @workgroup_size(256)
fn encode_rows(@builtin(global_invocation_id) gid: vec3u) {
    let row = gid.x;
    if (row >= params.height) {
        return;
    }

    let block_header_size = 5u;
    let row_data_size = params.row_bytes;
    let block_size = block_header_size + row_data_size;
    let base_offset = 43u + row * block_size;

    let is_final = (row == params.height - 1u);
    write_byte(base_offset, select(0x00u, 0x01u, is_final));
    write_u16_le(base_offset + 1u, row_data_size);
    write_u16_le(base_offset + 3u, row_data_size ^ 0xFFFFu);
    write_byte(base_offset + 5u, 0u);

    for (var x = 0u; x < params.width; x++) {
        let pixel_idx = row * params.width + x;
        let pixel = pixels[pixel_idx];

        let r = u32(clamp(pixel.r, 0.0, 1.0) * 255.0 + 0.5);
        let g = u32(clamp(pixel.g, 0.0, 1.0) * 255.0 + 0.5);
        let b = u32(clamp(pixel.b, 0.0, 1.0) * 255.0 + 0.5);

        let byte_offset = base_offset + 6u + x * 3u;
        write_byte(byte_offset, r);
        write_byte(byte_offset + 1u, g);
        write_byte(byte_offset + 2u, b);
    }
}

fn adler32_update(adler: vec2u, byte: u32) -> vec2u {
    let a = (adler.x + byte) % 65521u;
    let b = (adler.y + a) % 65521u;
    return vec2u(a, b);
}

@compute @workgroup_size(1)
fn write_footer() {
    var adler = vec2u(1u, 0u);
    let data_start = 43u;
    let block_header_size = 5u;
    let row_data_size = params.row_bytes;
    let block_size = block_header_size + row_data_size;

    for (var row = 0u; row < params.height; row++) {
        let row_offset = data_start + row * block_size + block_header_size;
        for (var i = 0u; i < row_data_size; i++) {
            let byte_offset = row_offset + i;
            let word_idx = byte_offset / 4u;
            let byte_idx = byte_offset % 4u;
            let byte_val = (output[word_idx] >> (byte_idx * 8u)) & 0xFFu;
            adler = adler32_update(adler, byte_val);
        }
    }

    let adler32_offset = 43u + params.height * block_size;
    let adler32_val = (adler.y << 16u) | adler.x;
    write_u32_be(adler32_offset, adler32_val);

    var crc = 0xFFFFFFFFu;
    crc = crc32_byte(crc, 0x49u);
    crc = crc32_byte(crc, 0x44u);
    crc = crc32_byte(crc, 0x41u);
    crc = crc32_byte(crc, 0x54u);
    crc = crc32_byte(crc, 0x78u);
    crc = crc32_byte(crc, 0x01u);

    for (var row = 0u; row < params.height; row++) {
        let block_offset = data_start + row * block_size;
        for (var i = 0u; i < block_size; i++) {
            let byte_offset = block_offset + i;
            let word_idx = byte_offset / 4u;
            let byte_idx = byte_offset % 4u;
            let byte_val = (output[word_idx] >> (byte_idx * 8u)) & 0xFFu;
            crc = crc32_byte(crc, byte_val);
        }
    }

    crc = crc32_byte(crc, (adler32_val >> 24u) & 0xFFu);
    crc = crc32_byte(crc, (adler32_val >> 16u) & 0xFFu);
    crc = crc32_byte(crc, (adler32_val >> 8u) & 0xFFu);
    crc = crc32_byte(crc, adler32_val & 0xFFu);
    crc = crc ^ 0xFFFFFFFFu;
    write_u32_be(adler32_offset + 4u, crc);

    // IEND
    let iend_offset = adler32_offset + 8u;
    write_u32_be(iend_offset, 0u);
    write_byte(iend_offset + 4u, 0x49u);
    write_byte(iend_offset + 5u, 0x45u);
    write_byte(iend_offset + 6u, 0x4Eu);
    write_byte(iend_offset + 7u, 0x44u);

    var iend_crc = 0xFFFFFFFFu;
    iend_crc = crc32_byte(iend_crc, 0x49u);
    iend_crc = crc32_byte(iend_crc, 0x45u);
    iend_crc = crc32_byte(iend_crc, 0x4Eu);
    iend_crc = crc32_byte(iend_crc, 0x44u);
    iend_crc = iend_crc ^ 0xFFFFFFFFu;
    write_u32_be(iend_offset + 8u, iend_crc);
}
)";

namespace pipe {

class ViewLink : public Link {
public:
    Name name() const override { return "view"; }
    Name type() const override { return "view"; }

    Data flow(Data in) override {
        auto* ctx = static_cast<wgpu::Context*>(in.page);
        if (!ctx || !ctx->device) {
            in.info.text("error", "view: no gpu context");
            return in;
        }

        // Must have RGB data (channels == 3 or 4)
        if (ctx->channels < 3) {
            in.info.text("error", "view: need RGB data");
            return in;
        }

        uint32_t width = static_cast<uint32_t>(ctx->width);
        uint32_t height = static_cast<uint32_t>(ctx->height);

        // Calculate PNG sizes
        uint32_t row_bytes = 1 + width * 3;  // filter byte + RGB
        uint32_t block_size = 5 + row_bytes;  // DEFLATE block header + data
        uint32_t deflate_size = height * block_size;
        uint32_t idat_size = 2 + deflate_size + 4;  // zlib header + data + adler32

        // Total PNG size: signature(8) + IHDR(25) + IDAT(8+idat_size+4) + IEND(12)
        uint32_t png_size = 8 + 25 + 8 + idat_size + 4 + 12;
        uint32_t buffer_words = (png_size + 3) / 4;

        // Create params
        struct Params {
            uint32_t width;
            uint32_t height;
            uint32_t row_bytes;
            uint32_t idat_size;
        } params = { width, height, row_bytes, idat_size };

        // Create uniform buffer
        ::wgpu::BufferDescriptor paramDesc{};
        paramDesc.size = sizeof(Params);
        paramDesc.usage = ::wgpu::BufferUsage::Uniform | ::wgpu::BufferUsage::CopyDst;
        ::wgpu::Buffer paramBuffer = ctx->device.CreateBuffer(&paramDesc);
        ctx->device.GetQueue().WriteBuffer(paramBuffer, 0, &params, sizeof(Params));

        // Create output buffer for PNG
        ::wgpu::BufferDescriptor outDesc{};
        outDesc.size = buffer_words * 4;
        outDesc.usage = ::wgpu::BufferUsage::Storage | ::wgpu::BufferUsage::CopySrc;
        outDesc.mappedAtCreation = true;
        ::wgpu::Buffer outBuffer = ctx->device.CreateBuffer(&outDesc);

        // Zero-initialize output buffer
        void* mapped = outBuffer.GetMappedRange(0, buffer_words * 4);
        std::memset(mapped, 0, buffer_words * 4);
        outBuffer.Unmap();

        // Create shader module
        ::wgpu::ShaderModuleWGSLDescriptor wgslDesc{};
        wgslDesc.code = POST_WGSL;

        ::wgpu::ShaderModuleDescriptor shaderDesc{};
        shaderDesc.nextInChain = &wgslDesc;
        ::wgpu::ShaderModule shader = ctx->device.CreateShaderModule(&shaderDesc);

        // Create three pipelines for the three entry points
        auto createPipeline = [&](const char* entry) -> ::wgpu::ComputePipeline {
            ::wgpu::ComputePipelineDescriptor pipeDesc{};
            pipeDesc.compute.module = shader;
            pipeDesc.compute.entryPoint = entry;
            return ctx->device.CreateComputePipeline(&pipeDesc);
        };

        auto headerPipe = createPipeline("write_header");
        auto rowsPipe = createPipeline("encode_rows");
        auto footerPipe = createPipeline("write_footer");

        // Create bind group (same layout for all pipelines)
        ::wgpu::BindGroupEntry entries[3] = {};
        entries[0].binding = 0;
        entries[0].buffer = ctx->buffer;
        entries[0].size = width * height * ctx->channels * sizeof(float);
        entries[1].binding = 1;
        entries[1].buffer = outBuffer;
        entries[1].size = buffer_words * 4;
        entries[2].binding = 2;
        entries[2].buffer = paramBuffer;
        entries[2].size = sizeof(Params);

        ::wgpu::BindGroupDescriptor bgDesc{};
        bgDesc.layout = headerPipe.GetBindGroupLayout(0);
        bgDesc.entryCount = 3;
        bgDesc.entries = entries;
        ::wgpu::BindGroup bindGroup = ctx->device.CreateBindGroup(&bgDesc);

        // Execute: header -> rows -> footer
        ::wgpu::CommandEncoder encoder = ctx->device.CreateCommandEncoder();

        // Header pass
        {
            ::wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(headerPipe);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchWorkgroups(1);
            pass.End();
        }

        // Rows pass
        {
            ::wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(rowsPipe);
            pass.SetBindGroup(0, bindGroup);
            uint32_t workgroups = (height + 255) / 256;
            pass.DispatchWorkgroups(workgroups);
            pass.End();
        }

        // Footer pass
        {
            ::wgpu::ComputePassEncoder pass = encoder.BeginComputePass();
            pass.SetPipeline(footerPipe);
            pass.SetBindGroup(0, bindGroup);
            pass.DispatchWorkgroups(1);
            pass.End();
        }

        ::wgpu::CommandBuffer commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Create staging buffer for readback
        ::wgpu::BufferDescriptor stagingDesc{};
        stagingDesc.size = png_size;
        stagingDesc.usage = ::wgpu::BufferUsage::CopyDst | ::wgpu::BufferUsage::MapRead;
        ::wgpu::Buffer staging = ctx->device.CreateBuffer(&stagingDesc);

        // Copy GPU -> staging
        encoder = ctx->device.CreateCommandEncoder();
        encoder.CopyBufferToBuffer(outBuffer, 0, staging, 0, png_size);
        commands = encoder.Finish();
        ctx->device.GetQueue().Submit(1, &commands);

        // Map and read PNG data
        bool done = false;
        staging.MapAsync(
            ::wgpu::MapMode::Read, 0, png_size,
            ::wgpu::CallbackMode::AllowSpontaneous,
            [&](::wgpu::MapAsyncStatus status, ::wgpu::StringView) {
                done = true;
            });

        while (!done) {
            ctx->device.GetAdapter().GetInstance().ProcessEvents();
        }

        // Copy PNG data
        const uint8_t* src = static_cast<const uint8_t*>(staging.GetConstMappedRange(0, png_size));
        std::vector<uint8_t> pngData(src, src + png_size);
        staging.Unmap();

        // Get name and JWT from info
        std::string baseName = in.info.text("name");
        std::string jwt = in.info.text("jwt");

        if (baseName.empty()) {
            in.info.text("error", "view: no name in info");
            return in;
        }

        // Push to BASE
        std::string filename = baseName + ".png";

#ifdef __EMSCRIPTEN__
        // Build URL: POST /push?name={name}&file={filename}
        std::string url = "/push?name=" + baseName + "&file=" + filename;

        // Store PNG data for async push (static to survive callback)
        static std::vector<uint8_t> s_pngData;
        static std::string s_jwt;
        s_pngData = std::move(pngData);
        s_jwt = jwt;

        // Build auth header
        static char auth_header[2100];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", s_jwt.c_str());
        static const char* headers[] = {"Authorization", auth_header, nullptr};

        emscripten_fetch_attr_t attr;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "POST");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.requestHeaders = headers;
        attr.requestData = reinterpret_cast<const char*>(s_pngData.data());
        attr.requestDataSize = s_pngData.size();

        attr.onsuccess = [](emscripten_fetch_t* fetch) {
            emscripten_fetch_close(fetch);
        };

        attr.onerror = [](emscripten_fetch_t* fetch) {
            emscripten_fetch_close(fetch);
        };

        emscripten_fetch(&attr, url.c_str());
#endif

        // Cleanup temporary buffers
        outBuffer.Destroy();

        // Pass through unchanged - fidelity preserved
        Data out;
        out.page = ctx;  // Same context, unchanged
        out.info = std::move(in.info);
        out.info.text("view", filename);
        out.info.dial("png_size", static_cast<float>(png_size));

        return out;
    }
};

// Factory
Hold<Link> view() {
    return Hold<Link>(new ViewLink());
}

} // namespace pipe
