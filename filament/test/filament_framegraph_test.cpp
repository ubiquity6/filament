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

#include <gtest/gtest.h>

#include "fg/FrameGraph.h"

using namespace filament;

TEST(FrameGraphTest, SimpleRenderPass) {

    FrameGraph fg;

    bool renderPassExecuted = false;

    struct RenderPassData {
        FrameGraphResourceHandle output;
    };

    auto& renderPass = fg.addPass<RenderPassData>("Render",
            [&](FrameGraphBuilder& builder, RenderPassData& data) {
                data.output = builder.createTexture("renderTarget", {});
                EXPECT_TRUE(data.output.isValid());
                EXPECT_TRUE(fg.isValid(data.output));
            },
            [&renderPassExecuted](FrameGraphPassResources const& resources, RenderPassData const& data) {
                renderPassExecuted = true;
            });

    fg.present(renderPass.getData().output);

    fg.compile().execute();

    EXPECT_TRUE(renderPassExecuted);
}


TEST(FrameGraphTest, SimpleRenderAndPostProcessPasses) {

    FrameGraph fg;

    bool renderPassExecuted = false;
    bool postProcessPassExecuted = false;

    struct RenderPassData {
        FrameGraphResourceHandle output;
    };

    auto& renderPass = fg.addPass<RenderPassData>("Render",
            [&](FrameGraphBuilder& builder, RenderPassData& data) {
                data.output = builder.createTexture("renderTarget", {});
                EXPECT_TRUE(data.output.isValid());
                EXPECT_TRUE(fg.isValid(data.output));
            },
            [&renderPassExecuted](FrameGraphPassResources const& resources, RenderPassData const& data) {
                renderPassExecuted = true;
            });


    struct PostProcessPassData {
        FrameGraphResourceHandle input;
        FrameGraphResourceHandle output;
    };

    auto& postProcessPass = fg.addPass<PostProcessPassData>("PostProcess",
            [&](FrameGraphBuilder& builder, PostProcessPassData& data) {
                data.input = builder.read(renderPass.getData().output);
                data.output = builder.write(data.input);
                EXPECT_TRUE(data.input.isValid());
                EXPECT_TRUE(data.output.isValid());
                EXPECT_FALSE(fg.isValid(data.input));
                EXPECT_TRUE(fg.isValid(data.output));
            },
            [&postProcessPassExecuted](FrameGraphPassResources const& resources, PostProcessPassData const& data) {
                postProcessPassExecuted = true;
            });

    fg.present(postProcessPass.getData().output);

    EXPECT_FALSE(fg.isValid(renderPass.getData().output));
    EXPECT_FALSE(fg.isValid(postProcessPass.getData().input));
    EXPECT_TRUE(fg.isValid(postProcessPass.getData().output));

    fg.compile();

    fg.execute();

    EXPECT_TRUE(renderPassExecuted);
    EXPECT_TRUE(postProcessPassExecuted);
}


TEST(FrameGraphTest, SimplePassCulling) {

    FrameGraph fg;

    bool renderPassExecuted = false;
    bool postProcessPassExecuted = false;
    bool culledPassExecuted = false;

    struct RenderPassData {
        FrameGraphResourceHandle output;
    };

    auto& renderPass = fg.addPass<RenderPassData>("Render",
            [&](FrameGraphBuilder& builder, RenderPassData& data) {
                data.output = builder.createTexture("renderTarget", {});
            },
            [&renderPassExecuted](FrameGraphPassResources const& resources, RenderPassData const& data) {
                renderPassExecuted = true;
            });


    struct PostProcessPassData {
        FrameGraphResourceHandle input;
        FrameGraphResourceHandle output;
    };

    auto& postProcessPass = fg.addPass<PostProcessPassData>("PostProcess",
            [&](FrameGraphBuilder& builder, PostProcessPassData& data) {
                data.input = builder.read(renderPass.getData().output);
                data.output = builder.createTexture("postprocess-renderTarget", {});
            },
            [&postProcessPassExecuted](FrameGraphPassResources const& resources, PostProcessPassData const& data) {
                postProcessPassExecuted = true;
            });


    struct CulledPassData {
        FrameGraphResourceHandle input;
        FrameGraphResourceHandle output;
    };

    auto& culledPass = fg.addPass<CulledPassData>("CulledPass",
            [&](FrameGraphBuilder& builder, CulledPassData& data) {
                data.input = builder.read(renderPass.getData().output);
                data.output = builder.createTexture("unused-rendertarget", {});
            },
            [&culledPassExecuted](FrameGraphPassResources const& resources, CulledPassData const& data) {
                culledPassExecuted = true;
            });

    fg.present(postProcessPass.getData().output);

    EXPECT_TRUE(fg.isValid(renderPass.getData().output));
    EXPECT_TRUE(fg.isValid(postProcessPass.getData().input));
    EXPECT_TRUE(fg.isValid(postProcessPass.getData().output));
    EXPECT_TRUE(fg.isValid(culledPass.getData().input));
    EXPECT_TRUE(fg.isValid(culledPass.getData().output));

    fg.compile();

    fg.execute();

    EXPECT_TRUE(renderPassExecuted);
    EXPECT_TRUE(postProcessPassExecuted);
    EXPECT_FALSE(culledPassExecuted);
}



TEST(FrameGraphTest, BadGraph) {

    /*
     * We invalidate and rename handles that are writen into, to avoid undefined order
     * access to the resources.
     *
     * e.g. forbidden graphs
     *
     *              +->[R1]-+
     *             /         \
     * [R0]->(A)--+           +-> (A)
     *             \         /
     *              +->[R2]-+        // failure when setting R2 from (A)
     *
     */

    FrameGraph fg;

    bool R0exec = false;
    bool R1exec = false;
    bool R2exec = false;

    struct R0Data {
        FrameGraphResourceHandle output;
    };

    auto& R0 = fg.addPass<R0Data>("R1",
            [&](FrameGraphBuilder& builder, R0Data& data) {
                data.output = builder.createTexture("A", {});
            },
            [&](FrameGraphPassResources const&, R0Data const&) {
                R0exec = true;
            });


    struct RData {
        FrameGraphResourceHandle input;
        FrameGraphResourceHandle output;
    };

    auto& R1 = fg.addPass<RData>("R1",
            [&](FrameGraphBuilder& builder, RData& data) {
                data.input = builder.read(R0.getData().output);
                data.output = builder.write(data.input);
            },
            [&](FrameGraphPassResources const&, RData const&) {
                R1exec = true;
            });

    fg.present(R1.getData().output);

    auto& R2 = fg.addPass<RData>("R2",
            [&](FrameGraphBuilder& builder, RData& data) {
                EXPECT_FALSE(fg.isValid(R0.getData().output));
            },
            [&](FrameGraphPassResources const&, RData const&) {
                R2exec = true;
            });

    EXPECT_FALSE(fg.isValid(R2.getData().output));

    fg.compile();

    fg.execute();

    EXPECT_TRUE(R0exec);
    EXPECT_TRUE(R1exec);
    EXPECT_FALSE(R2exec);
}

