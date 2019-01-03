/*
 * Copyright (C) 2019 The Android Open Source Project
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

#include "FrameGraph.h"

#include <utils/Panic.h>

namespace filament {

FrameGraphPassBase::FrameGraphPassBase(const char* name) noexcept
        : mName(name) {
}

FrameGraphPassBase::~FrameGraphPassBase() = default;

void FrameGraphPassBase::read(FrameGraphResource const& resource) {
    mReads.push_back(resource.mId);
}

void FrameGraphPassBase::write(FrameGraphResource const& resource) {
    mRefCount++;
    resource.mWriter = this;
    resource.mWriterCount++;
}

// ------------------------------------------------------------------------------------------------

FrameGraphBuilder::FrameGraphBuilder(FrameGraph& fg, FrameGraphPassBase* pass) noexcept
    : mFrameGraph(fg), mPass(pass) {
}

FrameGraphResource* FrameGraphBuilder::getResource(FrameGraphResourceHandle input) {
    FrameGraph& frameGraph = mFrameGraph;
    const std::vector<uint32_t>& resourcesIds = frameGraph.mResourcesIds;
    std::vector<FrameGraphResource>& registry = frameGraph.mResourceRegistry;

    if (!ASSERT_POSTCONDITION_NON_FATAL(input.isValid(),
            "attempting to use invalid resource handle")) {
        return nullptr;
    }

    uint32_t handle = input.getHandle();
    assert(handle < resourcesIds.size());
    uint32_t id = resourcesIds[handle];

    if (!ASSERT_POSTCONDITION_NON_FATAL(id != FrameGraph::INVALID_ID,
            "attempting to use invalid resource id")) {
        return nullptr;
    }

    assert(id < registry.size());
    return &registry[id];
}

FrameGraphResourceHandle FrameGraphBuilder::createTexture(
        const char* name, FrameGraphResource::TextureDesc const& desc) noexcept {
    FrameGraph& frameGraph = mFrameGraph;
    FrameGraphResource& resource = frameGraph.createResource(name);
    mPass->write(resource);
    return frameGraph.createHandle(resource);
}

FrameGraphResourceHandle FrameGraphBuilder::read(FrameGraphResourceHandle const& input) {
    FrameGraphResource* resource = getResource(input);
    if (!resource) {
        return {};
    }
    mPass->read(*resource);
    return input;
}

FrameGraphResourceHandle FrameGraphBuilder::write(FrameGraphResourceHandle const& output) {
    FrameGraphResource* resource = getResource(output);
    if (!resource) {
        return {};
    }
    mPass->write(*resource);

    /*
     * We invalidate and rename handles that are writen into, to avoid undefined order
     * access to the resources.
     *
     * e.g. forbidden graphs
     *
     *         +-> [R1] -+
     *        /           \
     *  (A) -+             +-> (A)
     *        \           /
     *         +-> [R2] -+        // failure when setting R2 from (A)
     *
     */

    // rename handle
    FrameGraph& frameGraph = mFrameGraph;
    frameGraph.mResourcesIds[output.getHandle()] = FrameGraph::INVALID_ID;
    return frameGraph.createHandle(*resource);
}

// ------------------------------------------------------------------------------------------------

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() = default;

void FrameGraph::present(FrameGraphResourceHandle input) {
    struct Dummy {
    };
    addPass<Dummy>("Present",
            [&input](FrameGraphBuilder& builder, Dummy& data) {
                builder.read(input);
            },
            [](FrameGraphPassResources const& resources, Dummy const& data) {
            });
}

FrameGraphResource& FrameGraph::createResource(const char* name) {
    auto& registry = mResourceRegistry;
    uint32_t id = (uint32_t)registry.size();
    registry.emplace_back(name, id);
    return registry.back();
}

FrameGraphResourceHandle FrameGraph::createHandle(FrameGraphResource const& resource) {
    FrameGraphResourceHandle handle{ (uint32_t)mResourcesIds.size() };
    mResourcesIds.push_back(resource.mId);
    return handle;
}

FrameGraph& FrameGraph::compile() noexcept {
    auto& registry = mResourceRegistry;
    for (auto& pass : mFrameGraphPasses) {
        auto const& reads = pass->getReadResources();
        for (uint32_t id : reads) {
            FrameGraphResource& resource = registry[id];

            // add a reference for each pass that reads from this resource
            resource.mRefCount++;

            // figure out which is the first pass to need this resource
            resource.mFirst = resource.mFirst ? resource.mFirst : pass.get();

            // figure out which is the last pass to need this resource
            resource.mLast = pass.get();
        }
    }

    std::vector<FrameGraphResource*> stack;
    stack.reserve(registry.size());
    for (FrameGraphResource& resource : registry) {
        if (resource.mRefCount == 0) {
            stack.push_back(&resource);
        }
    }

    while (!stack.empty()) {
        FrameGraphResource* resource = stack.back();
        stack.pop_back();

        // by construction, this resource cannot have more than one producer because
        // - two unrelated passes can't write in the same resource
        // - passes that read + write into the resource imply that the refcount is not null

        assert(resource->mWriterCount == 1);

        FrameGraphPassBase* const writer = resource->mWriter;

        assert(writer->mRefCount >= 1);

        if (--writer->mRefCount == 0) {
            // this pass is culled
            auto const& reads = writer->getReadResources();
            for (uint32_t id : reads) {
                resource = &registry[id];
                resource->mRefCount--;
                if (resource->mRefCount == 0) {
                    stack.push_back(resource);
                }
            }
        }
    }

    for (FrameGraphResource& resource : registry) {
        if (resource.mWriter->mRefCount) {
            if (resource.mRefCount == 0) {
                // we have a resource with a ref-count of zero, written to by an active (non-culled)
                // pass. Is this allowable? This would happen for instance if a pass were to write
                // in two buffers, but only one of them when consumed. The pass couldn't be
                // discarded, but the unused resource would have to be created.
                // TODO: should we allow this?
            }

            assert(resource.mFirst);
            assert(resource.mLast);

            resource.mFirst->mDevirtualize.push_back(resource.mId);
            resource.mLast->mDestroy.push_back(resource.mId);
        }
    }

    return *this;
}

void FrameGraph::execute() noexcept {
    auto const& resources = mResources;
    auto& registry = mResourceRegistry;
    for (auto& pass : mFrameGraphPasses) {
        if (!pass->mRefCount) continue;

        for (uint32_t id : pass->mDevirtualize) {
            // TODO: create concrete resources
            FrameGraphResource& resource = registry[id];
        }

        pass->execute(resources);

        for (uint32_t id : pass->mDestroy) {
            // TODO: delete concrete resources
            FrameGraphResource& resource = registry[id];
        }
    }

    // reset the frame graph state
    mFrameGraphPasses.clear();
    mResourcesIds.clear();
    mResourceRegistry.clear();
}

} // namespace filament
