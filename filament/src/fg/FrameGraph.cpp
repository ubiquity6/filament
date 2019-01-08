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
#include <utils/Log.h>

using namespace utils;

namespace filament {

FrameGraphPassBase::FrameGraphPassBase(const char* name, uint32_t id) noexcept
        : mName(name), mId(id) {
}

FrameGraphPassBase::~FrameGraphPassBase() = default;

void FrameGraphPassBase::read(FrameGraphResourceHandle const& resource) {
    // just record that we're reading from this resource (at the given version)
    mReads.push_back({ resource.getIndex(), resource.getVersion() });
}

void FrameGraphPassBase::write(FrameGraphResourceHandle& resource) {
    // invalidate existing handles to this resource
    resource.write();
    // record the write
    mWrites.push_back({ resource.getIndex(), resource.getVersion() });
}

// ------------------------------------------------------------------------------------------------

FrameGraphBuilder::FrameGraphBuilder(FrameGraph& fg, FrameGraphPassBase* pass) noexcept
    : mFrameGraph(fg), mPass(pass) {
}

FrameGraphResourceHandle* FrameGraphBuilder::getResource(FrameGraphResource input) {
    FrameGraph& frameGraph = mFrameGraph;
    auto& registry = frameGraph.mResourceHandles;

    if (!ASSERT_POSTCONDITION_NON_FATAL(input.isValid(),
            "using an uninitialized resource handle")) {
        return nullptr;
    }

    assert(input.index < registry.size());
    auto& resource = registry[input.index];

    if (!ASSERT_POSTCONDITION_NON_FATAL(input.version == resource.getVersion(),
            "using an invalid resource handle (version=%u) for resource=\"%s\" (id=%u, version=%u)",
            input.version, resource.getName(), resource.getIndex(), resource.getVersion())) {
        return nullptr;
    }

    return &resource;
}

FrameGraphResource FrameGraphBuilder::createTexture(
        const char* name, CreateFlags flags, FrameGraphResourceHandle::TextureDesc const& desc) noexcept {
    FrameGraph& frameGraph = mFrameGraph;
    FrameGraphResourceHandle& resource = frameGraph.createResource(name);
    switch (flags) {
        case UNKNOWN:
            // we just create a resource, but we don't use it in this pass
            break;
        case READ:
            mPass->read(resource);
            break;
        case WRITE:
            mPass->write(resource);
            break;
    }
    return { resource.getIndex(), resource.getVersion() };
}

FrameGraphResource FrameGraphBuilder::read(FrameGraphResource const& input) {
    FrameGraphResourceHandle* resource = getResource(input);
    if (!resource) {
        return {};
    }
    mPass->read(*resource);
    return input;
}

FrameGraphResource FrameGraphBuilder::write(FrameGraphResource const& output) {
    FrameGraphResourceHandle* resource = getResource(output);
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

    return { resource->getIndex(), resource->getVersion() };
}

// ------------------------------------------------------------------------------------------------

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() = default;

bool FrameGraph::isValid(FrameGraphResource handle) const noexcept {
    if (!handle.isValid()) return false;
    auto const& registry = mResourceHandles;
    assert(handle.index < registry.size());
    auto& resource = registry[handle.index];
    return handle.version == resource.getVersion();
}

void FrameGraph::moveResource(FrameGraphResource from, FrameGraphResource to) {
    // FIXME: what checks need to happen?
    mAliases.push_back({from, to});
}

void FrameGraph::present(FrameGraphResource input) {
    struct Dummy {
    };
    addPass<Dummy>("Present",
            [&input](FrameGraphBuilder& builder, Dummy& data) {
                builder.read(input);
            },
            [](FrameGraphPassResources const& resources, Dummy const& data) {
            });
}

FrameGraphResourceHandle& FrameGraph::createResource(const char* name) {
    auto& registry = mResourceHandles;
    uint16_t id = (uint16_t)registry.size();
    registry.emplace_back(name, id);
    return registry.back();
}

FrameGraph& FrameGraph::compile() noexcept {
    auto& registry = mResourceHandles;
    mResourceRegistry.reserve(registry.size());

    // create the sub-resources
    for (FrameGraphResourceHandle& resource : registry) {
        mResourceRegistry.emplace_back(resource.getName());
        resource.mSubResource = &mResourceRegistry.back();
    }

    // remap them
    for (auto alias : mAliases) {
        registry[alias.to.index].mSubResource = registry[alias.from.index].mSubResource;
    }

    for (auto const& pass : mFrameGraphPasses) {
        // compute passes reference counts (i.e. resources we're writing to)
        pass->mRefCount = (uint32_t)pass->mWrites.size();

        // compute resources reference counts (i.e. resources we're reading from), and first/last users
        for (auto handle : pass->mReads) {
            FrameGraphResourceHandle::Resource* subResource = registry[handle.index].mSubResource;

            // add a reference for each pass that reads from this resource
            subResource->readerCount++;

            // figure out which is the first pass to need this resource
            subResource->first = subResource->first ? subResource->first : pass.get();

            // figure out which is the last pass to need this resource
            subResource->last = pass.get();
        }

        // set the writers
        for (auto handle : pass->mWrites) {
            FrameGraphResourceHandle::Resource* subResource = registry[handle.index].mSubResource;
            subResource->writer = pass.get();
            subResource->writerCount++;
        }
    }

    // cull passes and resources...
    std::vector<FrameGraphResourceHandle::Resource*> stack;
    stack.reserve(registry.size());
    for (auto& resource : mResourceRegistry) {
        if (resource.readerCount == 0) {
            stack.push_back(&resource);
        }
    }

    while (!stack.empty()) {
        FrameGraphResourceHandle::Resource const* const pSubResource = stack.back();
        stack.pop_back();

        // by construction, this resource cannot have more than one producer because
        // - two unrelated passes can't write in the same resource
        // - passes that read + write into the resource imply that the refcount is not null

        assert(pSubResource->writerCount <= 1);

        // if a resource doesn't have a writer and is not imported, then we the graph is not
        // set correctly. For now we ignore this.
        if (!pSubResource->writer) {
            slog.d << "resource \"" << pSubResource->name << "\" is never written!" << io::endl;
            continue; // TODO: this shouldn't happen in a valid graph
        }

        FrameGraphPassBase* const writer = pSubResource->writer;
        assert(writer->mRefCount >= 1);
        if (--writer->mRefCount == 0) {
            // this pass is culled
            auto const& reads = writer->mReads;
            for (auto id : reads) {
                FrameGraphResourceHandle::Resource* r = registry[id.index].mSubResource;
                if (--r->readerCount == 0) {
                    stack.push_back(r);
                }
            }
        }
    }

    for (size_t index = 0, c = mResourceRegistry.size() ; index < c ; index++) {
        auto& resource = mResourceRegistry[index];
        assert(!resource.first == !resource.last);
        if (resource.readerCount && resource.first && resource.last) {
            resource.first->mDevirtualize.push_back((uint16_t)index);
            resource.last->mDestroy.push_back((uint16_t)index);
        }
    }

    return *this;
}

void FrameGraph::execute() noexcept {
    auto const& resources = mResources;
    for (auto& pass : mFrameGraphPasses) {
        if (!pass->mRefCount) continue;

        for (size_t id : pass->mDevirtualize) {
            // TODO: create concrete resources
        }

        pass->execute(resources);

        for (uint32_t id : pass->mDestroy) {
            // TODO: delete concrete resources
        }
    }

    // reset the frame graph state
    mFrameGraphPasses.clear();
    mResourceHandles.clear();
    mAliases.clear();
}

void FrameGraph::export_graphviz(utils::io::ostream& out) {
    bool removeCulled = false;

    out << "digraph framegraph {\n";
    out << "rankdir = LR\n";
    out << "bgcolor = black\n";
    out << "node [shape=rectangle, fontname=\"helvetica\", fontsize=10]\n\n";

    auto const& registry = mResourceHandles;
    auto const& frameGraphPasses = mFrameGraphPasses;

    // declare passes
    for (auto const& pass : frameGraphPasses) {
        if (removeCulled && !pass->mRefCount) continue;
        out << "\"P" << pass->getId() << "\" [label=\"" << pass->getName()
               << "\\nrefs: " << pass->mRefCount
               << "\\nseq: " << pass->getId()
               << "\", style=filled, fillcolor="
               << (pass->mRefCount ? "darkorange" : "darkorange4") << "]\n";
    }

    // declare resources nodes
    out << "\n";
    for (auto const& resource : registry) {
        auto subresource = registry[resource.getIndex()].mSubResource;
        if (removeCulled && !subresource->readerCount) continue;
        for (size_t version = 0; version <= resource.getVersion(); version++) {
            out << "\"R" << resource.getIndex() << "_" << version << "\""
                   "[label=\"" << resource.getName() << "\\n(version: " << version << ")"
                   "\\nid:" << resource.getIndex() <<
                   "\\nrefs:" << subresource->readerCount
                   <<"\""
                   ", style=filled, fillcolor="
                   << (subresource->readerCount ? "skyblue" : "skyblue4") << "]\n";
        }
    }

    // connect passes to resources
    out << "\n";
    for (auto const& pass : frameGraphPasses) {
        if (removeCulled && !pass->mRefCount) continue;
        out << "P" << pass->getId() << " -> { ";
        for (auto const& writer : pass->mWrites) {
            auto resource = registry[writer.index].mSubResource;
            if (removeCulled && !resource->readerCount) continue;
            out << "R" << writer.index << "_" << writer.version << " ";
        }
        out << "} [color=red2]\n";
    }

    // connect resources to passes
    out << "\n";
    for (auto const& resource : registry) {
        auto subresource = registry[resource.getIndex()].mSubResource;
        if (removeCulled && !subresource->readerCount) continue;
        for (size_t version = 0; version <= resource.getVersion(); version++) {
            out << "R" << resource.getIndex() << "_" << version << " -> { ";

            // who reads us...
            for (auto const& pass : frameGraphPasses) {
                if (removeCulled && !pass->mRefCount) continue;
                for (auto const& read : pass->mReads) {
                    if (read.index == resource.getIndex() && read.version == version ) {
                        out << "P" << pass->getId() << " ";
                    }
                }
            }
            out << "} [color=lightgreen]\n";
        }
    }

    // aliases...
    if (!mAliases.empty()) {
        out << "\n";
        for (auto const& alias : mAliases) {
            out << "R" << alias.from.index << "_" << alias.from.version << " -> ";
            out << "R" << alias.to.index << "_" << alias.to.version;
            out << "[color=yellow, style=dashed]\n";
        }
    }

    out << "}" << utils::io::endl;
}

} // namespace filament
