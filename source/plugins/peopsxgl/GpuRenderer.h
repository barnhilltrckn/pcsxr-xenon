/* 
 * File:   XeRenderer.h
 * Author: cc
 *
 * Created on 11 octobre 2011, 16:51
 */

#ifndef GPURENDERER_H
#define	GPURENDERER_H

#include <xenos/xe.h>

#define XE_TEXF_POINT 0
#define XE_TEXF_LINEAR 1

typedef struct XeColor{
    union {
        unsigned int color;
        struct{
            uint8_t a;
            uint8_t r;
            uint8_t g;
            uint8_t b;
        };
        uint8_t s[4];
    };
} XeColor;

enum GpuPrimTypes{
    PRIM_TRIANGLE,
    PRIM_TRIANGLE_STRIP,
    PRIM_QUAD
};

class GpuRenderer {
private:
    /**
     * packed to 32bytes
     */
    typedef struct __attribute__((__packed__)) verticeformats {
        float x, y, z;
        float u, v;
        float u2, v2;
        uint32_t color;
    }
    verticeformats;

    verticeformats * pCurrentVertex;
    verticeformats * pFirstVertex;

    uint16_t * pCurrentIndice;
    uint16_t * pFirstIndice;

    XenosVertexBuffer *pVb;
    XenosIndexBuffer *pIb;

    XenosSurface * pRenderSurface;

    XenosShader * g_pVertexShader;
    XenosShader * g_pPixelShader;

    XenosShader * g_pPixelShaderC;
    XenosShader * g_pPixelShaderF;
    XenosShader * g_pPixelShaderG;

    /**
     * Render states
     */
    struct RenderStates {
        // texture
        struct XenosSurface * surface;
        // clear color
        uint32_t clearcolor;
        // z / depth
        uint32_t z_func;
        uint32_t z_write;
        uint32_t z_enable;

        // fillmode
        uint32_t fillmode_front;
        uint32_t fillmode_back;

        // blend
        uint32_t blend_op;
        uint32_t blend_src;
        uint32_t blend_dst;

        // alpha blend
        uint32_t alpha_blend_op;
        uint32_t alpha_blend_src;
        uint32_t alpha_blend_dst;

        // cull mode
        uint32_t cullmode;

        // alpha test
        uint32_t alpha_test_enable;
        uint32_t alpha_test_func;
        float alpha_test_ref;

        // stencil
        uint32_t stencil_enable;
        uint32_t stencil_func;
        uint32_t stencil_op;
        uint32_t stencil_ref;
        uint32_t stencil_mask;
        uint32_t stencil_writemask;
    };

    RenderStates m_RenderStates;

private:
    int b_StatesChanged;


    /**
     * states changed
     */
    void StatesChanged();

    void UpdatesStates();

    void SubmitVertices();

    void InitStates();
    void InitXe();

    void Lock();
    void Unlock();
public:

    /**
     * texture
     */

    void SetTexture(struct XenosSurface * s);
    void EnableTexture();
    void DisableTexture();

    /**
     * Clear
     */
    void Clear(uint32_t flags);
    void ClearColor(float r, float g, float b, float a);
    // fillmode

    /**
     * blend
     */
    void DisableBlend();
    void EnableBlend();
    void SetBlendFunc(int src, int dst);
    void SetBlendOp(int op);

    /**
     * Alpha blend
     */
    void DisableAlphaBlend();
    void EnableAlphaBlend();
    void SetAlphaBlendFunc(int src, int dst);
    void SetAlphaBlendOp(int op);

    // cull mode

    /**
     * Alpha test
     */
    void SetAlphaFunc(int func, float ref);
    void EnableAlphaTest();
    void DisableAlphaTest();

    // stencil

    /**
     * z / depth
     */
    void EnableDepthTest();
    void DisableDepthTest();
    void DepthFunc(int func);

    // scissor
    void DisableScissor();
    void EnableScissor();
    
    /**
     * Init
     */
    void Init();
    void Close();

    /**
     * Render
     */
    void Render();

    /**
     * Gl like func
     */
    int m_PrimType;
    float textcoord_u;
    float textcoord_v;
    uint32_t m_primColor;
    
    void primBegin(int primType);
    void primEnd();
    void primTexCoord(float * st);
    void primVertex(float * v);
    void primColor(u8 *v);
    
    void TextEnv(int f,int v){
        // nothing yet
    }
    
    int verticesCount();
    int indicesCount();
    int prevIndicesCount;
    int prevVerticesCount;
    
    // viewport
    void SetViewPort(int left, int top, int right, int bottom);
    void SetOrtho(float l, float r, float b, float t, float zn, float zf);
    
    // textures
    void DestroyTexture(XenosSurface *surf);
    XenosSurface * CreateTexture(unsigned int width, unsigned int height, int format);
    void SetTextureFiltering(int filtering_mode){
        if(m_RenderStates.surface)
                m_RenderStates.surface->use_filtering = filtering_mode;
    }
};

extern GpuRenderer gpuRenderer;

// tex
void xeGfx_setTextureData(void * tex, void * buffer);
void XeTexSubImage(struct XenosSurface * surf, int srcbpp, int dstbpp, int xoffset, int yoffset, int width, int height, const void * buffer);

static inline void DoBufferSwap(){
    gpuRenderer.Render();
}

#endif	/* GPURENDERER_H */

