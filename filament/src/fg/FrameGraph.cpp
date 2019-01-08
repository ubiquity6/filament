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

void FrameGraphPassBase::read(FrameGraphResource const& resource) {
    // just record that we're reading from this resource (at the given version)
    mReads.push_back({ resource.getId(), resource.getVersion() });
}

void FrameGraphPassBase::write(FrameGraphResource const& resource) {
    resource.mWriters.push_back(this);
}

// ------------------------------------------------------------------------------------------------

FrameGraphBuilder::FrameGraphBuilder(FrameGraph& fg, FrameGraphPassBase* pass) noexcept
    : mFrameGraph(fg), mPass(pass) {
}

FrameGraphResource* FrameGraphBuilder::getResource(FrameGraphResourceHandle input) {
    FrameGraph& frameGraph = mFrameGraph;
    auto& registry = frameGraph.mResourceRegistry;

    if (!ASSERT_POSTCONDITION_NON_FATAL(input.isValid(),
            "using an uninitialized resource handle")) {
        return nullptr;
    }

    assert(input.index < registry.size());
    auto& resource = registry[input.index];

    if (!ASSERT_POSTCONDITION_NON_FATAL(input.version == resource.getVersion(),
            "using an invalid resource handle (version=%u) for resource=\"%s\" (id=%u, version=%u)",
            input.version, resource.getName(), resource.getId(), resource.getVersion())) {
        return nullptr;
    }

    return &resource;
}

FrameGraphResourceHandle FrameGraphBuilder::createTexture(
        const char* name, CreateFlags flags, FrameGraphResource::TextureDesc const& desc) noexcept {
    FrameGraph& frameGraph = mFrameGraph;
    FrameGraphResource& resource = frameGraph.createResource(name);
    switch (flags) {
        case UNKNOWN:
            // we just create a resource, but we don't use it in this pass
            break;
        case READ:
            mPass->read(resource);
            break;
        case WRITE:
            // since we're writing to this resource, we increase its version
            resource.mId.version = 1;
            mPass->write(resource);
            break;
    }
    return { resource.getId(), resource.getVersion() };
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
    resource->mId.version++; // this invalidates previous handles
    return { resource->getId(), resource->getVersion() };
}

// ------------------------------------------------------------------------------------------------

FrameGraph::FrameGraph() = default;

FrameGraph::~FrameGraph() = default;

bool FrameGraph::isValid(FrameGraphResourceHandle handle) const noexcept {
    if (!handle.isValid()) return false;
    auto const& registry = mResourceRegistry;
    assert(handle.index < registry.size());
    auto& resource = registry[handle.index];
    return handle.version == resource.getVersion();
}

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
    uint16_t id = (uint16_t)registry.size();
    registry.emplace_back(name, id);
    return registry.back();
}

FrameGraph& FrameGraph::compile() noexcept {
    auto& registry = mResourceRegistry;

    // compute passes reference counts (i.e. resources we're writing to)
    for (FrameGraphResource& resource : registry) {
        for (FrameGraphPassBase* writer : resource.mWriters) {
            writer->mRefCount++;
        }
    }

    // compute resources reference counts (i.e. resources we're reading from), and first/last users
    for (auto const& pass : mFrameGraphPasses) {
        for (auto handle : pass->mReads) {
            FrameGraphResource& resource = registry[handle.index];

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
        FrameGraphResource const* const pResource = stack.back();
        stack.pop_back();

        // by construction, this resource cannot have more than one producer because
        // - two unrelated passes can't write in the same resource
        // - passes that read + write into the resource imply that the refcount is not null

        assert(pResource->mWriters.size() <= 1);

        // if a resource doesn't have a writer and is not imported, then we the graph is not
        // set correctly. For now we ignore this.
        if (pResource->mWriters.empty()) {
            slog.d << "resource \"" << pResource->getName() << "\" is never written!" << io::endl;
            continue; // TODO: this shouldn't happen in a valid graph
        }

        FrameGraphPassBase* const writer = pResource->mWriters.back();
        assert(writer->mRefCount >= 1);
        if (--writer->mRefCount == 0) {
            // this pass is culled
            auto const& reads = writer->mReads;
            for (auto id : reads) {
                FrameGraphResource& r = registry[id.index];
                if (--r.mRefCount == 0) {
                    stack.push_back(&r);
                }
            }
        }
    }

    for (FrameGraphResource& resource : registry) {
        if (resource.mWriters.empty()) continue; // TODO: this shouldn't happen in a valid graph
        FrameGraphPassBase* const writer = resource.mWriters.back();
        if (writer->mRefCount) {
            if (resource.mRefCount == 0) {
                // we have a resource with a ref-count of zero, written to by an active (non-culled)
                // pass. Is this allowable? This would happen for instance if a pass were to write
                // in two buffers, but only one of them when consumed. The pass couldn't be
                // discarded, but the unused resource would have to be created.
                // TODO: should we allow this?
            } else {
                assert(resource.mFirst);
                assert(resource.mLast);
                resource.mFirst->mDevirtualize.push_back(resource.getId());
                resource.mLast->mDestroy.push_back(resource.getId());
            }
        }
    }

    return *this;
}

void FrameGraph::execute() noexcept {
    auto const& resources = mResources;
    auto& registry = mResourceRegistry;
    for (auto& pass : mFrameGraphPasses) {
        if (!pass->mRefCount) continue;

        for (size_t id : pass->mDevirtualize) {
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
    mResourceRegistry.clear();
}

void FrameGraph::export_graphviz(utils::io::ostream& out) {
    bool removeCulled = false;

    out << "digraph framegraph {\n";
    out << "rankdir = LR\n";
    out << "bgcolor = black\n";
    out << "node [shape=rectangle, fontname=\"helvetica\", fontsize=10]\n\n";

    auto const& registry = mResourceRegistry;

    for (auto const& pass : mFrameGraphPasses) {
        if (removeCulled && !pass->mRefCount) continue;
        out << "\"P" << pass->getId() << "\" [label=\"" << pass->getName()
               << "\\nrefs: " << pass->mRefCount
               << "\\nseq: " << pass->getId()
               << "\", style=filled, fillcolor="
               << (pass->mRefCount ? "darkorange" : "darkorange4") << "]\n";
    }

    out << "\n";
    for (auto const& resource : registry) {
        if (removeCulled && !resource.mRefCount) continue;
        for (size_t i = 0; i <= resource.getVersion(); i++) {
            out << "\"R" << resource.getId() << "_" << i << "\""
                   "[label=\"" << resource.getName() << "\\n(version: " << i << ")"
                   "\\nid:" << resource.getId() <<
                   "\\nrefs:" << resource.mRefCount
                   <<"\""
                   ", style=filled, fillcolor="
                   << (resource.mRefCount ? "skyblue" : "skyblue4") << "]\n";
        }
    }

    out << "\n";
    for (auto const& resource : registry) {
        if (removeCulled && !resource.mRefCount) continue;
        auto const& writers = resource.mWriters;
        size_t version = 1;
        for (auto const& writer : writers) {
            if (removeCulled && !writer->mRefCount) continue;
            out << "P" << writer->getId() << " -> { ";
            out << "R" << resource.getId() << "_" << version++ << " ";
            out << "} [color=red2]\n";
        }
    }

    out << "\n";
    for (auto const& resource : registry) {
        if (removeCulled && !resource.mRefCount) continue;
        for (size_t i = 0; i <= resource.getVersion(); i++) {
            out << "R" << resource.getId() << "_" << i << " -> { ";
            // who reads us...
            for (auto const& pass : mFrameGraphPasses) {
                if (removeCulled && !pass->mRefCount) continue;
                for (auto const& pass_id : pass->mReads) {
                    if (pass_id.index == resource.getId() && pass_id.version == i ) {
                        out << "P" << pass->getId() << " ";
                    }
                }
            }
            out << "} [color=lightgreen]\n";
        }
    }
    out << "}" << utils::io::endl;
}

} // namespace filament
