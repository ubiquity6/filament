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

#include <filaflat/ChunkContainer.h>

#include <filament/MaterialEnums.h>

#include <backend/DriverEnums.h>

#include <matdbg/TextWriter.h>
#include <matdbg/ShaderInfo.h>

#include <sstream>
#include <iomanip>

#include "CommonWriter.h"

using namespace filament;
using namespace backend;
using namespace filaflat;
using namespace filamat;
using namespace std;
using namespace utils;

namespace filament {
namespace matdbg {

static const int alignment = 24;

static string arraySizeToString(uint64_t size) {
    if (size > 1) {
        string s = "[";
        s += size;
        s += "]";
        return s;
    }
    return "";
}

template<typename T, typename V>
static void printChunk(ostream& text, const ChunkContainer& container, ChunkType type,
        const char* title, bool last = false) {
    T value;
    if (read(container, type, reinterpret_cast<V*>(&value))) {
        text << "\"" << title << "\": \"" << toString(value) << (last? "\"" : "\",") << endl;
    }
}

static void printBoolChunk(ostream& text, const ChunkContainer& container, ChunkType type,
        const char* title, bool last = false) {
    bool value;
    if (read(container, type, &value)) {
        text << "\"" << title << "\": " << (value? "true" : "false") << (last? "" : ",") << endl;
    }
}

static void printFloatChunk(ostream& text, const ChunkContainer& container, ChunkType type,
        const char* title, bool last = false) {
    float value;
    if (read(container, type, &value)) {
        text << "\"" << title << "\": " << setprecision(2) << value << (last? "" : ",") << endl;
    }
}

static void printUint32Chunk(ostream& text, const ChunkContainer& container,
        ChunkType type, const char* title, bool last = false) {
    uint32_t value;
    if (read(container, type, &value)) {
        text << "\"" << title << "\": \"" << value << (last? "\"" : "\",") << endl;
    }
}

static void printStringChunk(ostream& text, const ChunkContainer& container,
        ChunkType type, const char* title, bool last = false) {
    CString value;
    if (read(container, type, &value)) {
        text << "\"" << title << "\": \"" << value.c_str() << (last? "\"" : "\",") << endl;
    }
}

static bool printMaterial(ostream& text, const ChunkContainer& container) {
    text << "\"material\": {" << endl;

    uint32_t version;
    if (read(container, MaterialVersion, &version)) {
        text << "\"version\": \"" << version << "\"," << endl;
    }

    printUint32Chunk(text, container, PostProcessVersion, "postProcessVersion");

    CString name;
    if (read(container, MaterialName, &name)) {
        text << "\"name\": \"" << name.c_str() << "\"" << endl;
    }

    text << "}," << endl;

    text << "\"shading\": {" << endl;
    printChunk<Shading, uint8_t>(text, container, MaterialShading, "model");
    printChunk<VertexDomain, uint8_t>(text, container, MaterialVertexDomain, "vertexDomain");
    printChunk<Interpolation, uint8_t>(text, container, MaterialInterpolation, "interpolation");
    printBoolChunk(text, container, MaterialShadowMultiplier, "shadowMultiply");
    printBoolChunk(text, container, MaterialSpecularAntiAliasing, "specularAA");
    printFloatChunk(text, container, MaterialSpecularAntiAliasingVariance, "variance");
    printFloatChunk(text, container, MaterialSpecularAntiAliasingThreshold, "threshold");
    printBoolChunk(text, container, MaterialClearCoatIorChange, "clearCoatIORChange", true);

    text << "}," << endl;

    text << "\"rasterState\": {" << endl;
    printChunk<BlendingMode, uint8_t>(text, container, MaterialBlendingMode, "blending");
    printFloatChunk(text, container, MaterialMaskThreshold, "maskThreshold");
    printBoolChunk(text, container, MaterialColorWrite, "colorWrite");
    printBoolChunk(text, container, MaterialDepthWrite, "depthWrite");
    printBoolChunk(text, container, MaterialDepthTest, "depthTest");
    printBoolChunk(text, container, MaterialDoubleSided, "doubleSided");
    printChunk<CullingMode, uint8_t>(text, container, MaterialCullingMode, "culling");
    printChunk<TransparencyMode, uint8_t>(text, container, MaterialTransparencyMode, "transparency", true);

    text << "}," << endl;

    uint32_t requiredAttributes;
    if (read(container, MaterialRequiredAttributes, &requiredAttributes)) {
        AttributeBitset bitset;
        bitset.setValue(requiredAttributes);

        if (bitset.count() > 0) {
            text << "\"attributes\": [" << endl;
            for (size_t i = 0, j = 0, n = bitset.size(), m = bitset.count(); i < n; i++) {
                if (bitset.test(i)) {
                    text << "\"" << toString(static_cast<VertexAttribute>(i)) << ((++j < m) ? "\"," : "\"") << endl;
                }
            }
            text << "]" << endl;
        }
    }

    return true;
}

static bool printParametersInfo(ostream& text, const ChunkContainer& container) {
    if (!container.hasChunk(ChunkType::MaterialUib)) {
        return true;
    }

    Unflattener uib(
            container.getChunkStart(ChunkType::MaterialUib),
            container.getChunkEnd(ChunkType::MaterialUib));

    CString name;
    if (!uib.read(&name)) {
        return false;
    }

    uint64_t uibCount;
    if (!uib.read(&uibCount)) {
        return false;
    }

    Unflattener sib(
            container.getChunkStart(ChunkType::MaterialSib),
            container.getChunkEnd(ChunkType::MaterialSib));

    if (!sib.read(&name)) {
        return false;
    }

    uint64_t sibCount;
    if (!sib.read(&sibCount)) {
        return false;
    }

    if (uibCount == 0 && sibCount == 0) {
        return true;
    }

    text << ", \"parameters\": [" << endl;

    for (uint64_t i = 0; i < uibCount; i++) {
        CString fieldName;
        uint64_t fieldSize;
        uint8_t fieldType;
        uint8_t fieldPrecision;

        if (!uib.read(&fieldName)) {
            return false;
        }

        if (!uib.read(&fieldSize)) {
            return false;
        }

        if (!uib.read(&fieldType)) {
            return false;
        }

        if (!uib.read(&fieldPrecision)) {
            return false;
        }

        //skip 'injected' parameters

        if (fieldName.c_str()[0] != '_') {
            text << ((i > 0) ? ",{" : "{") << endl;
            text << "\"name\": \"" << fieldName.c_str() << "\"," << endl;
            text << "\"type\": \"" << toString(UniformType(fieldType)) << "\"," << endl;
            text << "\"size\": \"" << fieldSize << "\"," << endl;
            text << "\"precision\": \"" << toString(Precision(fieldPrecision)) << "\"" << endl;
            text << "}" << endl;
        }
    }

    for (uint64_t i = 0; i < sibCount; i++) {
        CString fieldName;
        uint8_t fieldType;
        uint8_t fieldFormat;
        uint8_t fieldPrecision;
        bool fieldMultisample;

        if (!sib.read(&fieldName)) {
            return false;
        }

        if (!sib.read(&fieldType)) {
            return false;
        }

        if (!sib.read(&fieldFormat))
            return false;

        if (!sib.read(&fieldPrecision)) {
            return false;
        }

        if (!sib.read(&fieldMultisample)) {
            return false;
        }

        text << ((uibCount > 0 || i > 0) ? ",{" : "{") << endl;
        text << "\"name\": \"" << fieldName.c_str() << "\"," << endl;
        text << "\"type\": \"" << toString(SamplerType(fieldType)) << "\"," << endl;
        text << "\"format\": \"" << toString(SamplerFormat(fieldFormat)) << "\"," << endl;
        text << "\"precision\": \"" << toString(Precision(fieldPrecision)) << "\"" << endl;
        text << "}" << endl;
    }
    text << "]" << endl;

    return true;
}

// Unpack a 64 bit integer into a string
inline utils::CString typeToString(uint64_t v) {
    uint8_t* raw = (uint8_t*) &v;
    char str[9];
    for (size_t i = 0; i < 8; i++) {
        str[7 - i] = raw[i];
    }
    str[8] = '\0';
    return utils::CString(str, strnlen(str, 8));
}

static void printChunks(ostream& text, const ChunkContainer& container) {
    text << "Chunks:" << endl;

    text << "    " << setw(9) << left << "Name ";
    text << setw(7) << right << "Size" << endl;

    size_t count = container.getChunkCount();
    for (size_t i = 0; i < count; i++) {
        auto chunk = container.getChunk(i);
        text << "    " << typeToString(chunk.type).c_str() << " ";
        text << setw(7) << right << chunk.desc.size << endl;
    }
}

static void printShaderInfo(ostream& text, const vector<ShaderInfo>& info) {
    for (uint64_t i = 0; i < info.size(); ++i) {
        const auto& item = info[i];
        text << "    #";
        text << setw(4) << left << i;
        text << setw(6) << left << toString(item.shaderModel);
        text << " ";
        text << setw(2) << left << toString(item.pipelineStage);
        text << " ";
        text << "0x" << hex << setfill('0') << setw(2)
             << right << (int) item.variant;
        text << setfill(' ') << dec << endl;
    }
    text << endl;
}

static bool printGlslInfo(ostream& text, const ChunkContainer& container) {
    vector<ShaderInfo> info;
    info.resize(getShaderCount(container, ChunkType::MaterialGlsl));
    if (!getGlShaderInfo(container, info.data())) {
        return false;
    }
    text << "GLSL shaders:" << endl;
    printShaderInfo(text, info);
    return true;
}

static bool printVkInfo(ostream& text, const ChunkContainer& container) {
    vector<ShaderInfo> info;
    info.resize(getShaderCount(container, ChunkType::MaterialSpirv));
    if (!getVkShaderInfo(container, info.data())) {
        return false;
    }
    text << "Vulkan shaders:" << endl;
    printShaderInfo(text, info);
    return true;
}

static bool printMetalInfo(ostream& text, const ChunkContainer& container) {
    vector<ShaderInfo> info;
    info.resize(getShaderCount(container, ChunkType::MaterialMetal));
    if (!getMetalShaderInfo(container, info.data())) {
        return false;
    }
    text << "Metal shaders:" << endl;
    printShaderInfo(text, info);
    return true;
}

bool TextWriter::writeMaterialInfo(const filaflat::ChunkContainer& container) {
    ostringstream text;
    text << "{" << endl;

    if (!printMaterial(text, container)) {
        return false;
    }
    if (!printParametersInfo(text, container)) {
        return false;
    }
    // if (!printGlslInfo(text, container)) {
    //     return false;
    // }
    // if (!printVkInfo(text, container)) {
    //     return false;
    // }
    // if (!printMetalInfo(text, container)) {
    //     return false;
    // }

    //printChunks(text, container);

    text << endl;
    text << "}" << endl;

    mTextString = CString(text.str().c_str());
    return true;
}

const char* TextWriter::getString() const {
    return mTextString.c_str();
}

size_t TextWriter::getSize() const {
    return mTextString.size();
}

} // namespace matdbg
} // namespace filament
