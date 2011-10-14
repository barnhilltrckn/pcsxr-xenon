#if 0
//
// Generated by 2.0.20353.0
//
//   fxc /Fh psx.vs.h /Tvs_3_0 vs_psx.hlsl
//
//
// Parameters:
//
//   float2 screenSize;
//
//
// Registers:
//
//   Name         Reg   Size
//   ------------ ----- ----
//   screenSize   c1       1
//

// Shader type: vertex 

xvs_3_0
defconst screenSize, float, vector, [1, 2], c1
config AutoSerialize=false
config AutoResource=false
config VsMaxReg=3
config VsResource=1
// VsExportCount=2

dcl_index r0.x
dcl_texcoord o0.xy
dcl_texcoord1 o1.xy
dcl_color o2

def c252, 0, 0, 0, 0
def c253, 0, 0, 0, 0
def c254, 0, 0, 0, 0
def c255, 1, 0, 0, 0


    exec
    vfetch r2._xyz, r0.x, position
    vfetch r1.__xy, r0.x, texcoord
    vfetch r1.xy__, r0.x, texcoord1
    vfetch r0, r0.x, color
    alloc position
    exec
    rcp r2.x, c1.y
    add r3.xy, r2.yz, r2.yz
  + rcp r3.z, c1.x
    mad oPos.x001, r3.x, r3.z, -c255.x
    mul r2.x, r3.y, r2.x
    add oPos.yz, -r2.xxw, c255.x
    alloc interpolators
    exece
    mov o0.xy, r1.zw
    mov o1.xy, r1.xy
    mov o2, r0

// PDB hint 00000000-00000000-00000000

#endif

// This microcode is in native DWORD byte order.

const DWORD g_xvs_main[] =
{
    0x102a1101, 0x00000100, 0x00000100, 0x00000000, 0x00000024, 0x00000088, 
    0x000000b0, 0x00000000, 0x00000000, 0x00000060, 0x0000001c, 0x00000053, 
    0xfffe0300, 0x00000001, 0x0000001c, 0x00000000, 0x0000004c, 0x00000030, 
    0x00020001, 0x00010000, 0x0000003c, 0x00000000, 0x73637265, 0x656e5369, 
    0x7a6500ab, 0x00010003, 0x00010002, 0x00010000, 0x00000000, 0x76735f33, 
    0x5f300032, 0x2e302e32, 0x30333533, 0x2e3000ab, 0x00000000, 0x00000001, 
    0x00000000, 0x00000000, 0x00000014, 0x00fc0010, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000040, 0x000000c0, 0x00210003, 0x00000000, 
    0x00000000, 0x00002063, 0x00000001, 0x00000004, 0x00000003, 0x00000290, 
    0x00100003, 0x00005004, 0x00015005, 0x0030a006, 0x00003050, 0x00013151, 
    0x0002f2a0, 0x0000100c, 0x0000100d, 0x0000100e, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 
    0x00000000, 0x00000000, 0xf0554003, 0x00001200, 0xc2000000, 0x00005007, 
    0x00001200, 0xc4000000, 0x0000300c, 0x00002200, 0x00000000, 0x05f82000, 
    0x00000447, 0x00000000, 0x05f81000, 0x0000023f, 0x00000000, 0x05f81000, 
    0x00000fc8, 0x00000000, 0x05f80000, 0x00000688, 0x00000000, 0x4c100200, 
    0x000000b1, 0xc2000001, 0x4c430303, 0x00c5c56c, 0xc0020201, 0xc889c03e, 
    0x016cc66c, 0xcb0303ff, 0xc8010002, 0x00b16c00, 0xe1030200, 0xc806803e, 
    0x041c6c00, 0xa002ff00, 0xc8038000, 0x001a1a00, 0xe2010100, 0xc8038001, 
    0x00b0b000, 0xe2010100, 0xc80f8002, 0x00000000, 0xe2000000, 0x00000000, 
    0x00000000, 0x00000000
};
