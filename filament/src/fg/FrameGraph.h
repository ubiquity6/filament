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

#ifndef TNT_FILAMENT_FRAMEGRAPH_H
#define TNT_FILAMENT_FRAMEGRAPH_H


#include "FrameGraphResourceHandle.h"

#include <utils/Log.h>

#include <vector>

/*
 * A somewhat generic frame graph API.
 *
 * The design is largely inspired from Yuriy O'Donnell 2017 GDC talk
 * "FrameGraph: Extensible Rendering Architecture in Frostbite"
 *
 */

namespace filament {

class FrameGraph;
class FrameGraphBuilder;
class FrameGraphPassBase;
class FrameGraphResource;
class FrameGraphPassResources;

// ------------------------------------------------------------------------------------------------

class FrameGraphPassBase {
public:
    // TODO: use something less heavy than a std::vector<>
    using ResourceList = std::vector<FrameGraphResourceHandle>;

    FrameGraphPassBase(FrameGraphPassBase const&) = delete;
    FrameGraphPassBase& operator = (FrameGraphPassBase const&) = delete;
    virtual ~FrameGraphPassBase();

    const char* getName() const noexcept { return mName; }
    uint32_t getId() const noexcept { return mId; }

    // for FrameGraphBuilder
    void read(FrameGraphResource const& resource);
    void write(FrameGraphResource const& resource);

protected:
    FrameGraphPassBase(const char* name, uint32_t id) noexcept;

private:
    friend class FrameGraph;
    virtual void execute(FrameGraphPassResources const& resources) noexcept = 0;

    // constants
    const char* mName = nullptr;            // our name
    uint32_t mId = 0;                       // a unique id (only for debugging)

    // resources we're reading from
    ResourceList mReads;                    // set by the builder

    // computed during compile()
    std::vector<uint16_t> mDevirtualize;    // resources we need to create before executing
    std::vector<uint16_t> mDestroy;         // resources we need to destroy after executing
    uint32_t mRefCount = 0;                 // count resources that have a reference to us
};

// ------------------------------------------------------------------------------------------------

template <typename Data, typename Execute>
class FrameGraphPass : public FrameGraphPassBase {
public:
    FrameGraphPass(const char* name, uint32_t id, Execute&& execute) noexcept
        : FrameGraphPassBase(name, id), mExecute(std::forward<Execute>(execute)) {
    }

    Data const& getData() const noexcept { return mData; }
    Data& getData() noexcept { return mData; }

private:
    void execute(FrameGraphPassResources const& resources) noexcept final {
        mExecute(resources, mData);
    }

    Execute mExecute;
    Data mData;
};

// ------------------------------------------------------------------------------------------------

class FrameGraphResource {
public:
    struct TextureDesc {
        // TODO: descriptor for textures and render targets
    };

    FrameGraphResource(const char* name, uint16_t index) noexcept : mName(name), mId{ index, 0 } {}

    // disallow copy ctor
    FrameGraphResource(FrameGraphResource const&) = delete;
    FrameGraphResource& operator = (FrameGraphResource const&) = delete;

    // but allow moves
    FrameGraphResource(FrameGraphResource&&) noexcept = default;
    FrameGraphResource& operator = (FrameGraphResource&&) noexcept = default;


    const char* getName() const noexcept { return mName; }

    // a unique id for this resource
    uint16_t getId() const noexcept { return mId.index; }
    uint16_t getVersion() const noexcept { return mId.version; }

private:
    friend class FrameGraphPassBase;
    friend class FrameGraph;
    friend class FrameGraphBuilder;

    // constants
    const char* mName = nullptr;
    FrameGraphResourceHandle mId;

    // set by the builder
    mutable std::vector<FrameGraphPassBase*> mWriters;

    // computed during compile()
    FrameGraphPassBase* mFirst = nullptr;
    FrameGraphPassBase* mLast = nullptr;
    uint32_t mRefCount = 0;
};

// ------------------------------------------------------------------------------------------------

class FrameGraphPassResources {
public:
};

// ------------------------------------------------------------------------------------------------

class FrameGraphBuilder {
public:
    FrameGraphBuilder(FrameGraph& fg, FrameGraphPassBase* pass) noexcept;
    FrameGraphBuilder(FrameGraphBuilder const&) = delete;
    FrameGraphBuilder& operator = (FrameGraphBuilder const&) = delete;

    // create a resource
    enum CreateFlags : uint32_t {
        UNKNOWN, READ, WRITE
    };
    FrameGraphResourceHandle createTexture(
            const char* name,
            CreateFlags flags,
            FrameGraphResource::TextureDesc const& desc) noexcept;

    // read from a resource (i.e. add a reference to that resource)
    FrameGraphResourceHandle read(FrameGraphResourceHandle const& input /*, read-flags*/);

    // write to a resource (i.e. add a reference to the pass that's doing the writing))
    FrameGraphResourceHandle write(FrameGraphResourceHandle const& output  /*, write-flags*/);

private:
    FrameGraph& mFrameGraph;
    FrameGraphPassBase* mPass;
    FrameGraphResource* getResource(FrameGraphResourceHandle handle);
};

// ------------------------------------------------------------------------------------------------

class FrameGraph {
public:
    FrameGraph();
    FrameGraph(FrameGraph const&) = delete;
    FrameGraph& operator = (FrameGraph const&) = delete;
    ~FrameGraph();

    template <typename Data, typename Setup, typename Execute>
    FrameGraphPass<Data, Execute>& addPass(const char* name, Setup setup, Execute&& execute) {
        static_assert(sizeof(Execute) < 1024, "Execute() lambda is capturing too much data.");

        // create the FrameGraph pass (TODO: use special allocator)
        const uint32_t id = (uint32_t)mFrameGraphPasses.size();
        auto* pass = new FrameGraphPass<Data, Execute>(name, id, std::forward<Execute>(execute));

        FrameGraphBuilder builder(*this, pass);

        // call the setup code, which will declare used resources
        setup(builder, pass->getData());

        // record in our pass list
        mFrameGraphPasses.emplace_back(pass);

        // return a reference to the pass to the user
        return *pass;
    }

    void present(FrameGraphResourceHandle input);

    bool isValid(FrameGraphResourceHandle handle) const noexcept ;

    FrameGraph& compile() noexcept;
    void execute() noexcept;

    void export_graphviz(utils::io::ostream& out);

private:
    friend class FrameGraphBuilder;

    FrameGraphPassResources mResources;

    FrameGraphResource& createResource(const char* name);

    // list of frame graph passes
    std::vector<std::unique_ptr<FrameGraphPassBase>> mFrameGraphPasses;

    // frame graph concrete resources
    std::vector<FrameGraphResource> mResourceRegistry;
};


} // namespace filament

#endif //TNT_FILAMENT_FRAMEGRAPH_H
