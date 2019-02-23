// @flow

import {requireNativeComponent, DeviceEventEmitter} from 'react-native';
import {mat4, vec3} from 'gl-matrix';

import React from 'react';
var RCTFilamentView = requireNativeComponent('RCTFilamentView');

export default class FilamentView extends React.Component {
    engine;
    view;
    scene;
    triangle;
    //triangleInstance;
    transformManager;
    materialInstance;
    material;
    vb;
    ib;

    componentWillMount() 
    {       

        DeviceEventEmitter.addListener('filamentViewReady', (e) => {            
            console.log(`Filament view ready!`);
            
            initUtilities();
            initExtensions();

            console.log(`Filament extensions initialized`);
           

            this.view = Filament.getViewFromJavaPtr(e.viewPtr);
            this.engine = Filament.getEngineFromJavaPtr(e.enginePtr);
            this.scene = Filament.getSceneFromJavaPtr(e.scenePtr);            
            //this.materialInstance = Filament.getMaterialInstanceFromJavaPtr(e.materialInstancePtr);
            this.material = Filament.getMaterialFromJavaPtr(e.materialPtr);
            this.transformManager = this.engine.getTransformManager();
            //this.triangleInstance = Filament.getTransformInstance(this.transformManager, e.triangleId);
            this.view.setClearColor([0,0,0,1]);            

            //Geometry

            this.triangle = Filament.EntityManager.get().create();
            this.scene.addEntity(this.triangle);
    
            const TRIANGLE_POSITIONS = new Float32Array([
                1, 0,
                Math.cos(Math.PI * 2 / 3), Math.sin(Math.PI * 2 / 3),
                Math.cos(Math.PI * 4 / 3), Math.sin(Math.PI * 4 / 3),
            ]);
    
            const TRIANGLE_COLORS = new Uint32Array([0xffff0000, 0xff00ff00, 0xff0000ff]);
    
            this.vb = Filament.VertexBuffer.Builder()
                .vertexCount(3)
                .bufferCount(2)
                .attribute(VertexAttribute.POSITION, 0, Filament.VertexBuffer$AttributeType.FLOAT2, 0, 8)
                .attribute(VertexAttribute.COLOR, 1, Filament.VertexBuffer$AttributeType.UBYTE4, 0, 4)
                .normalized(VertexAttribute.COLOR)
                .build(this.engine);
    
            this.vb.setBufferAt(this.engine, 0, TRIANGLE_POSITIONS);
            this.vb.setBufferAt(this.engine, 1, TRIANGLE_COLORS);
    
            this.ib = Filament.IndexBuffer.Builder()
                .indexCount(3)
                .bufferType(Filament.IndexBuffer$IndexType.USHORT)
                .build(this.engine);
    
            this.ib.setBuffer(this.engine, new Uint16Array([0, 1, 2]));

            this.materialInstance = this.material.getDefaultInstance();
                
            var b = Filament.RenderableManager.Builder(1);
                b.boundingBox([[ -1, -1, -1 ], [ 1, 1, 1 ]]);
                b.geometry(0, Filament.RenderableManager$PrimitiveType.TRIANGLES, this.vb, this.ib);                
                b.material(0, this.materialInstance);                
                b.build(this.engine, this.triangle);

        });

        DeviceEventEmitter.addListener('filamentViewRender', (e) => { 
            const radians = Date.now() / 1000;            
            const transform = mat4.fromRotation(mat4.create(), radians, [0, 0, 1]);
            this.transformManager.setTransform(this.triangle, transform);
        });


    }

    render() {        
        return <RCTFilamentView style={{flex: 1}}></RCTFilamentView>
    }
}


function getBufferDescriptor(buffer) {
    if ('string' == typeof buffer || buffer instanceof String) {
        buffer = Filament.assets[buffer];
    }
    if (buffer.buffer instanceof ArrayBuffer) {
        buffer = Filament.Buffer(buffer);
    }
    return buffer;
}

function initUtilities() {

    console.assert = function(expression) {
        if(!expression) {
            console.log('Assert failed');
        }
    };


    // ---------------
    // Buffer Wrappers
    // ---------------

    // These wrappers make it easy for JavaScript clients to pass large swaths of data to Filament. They
    // copy the contents of the given typed array into the WASM heap, then return a low-level buffer
    // descriptor object. If the given array was taken from the WASM heap, then they create a temporary
    // copy because the input pointer becomes invalidated after allocating heap memory for the buffer
    // descriptor.

    /// Buffer ::function:: Constructs a [BufferDescriptor] by copying a typed array into the WASM heap.
    /// typedarray ::argument:: Data to consume (e.g. Uint8Array, Uint16Array, Float32Array)
    /// ::retval:: [BufferDescriptor]
    Filament.Buffer = function(typedarray) {
        console.assert(typedarray.buffer instanceof ArrayBuffer);
        console.assert(typedarray.byteLength > 0);
        /*
        if (Filament.HEAPU32.buffer == typedarray.buffer) {
            typedarray = new Uint8Array(typedarray);
        }
        const ta = typedarray;
        const bd = new Filament.driver$BufferDescriptor(ta);
        const uint8array = new Uint8Array(ta.buffer, ta.byteOffset, ta.byteLength);
        bd.getBytes().set(uint8array);*/

        return new Filament.driver$BufferDescriptor(typedarray);

        //return bd;
    };

    /// PixelBuffer ::function:: Constructs a [PixelBufferDescriptor] by copying a typed array into \
    /// the WASM heap.
    /// typedarray ::argument:: Data to consume (e.g. Uint8Array, Uint16Array, Float32Array)
    /// format ::argument:: [PixelDataFormat]
    /// datatype ::argument:: [PixelDataType]
    /// ::retval:: [PixelBufferDescriptor]
    Filament.PixelBuffer = function(typedarray, format, datatype) {
        console.assert(typedarray.buffer instanceof ArrayBuffer);
        console.assert(typedarray.byteLength > 0);
        if (Filament.HEAPU32.buffer == typedarray.buffer) {
            typedarray = new Uint8Array(typedarray);
        }
        const ta = typedarray;
        const bd = new Filament.driver$PixelBufferDescriptor(ta, format, datatype);
        const uint8array = new Uint8Array(ta.buffer, ta.byteOffset, ta.byteLength);
        bd.getBytes().set(uint8array);
        return bd;
    };

    /// CompressedPixelBuffer ::function:: Constructs a [PixelBufferDescriptor] for compressed texture
    /// data by copying a typed array into the WASM heap.
    /// typedarray ::argument:: Data to consume (e.g. Uint8Array, Uint16Array, Float32Array)
    /// cdatatype ::argument:: [CompressedPixelDataType]
    /// faceSize ::argument:: Number of bytes in each face (cubemaps only)
    /// ::retval:: [PixelBufferDescriptor]
    Filament.CompressedPixelBuffer = function(typedarray, cdatatype, faceSize) {
        console.assert(typedarray.buffer instanceof ArrayBuffer);
        console.assert(typedarray.byteLength > 0);
        faceSize = faceSize || typedarray.byteLength;
        if (Filament.HEAPU32.buffer == typedarray.buffer) {
            typedarray = new Uint8Array(typedarray);
        }
        const ta = typedarray;
        const bd = new Filament.driver$PixelBufferDescriptor(ta, cdatatype, faceSize, true);
        const uint8array = new Uint8Array(ta.buffer, ta.byteOffset, ta.byteLength);
        bd.getBytes().set(uint8array);
        return bd;
    };

    Filament._loadFilamesh = function(engine, buffer, definstance, matinstances) {
        matinstances = matinstances || {};
        const registry = new Filament.MeshReader$MaterialRegistry();
        for (var key in matinstances) {
            registry.set(key, matinstances[key]);
        }
        if (definstance) {
            registry.set("DefaultMaterial", definstance);
        }
        const mesh = Filament.MeshReader.loadMeshFromBuffer(engine, buffer, registry);
        const keys = registry.keys();
        for (var i = 0; i < keys.size(); i++) {
            const key = keys.get(i);
            const minstance = registry.get(key);
            matinstances[key] = minstance;
        }
        return {
            "renderable": mesh.renderable(),
            "vertexBuffer": mesh.vertexBuffer(),
            "indexBuffer": mesh.indexBuffer(),
        }
    }

    // ------------------
    // Geometry Utilities
    // ------------------

    /// IcoSphere ::class:: Utility class for constructing spheres (requires glMatrix).
    ///
    /// The constructor takes an integer subdivision level, with 0 being an icosahedron.
    ///
    /// Exposes three arrays as properties:
    ///
    /// - `icosphere.vertices` Float32Array of XYZ coordinates.
    /// - `icosphere.tangents` Uint16Array (interpreted as half-floats) encoding the surface orientation
    /// as quaternions.
    /// - `icosphere.triangles` Uint16Array with triangle indices.
    ///
    Filament.IcoSphere = function(nsubdivs) {
        const X = .525731112119133606;
        const Z = .850650808352039932;
        const N = 0.;
        this.vertices = new Float32Array([
            -X, +N, +Z, +X, +N, +Z, -X, +N, -Z, +X, +N, -Z ,
            +N, +Z, +X, +N, +Z, -X, +N, -Z, +X, +N, -Z, -X ,
            +Z, +X, +N, -Z, +X, +N, +Z, -X, +N, -Z, -X, +N ,
        ]);
        this.triangles = new Uint16Array([
            1,   4, 0,  4,  9,  0, 4,   5, 9, 8, 5,   4 ,  1,  8,  4 ,
            1,  10, 8, 10,  3,  8, 8,   3, 5, 3, 2,   5 ,  3,  7,  2 ,
            3,  10, 7, 10,  6,  7, 6,  11, 7, 6, 0,  11 ,  6,  1,  0 ,
            10,   1, 6, 11,  0,  9, 2,  11, 9, 5, 2,   9 , 11,  2,  7 ,
        ]);
        if (nsubdivs) {
            while (nsubdivs-- > 0) {
                this.subdivide();
            }
        }
        const nverts = this.vertices.length / 3;
        this.tangents = new Uint16Array(4 * nverts);
        for (var i = 0; i < nverts; ++i) {
            const src = this.vertices.subarray(i * 3, i * 3 + 3);
            const dst = this.tangents.subarray(i * 4, i * 4 + 4);
            const n = vec3.normalize(vec3.create(), src);
            const b = vec3.cross(vec3.create(), n, [0, 1, 0]);
            vec3.normalize(b, b);
            const t = vec3.cross(vec3.create(), b, n);
            const q = quat.fromMat3(quat.create(), [
                    t[0], t[1], t[2], b[0], b[1], b[2], n[0], n[1], n[2]]);
            vec4.packSnorm16(dst, q);
        }
    }

    Filament.IcoSphere.prototype.subdivide = function() {
        const srctris = this.triangles;
        const srcverts = this.vertices;
        const nsrctris = srctris.length / 3;
        const ndsttris = nsrctris * 4;
        const nsrcverts = srcverts.length / 3;
        const ndstverts = nsrcverts + nsrctris * 3;
        const dsttris = new Uint16Array(ndsttris * 3);
        const dstverts = new Float32Array(ndstverts * 3);
        dstverts.set(srcverts);
        var srcind = 0, dstind = 0, i3 = nsrcverts * 3, i4 = i3 + 3, i5 = i4 + 3;
        for (var tri = 0; tri < nsrctris; tri++, i3 += 9, i4 += 9, i5 += 9) {
            const i0 = srctris[srcind++] * 3;
            const i1 = srctris[srcind++] * 3;
            const i2 = srctris[srcind++] * 3;
            const v0 = srcverts.subarray(i0, i0 + 3);
            const v1 = srcverts.subarray(i1, i1 + 3);
            const v2 = srcverts.subarray(i2, i2 + 3);
            const v3 = dstverts.subarray(i3, i3 + 3);
            const v4 = dstverts.subarray(i4, i4 + 3);
            const v5 = dstverts.subarray(i5, i5 + 3);
            vec3.normalize(v3, vec3.add(v3, v0, v1));
            vec3.normalize(v4, vec3.add(v4, v1, v2));
            vec3.normalize(v5, vec3.add(v5, v2, v0));
            dsttris[dstind++] = i0 / 3;
            dsttris[dstind++] = i3 / 3;
            dsttris[dstind++] = i5 / 3;
            dsttris[dstind++] = i3 / 3;
            dsttris[dstind++] = i1 / 3;
            dsttris[dstind++] = i4 / 3;
            dsttris[dstind++] = i5 / 3;
            dsttris[dstind++] = i3 / 3;
            dsttris[dstind++] = i4 / 3;
            dsttris[dstind++] = i2 / 3;
            dsttris[dstind++] = i5 / 3;
            dsttris[dstind++] = i4 / 3;
        }
        this.triangles = dsttris;
        this.vertices = dstverts;
    }

    // ---------------
    // Math Extensions
    // ---------------

    function clamp(v, least, most) {
        return Math.max(Math.min(most, v), least);
    }

    /// packSnorm16 ::function:: Converts a float in [-1, +1] into a half-float.
    /// value ::argument:: float
    /// ::retval:: half-float
    Filament.packSnorm16 = function(value) {
        return Math.round(clamp(value, -1.0, 1.0) * 32767.0);
    }

    /// loadMathExtensions ::function:: Extends the [glMatrix](http://glmatrix.net/) math library.
    /// Filament does not require its clients to use glMatrix, but if its usage is detected then
    /// the [init] function will automatically call `loadMathExtensions`.
    /// This defines the following functions:
    /// - **vec4.packSnorm16** can be used to create half-floats (see [packSnorm16])
    /// - **mat3.fromRotation** now takes an arbitrary axis
    Filament.loadMathExtensions = function() {
        vec4.packSnorm16 = function(out, src) {
            out[0] = Filament.packSnorm16(src[0]);
            out[1] = Filament.packSnorm16(src[1]);
            out[2] = Filament.packSnorm16(src[2]);
            out[3] = Filament.packSnorm16(src[3]);
            return out;
        }
        // In gl-matrix, mat3 rotation assumes rotation about the Z axis, so here we add a function
        // to allow an arbitrary axis.
        const fromRotationZ = mat3.fromRotation;
        mat3.fromRotation = function(out, radians, axis) {
            if (axis) {
                return mat3.fromMat4(out, mat4.fromRotation(mat4.create(), radians, axis));
            }
            return fromRotationZ(out, radians);
        };
    };

    // ---------------
    // Texture helpers
    // ---------------

    Filament._createTextureFromKtx = function(ktxdata, engine, options) {
        options = options || {};
        const ktx = options['ktx'] || new Filament.KtxBundle(ktxdata);
        const rgbm = !!options['rgbm'];
        const srgb = !!options['srgb'];
        return Filament.KtxUtility$createTexture(engine, ktx, srgb, rgbm);
    };

    Filament._createIblFromKtx = function(ktxdata, engine, options) {
        options = options || {'rgbm': true};
        const iblktx = options['ktx'] = new Filament.KtxBundle(ktxdata);
        const ibltex = Filament._createTextureFromKtx(ktxdata, engine, options);
        const shstring = iblktx.getMetadata("sh");
        const shfloats = shstring.split(/\s/, 9 * 3).map(parseFloat);
        return Filament.IndirectLight.Builder()
            .reflections(ibltex)
            .irradianceSh(3, shfloats)
            .build(engine);
    };

    Filament._createTextureFromPng = function(pngdata, engine, options) {
        const Sampler = Filament.Texture$Sampler;
        const TextureFormat = Filament.Texture$InternalFormat;
        const PixelDataFormat = Filament.PixelDataFormat;

        options = options || {};
        const srgb = !!options['srgb'];
        const rgbm = !!options['rgbm'];
        const noalpha = !!options['noalpha'];
        const nomips = !!options['nomips'];

        const decodedpng = Filament.decodePng(pngdata, noalpha ? 3 : 4);

        var texformat, pbformat, pbtype;
        if (noalpha) {
            texformat = srgb ? TextureFormat.SRGB8 : TextureFormat.RGB8;
            pbformat = PixelDataFormat.RGB;
            pbtype = Filament.PixelDataType.UBYTE;
        } else {
            texformat = srgb ? TextureFormat.SRGB8_A8 : TextureFormat.RGBA8;
            pbformat = rgbm ? PixelDataFormat.RGBM : PixelDataFormat.RGBA;
            pbtype = Filament.PixelDataType.UBYTE;
        }

        const tex = Filament.Texture.Builder()
            .width(decodedpng.width)
            .height(decodedpng.height)
            .levels(nomips ? 1 : 0xff)
            .sampler(Sampler.SAMPLER_2D)
            .format(texformat)
            .rgbm(rgbm)
            .build(engine);

        const pixelbuffer = Filament.PixelBuffer(decodedpng.data.getBytes(), pbformat, pbtype);
        tex.setImage(engine, 0, pixelbuffer);
        if (!nomips) {
            tex.generateMipmaps(engine);
        }
        return tex;
    };

    Filament._createTextureFromJpeg = function(image, engine, options) {

        options = options || {};
        const srgb = !!options['srgb'];
        const nomips = !!options['nomips'];

        var context2d = document.createElement('canvas').getContext('2d');
        context2d.canvas.width = image.width;
        context2d.canvas.height = image.height;
        context2d.width = image.width;
        context2d.height = image.height;
        context2d.globalCompositeOperation = 'copy';
        context2d.drawImage(image, 0, 0);

        var imgdata = context2d.getImageData(0, 0, image.width, image.height).data.buffer;
        var decodedjpeg = new Uint8Array(imgdata);

        const TF = Filament.Texture$InternalFormat;
        const texformat = srgb ? TF.SRGB8_A8 : TF.RGBA8;
        const pbformat = Filament.PixelDataFormat.RGBA;
        const pbtype = Filament.PixelDataType.UBYTE;

        const tex = Filament.Texture.Builder()
            .width(image.width)
            .height(image.height)
            .levels(nomips ? 1 : 0xff)
            .sampler(Filament.Texture$Sampler.SAMPLER_2D)
            .format(texformat)
            .build(engine);

        const pixelbuffer = Filament.PixelBuffer(decodedjpeg, pbformat, pbtype);
        tex.setImage(engine, 0, pixelbuffer);
        if (!nomips) {
            tex.generateMipmaps(engine);
        }
        return tex;
    };

    /// getSupportedFormats ::function:: Queries WebGL to check which compressed formats are supported.
    /// ::retval:: object with boolean values and the following keys: s3tc, astc, etc
    Filament.getSupportedFormats = function() {
        if (Filament.supportedFormats) {
            return Filament.supportedFormats;
        }
        const options = { majorVersion: 2, minorVersion: 0 };
        var ctx = document.createElement('canvas').getContext('webgl2', options);
        const result = {
            s3tc: false,
            astc: false,
            etc: false,
        }
        var exts = ctx.getSupportedExtensions(), nexts = exts.length, i;
        for (i = 0; i < nexts; i++) {
            var ext = exts[i];
            if (ext == "WEBGL_compressed_texture_s3tc") {
                result.s3tc = true;
            } else if (ext == "WEBGL_compressed_texture_astc") {
                result.astc = true;
            } else if (ext == "WEBGL_compressed_texture_etc") {
                result.etc = true;
            }
        }
        return Filament.supportedFormats = result;
    }

    /// getSupportedFormatSuffix ::function:: Generate a file suffix according to the texture format.
    /// Consumes a string describing desired formats and produces a file suffix depending on
    /// which (if any) of the formats are actually supported by the WebGL implementation. This is
    /// useful for compressed textures. For example, some platforms accept ETC and others accept S3TC.
    /// desiredFormats ::argument:: space-delimited string of desired formats
    /// ::retval:: empty string if there is no intersection of supported and desired formats.
    Filament.getSupportedFormatSuffix = function(desiredFormats) {
        desiredFormats = desiredFormats.split(' ');
        var exts = Filament.getSupportedFormats();
        for (var key in exts) {
            if (exts[key] && desiredFormats.includes(key)) {
                return '_' + key;
            }
        }
        return '';
    }
}

function initExtensions() {
    /// Engine ::core class::

    /// create ::static method:: Creates an Engine instance for the given canvas.
    /// canvas ::argument:: the canvas DOM element
    /// options ::argument:: optional WebGL 2.0 context configuration
    /// ::retval:: an instance of [Engine]

    /*
    Filament.Engine.create = function(canvas, options) {
        const defaults = {
            majorVersion: 2,
            minorVersion: 0,
            antialias: false,
            depth: false,
            alpha: false
        };
        options = Object.assign(defaults, options);
        Filament.createContext(canvas, true, true, options);

        // Enable all desired extensions by calling getExtension on each one.
        Filament.ctx.getExtension('WEBGL_compressed_texture_s3tc');
        Filament.ctx.getExtension('WEBGL_compressed_texture_astc');
        Filament.ctx.getExtension('WEBGL_compressed_texture_etc');

        return Filament.Engine._create();
    };
    */

    /// createMaterial ::method::
    /// package ::argument:: asset string, or Uint8Array, or [Buffer] with filamat contents
    /// ::retval:: an instance of [createMaterial]
    Filament.Engine.prototype.createMaterial = function(buffer) {
        buffer = getBufferDescriptor(buffer);
        const result = this._createMaterial(buffer);
        buffer.delete();
        return result;
    };

    /// createTextureFromKtx ::method:: Utility function that creates a [Texture] from a KTX file.
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer] with KTX file contents
    /// options ::argument:: Options dictionary. For now, the `rgbm` boolean is the only option.
    /// ::retval:: [Texture]
    Filament.Engine.prototype.createTextureFromKtx = function(buffer, options) {
        buffer = getBufferDescriptor(buffer);
        const result = Filament._createTextureFromKtx(buffer, this, options);
        buffer.delete();
        return result;
    };

    /// createIblFromKtx ::method:: Utility that creates an [IndirectLight] from a KTX file.
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer] with KTX file contents
    /// options ::argument:: Options dictionary. For now, the `rgbm` boolean is the only option.
    /// ::retval:: [IndirectLight]
    Filament.Engine.prototype.createIblFromKtx = function(buffer, options) {
        buffer = getBufferDescriptor(buffer);
        const result = Filament._createIblFromKtx(buffer, this, options);
        buffer.delete();
        return result;
    };

    /// createSkyFromKtx ::method:: Utility function that creates a [Skybox] from a KTX file.
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer] with KTX file contents
    /// options ::argument:: Options dictionary. For now, the `rgbm` boolean is the only option.
    /// ::retval:: [Skybox]
    Filament.Engine.prototype.createSkyFromKtx = function(buffer, options) {
        options = options || {'rgbm': true};
        const skytex = this.createTextureFromKtx(buffer, options);
        return Filament.Skybox.Builder().environment(skytex).build(this);
    };

    /// createTextureFromPng ::method:: Creates a 2D [Texture] from the raw contents of a PNG file.
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer] with PNG file contents
    /// options ::argument:: object with optional `srgb`, `rgbm`, `noalpha`, and `nomips` keys.
    /// ::retval:: [Texture]
    Filament.Engine.prototype.createTextureFromPng = function(buffer, options) {
        buffer = getBufferDescriptor(buffer);
        const result = Filament._createTextureFromPng(buffer, this, options);
        buffer.delete();
        return result;
    };

    /// createTextureFromJpeg ::method:: Creates a 2D [Texture] from a JPEG image.
    /// image ::argument:: asset string or DOM Image that has already been loaded
    /// options ::argument:: JavaScript object with optional `srgb` and `nomips` keys.
    /// ::retval:: [Texture]
    Filament.Engine.prototype.createTextureFromJpeg = function(image, options) {
        if ('string' == typeof image || image instanceof String) {
            image = Filament.assets[image];
        }
        return Filament._createTextureFromJpeg(image, this, options);
    };

    /// loadFilamesh ::method:: Consumes the contents of a filamesh file and creates a renderable.
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer] with filamesh contents
    /// definstance ::argument:: Optional default [MaterialInstance]
    /// matinstances ::argument:: Optional object that gets filled with name => [MaterialInstance]
    /// ::retval:: JavaScript object with keys `renderable`, `vertexBuffer`, and `indexBuffer`. \
    /// These are of type [Entity], [VertexBuffer], and [IndexBuffer].
    Filament.Engine.prototype.loadFilamesh = function(buffer, definstance, matinstances) {
        buffer = getBufferDescriptor(buffer);
        const result = Filament._loadFilamesh(this, buffer, definstance, matinstances);
        buffer.delete();
        return result;
    };

    /// VertexBuffer ::core class::

    /// setBufferAt ::method::
    /// engine ::argument:: [Engine]
    /// bufferIndex ::argument:: non-negative integer
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer]
    Filament.VertexBuffer.prototype.setBufferAt = function(engine, bufferIndex, buffer) {
        buffer = getBufferDescriptor(buffer);
        this._setBufferAt(engine, bufferIndex, buffer);
        buffer.delete();
    };

    /// IndexBuffer ::core class::

    /// setBuffer ::method::
    /// engine ::argument:: [Engine]
    /// buffer ::argument:: asset string, or Uint8Array, or [Buffer]
    Filament.IndexBuffer.prototype.setBuffer = function(engine, buffer) {
        buffer = getBufferDescriptor(buffer);
        this._setBuffer(engine, buffer);
        buffer.delete();
    };

    Filament.RenderableManager$Builder.prototype.build =
    Filament.LightManager$Builder.prototype.build =
        function(engine, entity) {
            const result = this._build(engine, entity);
            this.delete();
            return result;
        };

    Filament.VertexBuffer$Builder.prototype.build =
    Filament.IndexBuffer$Builder.prototype.build =
    Filament.Texture$Builder.prototype.build =
    Filament.IndirectLight$Builder.prototype.build =
    Filament.Skybox$Builder.prototype.build =
        function(engine) {
            const result = this._build(engine);
            this.delete();
            return result;
        };

    Filament.KtxBundle.prototype.getBlob = function(index) {
        const blob = this._getBlob(index);
        const result = blob.getBytes();
        blob.delete();
        return result;
    }

    Filament.KtxBundle.prototype.getCubeBlob = function(miplevel) {
        const blob = this._getCubeBlob(miplevel);
        const result = blob.getBytes();
        blob.delete();
        return result;
    }

    Filament.Texture.prototype.setImage = function(engine, level, pbd) {
        this._setImage(engine, level, pbd);
        pbd.delete();
    }

    Filament.Texture.prototype.setImageCube = function(engine, level, pbd) {
        this._setImageCube(engine, level, pbd);
        pbd.delete();
    }
}
