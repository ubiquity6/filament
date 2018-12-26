/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "driver/CommandStream.h"
#include "driver/CommandStreamDispatcher.h"
#include "driver/metal/MetalDriver.h"

#include "MetalBinder.h"
#include "MetalEnums.h"
#include "MetalHandles.h"

#include <AppKit/AppKit.h>
#include <Metal/Metal.h>
#include <QuartzCore/QuartzCore.h>

#include <utils/Log.h>
#include <utils/Panic.h>
#include <utils/trap.h>

#pragma clang diagnostic push
#pragma ide diagnostic ignored "HidingNonVirtualFunction"
namespace filament {
namespace driver {
namespace metal {

struct MetalDriverImpl {
    id<MTLDevice> mDevice = nullptr;
    id<MTLCommandQueue> mCommandQueue = nullptr;

    NSAutoreleasePool* mDriverPool = nil;
    NSAutoreleasePool* mPool = nil;

    // Single use, re-created each frame.
    id<MTLCommandBuffer> mCurrentCommandBuffer = nullptr;
    id<MTLRenderCommandEncoder> mCurrentCommandEncoder = nullptr;

    PipelineStateTracker mPipelineState;
    PipelineStateCache mPipelineStateCache;

    UniformBufferStateTracker mUniformState[VERTEX_BUFFER_START];

    DepthStencilStateTracker mDepthStencilState;
    DepthStencilStateCache mDepthStencilStateCache;

    SamplerStateCache mSamplerStateCache;

    id<MTLSamplerState> mBoundSamplers[NUM_SAMPLER_BINDINGS] = {};
    id<MTLTexture> mBoundTextures[NUM_SAMPLER_BINDINGS] = {};
    bool mSamplersDirty = true;
    bool mTexturesDirty = true;

    CullModeStateTracker mCullModeState;
    ViewportStateTracker mViewportState;

    MetalSamplerBuffer* mSamplerBindings[NUM_SAMPLER_BINDINGS] = {};

    // Surface-related properties.
    MetalSwapChain* mCurrentSurface = nullptr;
    id<CAMetalDrawable> mCurrentDrawable = nullptr;
    MTLPixelFormat mCurrentSurfacePixelFormat = MTLPixelFormatInvalid;
    MTLPixelFormat mCurrentDepthPixelFormat = MTLPixelFormatInvalid;
};

Driver* MetalDriver::create(MetalPlatform* const platform) {
    assert(platform);
    return new MetalDriver(platform);
}

MetalDriver::MetalDriver(driver::MetalPlatform* platform) noexcept
        : DriverBase(new ConcreteDispatcher<MetalDriver>(this)),
        mPlatform(*platform),
        pImpl(new MetalDriverImpl) {

    pImpl->mDriverPool = [[NSAutoreleasePool alloc] init];
    pImpl->mDevice = MTLCreateSystemDefaultDevice();
    pImpl->mCommandQueue = [pImpl->mDevice newCommandQueue];
    pImpl->mPipelineStateCache.setDevice(pImpl->mDevice);
    pImpl->mDepthStencilStateCache.setDevice(pImpl->mDevice);
    pImpl->mSamplerStateCache.setDevice(pImpl->mDevice);
}

MetalDriver::~MetalDriver() noexcept {
    delete pImpl;
    [pImpl->mDevice release];
}

#if !defined(NDEBUG)
void MetalDriver::debugCommand(const char *methodName) {
#if METAL_DEBUG_COMMANDS == 1
    utils::slog.d << methodName << utils::io::endl;
#endif
}
#endif

void MetalDriver::beginFrame(int64_t monotonic_clock_ns, uint32_t frameId) {
    pImpl->mPool = [[NSAutoreleasePool alloc] init];
    pImpl->mCurrentCommandBuffer = [pImpl->mCommandQueue commandBuffer];
}

void MetalDriver::setPresentationTime(int64_t monotonic_clock_ns) {

}

void MetalDriver::endFrame(uint32_t frameId) {

}

void MetalDriver::flush(int dummy) {

}

void MetalDriver::createVertexBuffer(Driver::VertexBufferHandle vbh, uint8_t bufferCount,
        uint8_t attributeCount, uint32_t vertexCount, Driver::AttributeArray attributes,
        Driver::BufferUsage usage) {
    // todo: make use of usage
    construct_handle<MetalVertexBuffer>(mHandleMap, vbh, pImpl->mDevice, bufferCount,
            attributeCount, vertexCount, attributes);
}

void MetalDriver::createIndexBuffer(Driver::IndexBufferHandle ibh, Driver::ElementType elementType,
        uint32_t indexCount, Driver::BufferUsage usage) {
    auto elementSize = (uint8_t) getElementTypeSize(elementType);
    construct_handle<MetalIndexBuffer>(mHandleMap, ibh, pImpl->mDevice, elementSize, indexCount);
}

void MetalDriver::createTexture(Driver::TextureHandle th, Driver::SamplerType target, uint8_t levels,
        Driver::TextureFormat format, uint8_t samples, uint32_t width, uint32_t height,
        uint32_t depth, Driver::TextureUsage usage) {
    construct_handle<MetalTexture>(mHandleMap, th, pImpl->mDevice, target, levels, format, samples,
            width, height, depth, usage);
}

void MetalDriver::createSamplerBuffer(Driver::SamplerBufferHandle sbh, size_t size) {
    construct_handle<MetalSamplerBuffer>(mHandleMap, sbh, size);
}

void MetalDriver::createUniformBuffer(Driver::UniformBufferHandle ubh, size_t size,
        Driver::BufferUsage usage) {
    construct_handle<MetalUniformBuffer>(mHandleMap, ubh, pImpl->mDevice, size);
}

void MetalDriver::createRenderPrimitive(Driver::RenderPrimitiveHandle rph, int dummy) {
    construct_handle<MetalRenderPrimitive>(mHandleMap, rph);
}

void MetalDriver::createProgram(Driver::ProgramHandle rph, Program&& program) {
    construct_handle<MetalProgram>(mHandleMap, rph, pImpl->mDevice, program);
}

void MetalDriver::createDefaultRenderTarget(Driver::RenderTargetHandle rth, int dummy) {
    auto renderTarget = construct_handle<MetalRenderTarget>(mHandleMap, rth);
}

void MetalDriver::createRenderTarget(Driver::RenderTargetHandle rth,
        Driver::TargetBufferFlags targetBufferFlags, uint32_t width, uint32_t height,
        uint8_t samples, Driver::TextureFormat format, Driver::TargetBufferInfo color,
        Driver::TargetBufferInfo depth, Driver::TargetBufferInfo stencil) {
    auto renderTarget = construct_handle<MetalRenderTarget>(mHandleMap, rth, width, height);

    if (color.handle) {
        auto colorTexture = handle_cast<MetalTexture>(mHandleMap, color.handle);
        renderTarget->color = colorTexture->texture;
    } else if (targetBufferFlags & TargetBufferFlags::COLOR) {
        utils::slog.d << "Need color.";
        assert(false);
    }

    if (depth.handle) {
        auto depthTexture = handle_cast<MetalTexture>(mHandleMap, depth.handle);
        renderTarget->depth = depthTexture->texture;
    } else if (targetBufferFlags & TargetBufferFlags::DEPTH) {
        // Create a depth texture.
        // TODO: ownership is not clear here. Should we force Filament to always create a depth
        // texture ahead of time?
        MTLTextureDescriptor* depthTextureDesc =
                [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                   width:width
                                                                  height:height
                                                               mipmapped:NO];
        depthTextureDesc.usage = MTLTextureUsageRenderTarget;
        depthTextureDesc.resourceOptions = MTLResourceStorageModePrivate;
        renderTarget->depth = [pImpl->mDevice newTextureWithDescriptor:depthTextureDesc];
    }

    ASSERT_POSTCONDITION(
            !stencil.handle && !(targetBufferFlags & TargetBufferFlags::STENCIL),
            "Stencil buffer not supported.");
}

void MetalDriver::createFence(Driver::FenceHandle, int dummy) {

}

void MetalDriver::createSwapChain(Driver::SwapChainHandle sch, void* nativeWindow, uint64_t flags) {
    construct_handle<MetalSwapChain>(mHandleMap, sch, pImpl->mDevice, (CAMetalLayer*) nativeWindow);
}

void MetalDriver::createStreamFromTextureId(Driver::StreamHandle, intptr_t externalTextureId,
        uint32_t width, uint32_t height) {

}

Driver::VertexBufferHandle MetalDriver::createVertexBufferSynchronous() noexcept {
    return alloc_handle<MetalVertexBuffer, HwVertexBuffer>();
}

Driver::IndexBufferHandle MetalDriver::createIndexBufferSynchronous() noexcept {
    return alloc_handle<MetalIndexBuffer, HwIndexBuffer>();
}

Driver::TextureHandle MetalDriver::createTextureSynchronous() noexcept {
    return alloc_handle<MetalTexture, HwTexture>();
}

Driver::SamplerBufferHandle MetalDriver::createSamplerBufferSynchronous() noexcept {
    return alloc_handle<MetalSamplerBuffer, HwSamplerBuffer>();
}

Driver::UniformBufferHandle MetalDriver::createUniformBufferSynchronous() noexcept {
    return alloc_handle<MetalUniformBuffer, HwUniformBuffer>();
}

Driver::RenderPrimitiveHandle MetalDriver::createRenderPrimitiveSynchronous() noexcept {
    return alloc_handle<MetalRenderPrimitive, HwRenderPrimitive>();
}

Driver::ProgramHandle MetalDriver::createProgramSynchronous() noexcept {
    return alloc_handle<MetalProgram, HwProgram>();
}

Driver::RenderTargetHandle MetalDriver::createDefaultRenderTargetSynchronous() noexcept {
    return alloc_handle<MetalRenderTarget, HwRenderTarget>();
}

Driver::RenderTargetHandle MetalDriver::createRenderTargetSynchronous() noexcept {
    return alloc_handle<MetalRenderTarget, HwRenderTarget>();
}

Driver::FenceHandle MetalDriver::createFenceSynchronous() noexcept {
    return {};
}

Driver::SwapChainHandle MetalDriver::createSwapChainSynchronous() noexcept {
    return alloc_handle<MetalSwapChain, HwSwapChain>();
}

Driver::StreamHandle MetalDriver::createStreamFromTextureIdSynchronous() noexcept {
    return {};
}

void MetalDriver::destroyVertexBuffer(Driver::VertexBufferHandle vbh) {
    if (vbh) {
        destruct_handle<MetalVertexBuffer>(mHandleMap, vbh);
    }
}

void MetalDriver::destroyIndexBuffer(Driver::IndexBufferHandle ibh) {
    if (ibh) {
        destruct_handle<MetalIndexBuffer>(mHandleMap, ibh);
    }
}

void MetalDriver::destroyRenderPrimitive(Driver::RenderPrimitiveHandle rph) {
    if (rph) {
        destruct_handle<MetalRenderPrimitive>(mHandleMap, rph);
    }
}

void MetalDriver::destroyProgram(Driver::ProgramHandle ph) {
    if (ph) {
        destruct_handle<MetalProgram>(mHandleMap, ph);
    }
}

void MetalDriver::destroySamplerBuffer(Driver::SamplerBufferHandle sbh) {
    if (sbh) {
        destruct_handle<MetalSamplerBuffer>(mHandleMap, sbh);
    }
}

void MetalDriver::destroyUniformBuffer(Driver::UniformBufferHandle ubh) {
    if (ubh) {
        destruct_handle<MetalUniformBuffer>(mHandleMap, ubh);
    }
}

void MetalDriver::destroyTexture(Driver::TextureHandle th) {
    if (th) {
        destruct_handle<MetalTexture>(mHandleMap, th);
    }
}

void MetalDriver::destroyRenderTarget(Driver::RenderTargetHandle rth) {
    if (rth) {
        // todo: see comment inside of createRenderTarget for why this is difficult at the moment.
        // destruct_handle<MetalRenderTarget>(mHandleMap, rth);
    }
}

void MetalDriver::destroySwapChain(Driver::SwapChainHandle sch) {
    if (sch) {
        destruct_handle<MetalSwapChain>(mHandleMap, sch);
    }
}

void MetalDriver::destroyStream(Driver::StreamHandle sh) {
    // no-op
}

void MetalDriver::terminate() {
    [pImpl->mCommandQueue release];
    [pImpl->mDriverPool drain];
}

Driver::StreamHandle MetalDriver::createStream(void* stream) {
    return {};
}

void MetalDriver::setStreamDimensions(Driver::StreamHandle stream, uint32_t width,
        uint32_t height) {
    // no-op
}

int64_t MetalDriver::getStreamTimestamp(Driver::StreamHandle stream) {
    return 0;
}

void MetalDriver::updateStreams(driver::DriverApi* driver) {
    // no-op
}

void MetalDriver::destroyFence(Driver::FenceHandle fh) {
    // no-op
}

Driver::FenceStatus MetalDriver::wait(Driver::FenceHandle fh, uint64_t timeout) {
    return FenceStatus::ERROR;
}

bool MetalDriver::isTextureFormatSupported(Driver::TextureFormat format) {
    return getMetalFormat(format) != MTLPixelFormatInvalid;
}

bool MetalDriver::isRenderTargetFormatSupported(Driver::TextureFormat format) {
    // todo: There's probably a stricter set of supported render target formats.
    return getMetalFormat(format) != MTLPixelFormatInvalid;
}

bool MetalDriver::isFrameTimeSupported() {
    return false;
}

void MetalDriver::updateVertexBuffer(Driver::VertexBufferHandle vbh, size_t index,
        Driver::BufferDescriptor&& data, uint32_t byteOffset, uint32_t byteSize) {
    auto* vb = handle_cast<MetalVertexBuffer>(mHandleMap, vbh);
    memcpy(vb->buffers[index].contents, data.buffer, data.size);
}

void MetalDriver::updateIndexBuffer(Driver::IndexBufferHandle ibh, Driver::BufferDescriptor&& data,
        uint32_t byteOffset, uint32_t byteSize) {
    auto* ib = handle_cast<MetalIndexBuffer>(mHandleMap, ibh);
    memcpy(ib->buffer.contents, data.buffer, data.size);
}

void MetalDriver::update2DImage(Driver::TextureHandle th, uint32_t level, uint32_t xoffset,
        uint32_t yoffset, uint32_t width, uint32_t height, Driver::PixelBufferDescriptor&& data) {
    auto tex = handle_cast<MetalTexture>(mHandleMap, th);
    tex->load2DImage(level, xoffset, yoffset, width, height, data);
    scheduleDestroy(std::move(data));
}

void MetalDriver::updateCubeImage(Driver::TextureHandle th, uint32_t level,
        Driver::PixelBufferDescriptor&& data, Driver::FaceOffsets faceOffsets) {
    auto tex = handle_cast<MetalTexture>(mHandleMap, th);
    tex->loadCubeImage(data, faceOffsets, level);
    scheduleDestroy(std::move(data));
}

void MetalDriver::setExternalImage(Driver::TextureHandle th, void* image) {

}

void MetalDriver::setExternalStream(Driver::TextureHandle th, Driver::StreamHandle sh) {

}

void MetalDriver::generateMipmaps(Driver::TextureHandle th) {

}

void MetalDriver::updateUniformBuffer(Driver::UniformBufferHandle ubh,
        Driver::BufferDescriptor&& data) {
    auto buffer = handle_cast<MetalUniformBuffer>(mHandleMap, ubh);
    if (buffer->buffer) {
        memcpy(buffer->buffer.contents, data.buffer, data.size);
    } else {
        assert(buffer->cpuBuffer);
        memcpy(buffer->cpuBuffer, data.buffer, data.size);
    }
    scheduleDestroy(std::move(data));
}

void MetalDriver::updateSamplerBuffer(Driver::SamplerBufferHandle sbh,
        SamplerBuffer&& samplerBuffer) {
    auto sb = handle_cast<MetalSamplerBuffer>(mHandleMap, sbh);
    // todo: enable a move here.
    *sb->sb = samplerBuffer;
}

void MetalDriver::beginRenderPass(Driver::RenderTargetHandle rth,
        const Driver::RenderPassParams& params) {
    // Metal clears the entire attachment without respect to viewport or scissor.
    // todo: might need to clear the scissor area manually via a draw call if we need that
    // functionality.

    auto renderTarget = handle_cast<MetalRenderTarget>(mHandleMap, rth);

    MTLRenderPassDescriptor* descriptor = [MTLRenderPassDescriptor renderPassDescriptor];

    // Color

    if (renderTarget->isDefaultRenderTarget) {
        // Lazily acquire the next drawable, if we haven't already acquired it for this frame.
        if (!pImpl->mCurrentDrawable) {
            pImpl->mCurrentDrawable = [pImpl->mCurrentSurface->layer nextDrawable];
        }
        if (pImpl->mCurrentDrawable == nil) {
            utils::slog.e << "Could not obtain drawable." << utils::io::endl;
            utils::debug_trap();
        }
        // todo: refactor this a bit
        descriptor.colorAttachments[0].texture = pImpl->mCurrentDrawable.texture;
        pImpl->mCurrentSurfacePixelFormat = pImpl->mCurrentDrawable.texture.pixelFormat;
    } else {
        descriptor.colorAttachments[0].texture = renderTarget->color;
        pImpl->mCurrentSurfacePixelFormat = renderTarget->color.pixelFormat;
    }

    descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
    descriptor.colorAttachments[0].clearColor = MTLClearColorMake(
            params.clearColor.r, params.clearColor.g, params.clearColor.b, params.clearColor.a
    );

    // Depth

    if (renderTarget->isDefaultRenderTarget) {
        descriptor.depthAttachment.texture = pImpl->mCurrentSurface->depthTexture;
        pImpl->mCurrentDepthPixelFormat = pImpl->mCurrentSurface->depthTexture.pixelFormat;
    } else {
        descriptor.depthAttachment.texture = renderTarget->depth;
        if (renderTarget->depth) {
            pImpl->mCurrentDepthPixelFormat = renderTarget->depth.pixelFormat;
        } else {
            pImpl->mCurrentDepthPixelFormat = MTLPixelFormatInvalid;
        }
    }

    descriptor.depthAttachment.clearDepth = params.clearDepth;

    pImpl->mCurrentCommandEncoder =
            [pImpl->mCurrentCommandBuffer renderCommandEncoderWithDescriptor:descriptor];

    viewport(params.left, params.bottom, params.width, params.height);

    // Metal requires a new command encoder for each render pass, and they cannot be reused.
    // We must bind certain states for each command encoder, so we dirty the states here
    // to force a rebinding at the first the draw call of this pass.
    for (auto& i : pImpl->mUniformState) {
        i.invalidate();
    }
    pImpl->mPipelineState.invalidate();
    pImpl->mDepthStencilState.invalidate();
    pImpl->mCullModeState.invalidate();
    pImpl->mViewportState.invalidate();
    pImpl->mSamplersDirty = true;
    pImpl->mTexturesDirty = true;
}

void MetalDriver::endRenderPass(int dummy) {
    [pImpl->mCurrentCommandEncoder endEncoding];

    // Command encoders are one time use. Set it to nullptr so we don't accidentally use it again..
    pImpl->mCurrentCommandEncoder = nullptr;
}

void MetalDriver::discardSubRenderTargetBuffers(Driver::RenderTargetHandle rth,
        Driver::TargetBufferFlags targetBufferFlags, uint32_t left, uint32_t bottom, uint32_t width,
        uint32_t height) {

}

void MetalDriver::resizeRenderTarget(Driver::RenderTargetHandle rth, uint32_t width,
        uint32_t height) {

}

void MetalDriver::setRenderPrimitiveBuffer(Driver::RenderPrimitiveHandle rph,
        Driver::VertexBufferHandle vbh, Driver::IndexBufferHandle ibh, uint32_t enabledAttributes) {
    auto primitive = handle_cast<MetalRenderPrimitive>(mHandleMap, rph);
    auto vertexBuffer = handle_cast<MetalVertexBuffer>(mHandleMap, vbh);
    auto indexBuffer = handle_cast<MetalIndexBuffer>(mHandleMap, ibh);
    primitive->setBuffers(vertexBuffer, indexBuffer, enabledAttributes);
}

void MetalDriver::setRenderPrimitiveRange(Driver::RenderPrimitiveHandle rph,
        Driver::PrimitiveType pt, uint32_t offset, uint32_t minIndex, uint32_t maxIndex,
        uint32_t count) {
    auto primitive = handle_cast<MetalRenderPrimitive>(mHandleMap, rph);
    primitive->type = pt;
    primitive->offset = offset * primitive->indexBuffer->elementSize;
    primitive->count = count;
    primitive->minIndex = minIndex;
    primitive->maxIndex = maxIndex > minIndex ? maxIndex : primitive->maxVertexCount - 1;
}

void MetalDriver::setViewportScissor(int32_t left, int32_t bottom, uint32_t width,
        uint32_t height) {

}

void MetalDriver::makeCurrent(Driver::SwapChainHandle schDraw, Driver::SwapChainHandle schRead) {
    ASSERT_PRECONDITION_NON_FATAL(schDraw == schRead,
                                  "Metal driver does not support distinct draw/read swap chains.");
    auto* swapChain = handle_cast<MetalSwapChain>(mHandleMap, schDraw);

    pImpl->mCurrentSurface = swapChain;
}

void MetalDriver::commit(Driver::SwapChainHandle sch) {
    [pImpl->mCurrentCommandBuffer presentDrawable:pImpl->mCurrentDrawable];
    [pImpl->mCurrentCommandBuffer commit];
    pImpl->mCurrentDrawable = nil;
    [pImpl->mPool drain];
}

void MetalDriver::viewport(ssize_t left, ssize_t bottom, size_t width, size_t height) {
    ASSERT_PRECONDITION(pImpl->mCurrentCommandEncoder != nullptr, "mCurrentCommandEncoder is null");
    // Flip the viewport, because Metal's screen space is vertically flipped that of Filament's.
    pImpl->mViewportState.updateState(MTLViewport {
        .originX = static_cast<double>(left),
        .originY = pImpl->mCurrentSurface->surfaceHeight - static_cast<double>(bottom) -
                static_cast<double>(height),
        .height = static_cast<double>(height),
        .width = static_cast<double>(width),
        .znear = 0.0,
        .zfar = 1.0
    });
}

void MetalDriver::bindUniformBuffer(size_t index, Driver::UniformBufferHandle ubh) {
    pImpl->mUniformState[index].updateState(UniformBufferState {
        .bound = true,
        .ubh = ubh,
        .offset = 0
    });
}

void MetalDriver::bindUniformBufferRange(size_t index, Driver::UniformBufferHandle ubh,
        size_t offset, size_t size) {
    pImpl->mUniformState[index].updateState(UniformBufferState {
        .bound = true,
        .ubh = ubh,
        .offset = offset
    });
}

void MetalDriver::bindSamplers(size_t index, Driver::SamplerBufferHandle sbh) {
    auto sb = handle_cast<MetalSamplerBuffer>(mHandleMap, sbh);
    pImpl->mSamplerBindings[index] = sb;
}

void MetalDriver::insertEventMarker(const char* string, size_t len) {

}

void MetalDriver::pushGroupMarker(const char* string, size_t len) {

}

void MetalDriver::popGroupMarker(int dummy) {

}

void MetalDriver::readPixels(Driver::RenderTargetHandle src, uint32_t x, uint32_t y, uint32_t width,
        uint32_t height, Driver::PixelBufferDescriptor&& data) {

}

void MetalDriver::readStreamPixels(Driver::StreamHandle sh, uint32_t x, uint32_t y, uint32_t width,
        uint32_t height, Driver::PixelBufferDescriptor&& data) {

}

void MetalDriver::blit(Driver::TargetBufferFlags buffers, Driver::RenderTargetHandle dst,
        int32_t dstLeft, int32_t dstBottom, uint32_t dstWidth, uint32_t dstHeight,
        Driver::RenderTargetHandle src, int32_t srcLeft, int32_t srcBottom, uint32_t srcWidth,
        uint32_t srcHeight) {

}

void MetalDriver::draw(Driver::PipelineState ps, Driver::RenderPrimitiveHandle rph) {
    ASSERT_PRECONDITION(pImpl->mCurrentCommandEncoder != nullptr,
            "Attempted to draw without a valid command encoder.");
    auto primitive = handle_cast<MetalRenderPrimitive>(mHandleMap, rph);
    auto program = handle_cast<MetalProgram>(mHandleMap, ps.program);
    const auto& rs = ps.rasterState;

    // Pipeline state
    metal::PipelineState pipelineState {
        .vertexFunction = program->vertexFunction,
        .fragmentFunction = program->fragmentFunction,
        .vertexDescription = primitive->vertexDescription,
        .colorAttachmentPixelFormat = pImpl->mCurrentSurfacePixelFormat,
        .depthAttachmentPixelFormat = pImpl->mCurrentDepthPixelFormat,
        .blendState = BlendState {
            .blendingEnabled = rs.hasBlending(),
            .rgbBlendOperation = getMetalBlendOperation(rs.blendEquationRGB),
            .alphaBlendOperation = getMetalBlendOperation(rs.blendEquationAlpha),
            .sourceRGBBlendFactor = getMetalBlendFactor(rs.blendFunctionSrcRGB),
            .sourceAlphaBlendFactor = getMetalBlendFactor(rs.blendFunctionSrcAlpha),
            .destinationRGBBlendFactor = getMetalBlendFactor(rs.blendFunctionDstRGB),
            .destinationAlphaBlendFactor = getMetalBlendFactor(rs.blendFunctionDstAlpha)
        }
    };
    pImpl->mPipelineState.updateState(pipelineState);
    if (pImpl->mPipelineState.stateChanged()) {
        id<MTLRenderPipelineState> pipeline =
                pImpl->mPipelineStateCache.getOrCreateState(pipelineState);
        assert(pipeline != nil);
        [pImpl->mCurrentCommandEncoder setRenderPipelineState:pipeline];
    }

    // Cull mode
    MTLCullMode cullMode = getMetalCullMode(rs.culling);
    pImpl->mCullModeState.updateState(cullMode);
    if (pImpl->mCullModeState.stateChanged()) {
        [pImpl->mCurrentCommandEncoder setCullMode:cullMode];
    }

    // Viewport
    if (pImpl->mViewportState.stateChanged()) {
        // [pImpl->mCurrentCommandEncoder setViewport:pImpl->mViewportState.getState()];
    }

    // Set the depth-stencil state, if a state change is needed.
    DepthStencilState depthState {
        .compareFunction = getMetalCompareFunction(rs.depthFunc),
        .depthWriteEnabled = rs.depthWrite,
    };
    pImpl->mDepthStencilState.updateState(depthState);
    if (pImpl->mDepthStencilState.stateChanged()) {
        id<MTLDepthStencilState> state =
                pImpl->mDepthStencilStateCache.getOrCreateState(depthState);
        assert(state != nil);
        [pImpl->mCurrentCommandEncoder setDepthStencilState:state];
    }

    // Bind any uniform buffers that have changed since the last draw call.
    for (uint32_t i = 0; i < VERTEX_BUFFER_START; i++) {
        auto& thisUniform = pImpl->mUniformState[i];
        if (thisUniform.stateChanged()) {
            const auto& uniformState = thisUniform.getState();
            if (!uniformState.bound) {
                continue;
            }

            const auto* uniform = handle_const_cast<MetalUniformBuffer>(mHandleMap,
                    uniformState.ubh);

            // We have no way of knowing which uniform buffers will be used by which shader stage
            // so for now, bind the uniform buffer to both the vertex and fragment stages.

            if (uniform->buffer) {
                [pImpl->mCurrentCommandEncoder setVertexBuffer:uniform->buffer
                                                        offset:uniformState.offset
                                                       atIndex:i];

                [pImpl->mCurrentCommandEncoder setFragmentBuffer:uniform->buffer
                                                          offset:uniformState.offset
                                                         atIndex:i];
            } else {
                assert(uniform->cpuBuffer);
                uint8_t* bytes = static_cast<uint8_t*>(uniform->cpuBuffer) + uniformState.offset;
                [pImpl->mCurrentCommandEncoder setVertexBytes:bytes
                                                       length:(uniform->size - uniformState.offset)
                                                      atIndex:i];
                [pImpl->mCurrentCommandEncoder setFragmentBytes:bytes
                                                         length:(uniform->size - uniformState.offset)
                                                        atIndex:i];
            }
        }
    }

    uint8_t offset = 0;
    for (uint8_t bufferIdx = 0; bufferIdx < NUM_SAMPLER_BINDINGS; bufferIdx++) {
        MetalSamplerBuffer* metalSb = pImpl->mSamplerBindings[bufferIdx];
        if (!metalSb) {
            continue;
        }
        SamplerBuffer* sb = metalSb->sb.get();
        for (uint8_t samplerIdx = 0; samplerIdx < sb->getSize(); samplerIdx++) {
            const SamplerBuffer::Sampler* sampler = sb->getBuffer() + samplerIdx;
            if (!sampler->t) {
                continue;
            }
            uint8_t binding, group;
            if (program->samplerBindings.getSamplerBinding(bufferIdx, samplerIdx, &binding,
                    &group)) {

                const auto metalTexture = handle_const_cast<MetalTexture>(mHandleMap, sampler->t);
                auto& textureSlot = pImpl->mBoundTextures[binding - offset];
                if (textureSlot != metalTexture->texture) {
                    textureSlot = metalTexture->texture;
                    pImpl->mTexturesDirty = true;
                }

                id<MTLSamplerState> samplerState =
                        pImpl->mSamplerStateCache.getOrCreateState(sampler->s);
                auto& samplerSlot = pImpl->mBoundSamplers[binding - offset];
                if (samplerSlot != samplerState) {
                    samplerSlot = samplerState;
                    pImpl->mSamplersDirty = true;
                }
            }
        }
    }

    // Similar to uniforms, we can't tell which stage will use the textures / samplers, so bind
    // to both the vertex and fragment stages.

    NSRange range {
        .length = NUM_SAMPLER_BINDINGS,
        .location = 0
    };
    if (pImpl->mTexturesDirty) {
        [pImpl->mCurrentCommandEncoder setFragmentTextures:pImpl->mBoundTextures
                                                 withRange:range];
        [pImpl->mCurrentCommandEncoder setVertexTextures:pImpl->mBoundTextures
                                               withRange:range];
        pImpl->mTexturesDirty = false;
    }

    if (pImpl->mSamplersDirty) {
        [pImpl->mCurrentCommandEncoder setFragmentSamplerStates:pImpl->mBoundSamplers
                                                      withRange:range];
        [pImpl->mCurrentCommandEncoder setVertexSamplerStates:pImpl->mBoundSamplers
                                                    withRange:range];
        pImpl->mSamplersDirty = false;
    }

    // Bind the vertex buffers.
    NSRange bufferRange = NSMakeRange(VERTEX_BUFFER_START, primitive->buffers.size());
    [pImpl->mCurrentCommandEncoder setVertexBuffers:primitive->buffers.data()
                                            offsets:primitive->offsets.data()
                                          withRange:bufferRange];

    MetalIndexBuffer* indexBuffer = primitive->indexBuffer;

    [pImpl->mCurrentCommandEncoder drawIndexedPrimitives:getMetalPrimitiveType(primitive->type)
                                              indexCount:primitive->count
                                               indexType:getIndexType(indexBuffer->elementSize)
                                             indexBuffer:indexBuffer->buffer
                                       indexBufferOffset:0];
}

} // namespace metal
} // namespace driver

// explicit instantiation of the Dispatcher
template class ConcreteDispatcher<driver::metal::MetalDriver>;

} // namespace filament

#pragma clang diagnostic pop