#if 0
//
// Generated by 2.0.20353.0
//
//   fxc /Fh psx.ps.f.h /Tps_3_0 ps_psx.hlsl /EpsF
//
//
// Parameters:
//
//   sampler2D tex0;
//
//
// Registers:
//
//   Name         Reg   Size
//   ------------ ----- ----
//   tex0         s0       1
//

// Shader type: pixel 

xps_3_0
defconst tex0, sampler2d, object, [1, 1], s0
config AutoSerialize=false
config AutoResource=false
config PsMaxReg=1
// PsExportColorCount=1
// PsSampleControl=both

dcl_texcoord r0.xy
dcl_color_centroid r1


    exec
    tfetch2D r0, r0.xy, tf0
    alloc colors
    exece
    mov oC0, r0

// PDB hint 00000000-00000000-00000000

#endif

// This microcode is in native DWORD byte order.

const DWORD g_xps_psF[] =
{
    0x102a1100, 0x000000ac, 0x0000003c, 0x00000000, 0x00000024, 0x00000000, 
    0x00000084, 0x00000000, 0x00000000, 0x0000005c, 0x0000001c, 0x0000004f, 
    0xffff0300, 0x00000001, 0x0000001c, 0x00000000, 0x00000048, 0x00000030, 
    0x00030000, 0x00010000, 0x00000038, 0x00000000, 0x74657830, 0x00ababab, 
    0x0004000c, 0x00010001, 0x00010000, 0x00000000, 0x70735f33, 0x5f300032, 
    0x2e302e32, 0x30333533, 0x2e3000ab, 0x00000000, 0x0000003c, 0x10000100, 
    0x00000008, 0x00000000, 0x00001842, 0x00010003, 0x00000001, 0x00003050, 
    0x0000f1a0, 0x00011002, 0x00001200, 0xc4000000, 0x00001003, 0x00002200, 
    0x00000000, 0x10080001, 0x1f1ff688, 0x00004000, 0xc80f8000, 0x00000000, 
    0xe2000000, 0x00000000, 0x00000000, 0x00000000
};