#include <coreinit/memdefaultheap.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/texture.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <vpad/input.h>
#include <whb/file.h>
#include <whb/gfx.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/proc.h>
#include <stdint.h>
#include <string.h>

static const char *LAYOUT = "/vol/content/memo/layout/Body/Common-extracted/blyt/MemoTop.bflyt";
static const char *SHADER = "/vol/content/memo/shader/texture_shader.gsh";
static const char *ROOT = "/vol/content/memo/layout/Body/Common-extracted/timg/";

struct Pane { char name[33]; float x,y,w,h; uint16_t material; uint8_t visible; float uv[8]; };
struct Tex { GX2Texture t; void *image; char *file; bool ok; };
struct V { float x,y; };
struct UV { float u,v; };

static uint16_t u16(const uint8_t *p){ return (uint16_t(p[0])<<8)|p[1]; }
static uint32_t u32(const uint8_t *p){ return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
static float f32(const uint8_t *p){ uint32_t n=u32(p); float f; memcpy(&f,&n,4); return f; }
static void copyname(char *d,const uint8_t*s,size_t n){size_t i=0;for(;i+1<n&&s[i];++i)d[i]=(char)s[i];d[i]=0;}

static GX2SurfaceFormat bflimFmt(uint8_t f){
 switch(f){
  case 0x00: case 0x01: return GX2_SURFACE_FORMAT_UNORM_R8;
  case 0x02: return GX2_SURFACE_FORMAT_UNORM_R4_G4;
  case 0x03: return GX2_SURFACE_FORMAT_UNORM_R8_G8;
  case 0x05: return GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
  case 0x07: return GX2_SURFACE_FORMAT_UNORM_R5_G5_B5_A1;
  case 0x08: return GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
  case 0x09: return GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
  case 0x0c: return GX2_SURFACE_FORMAT_UNORM_BC1;
  case 0x0d: return GX2_SURFACE_FORMAT_UNORM_BC2;
  case 0x0e: return GX2_SURFACE_FORMAT_UNORM_BC3;
  case 0x0f: case 0x10: return GX2_SURFACE_FORMAT_UNORM_BC4;
  case 0x11: return GX2_SURFACE_FORMAT_UNORM_BC5;
  case 0x14: return GX2_SURFACE_FORMAT_SRGB_R8_G8_B8_A8;
  case 0x15: return GX2_SURFACE_FORMAT_SRGB_BC1;
  case 0x16: return GX2_SURFACE_FORMAT_SRGB_BC2;
  case 0x17: return GX2_SURFACE_FORMAT_SRGB_BC3;
  case 0x18: return GX2_SURFACE_FORMAT_UNORM_R10_G10_B10_A2;
  default: return GX2_SURFACE_FORMAT_INVALID;
 }
}

static bool loadBflim(Tex &o,const char *path){
 memset(&o,0,sizeof(o));
 uint32_t sz=0; char *r=WHBReadWholeFile(path,&sz);
 if(!r||sz<0x28){if(r)WHBFreeWholeFile(r);return false;}
 uint8_t *d=(uint8_t*)r;
 uint8_t *fl=d+sz-0x28;
 uint8_t *im=fl+0x14;
 if(memcmp(fl,"FLIM",4)||memcmp(im,"imag",4)){WHBFreeWholeFile(r);return false;}
 uint16_t w=u16(im+8),h=u16(im+10),align=u16(im+12);
 uint8_t fmt=im[0x0e],packed=im[0x0f];
 uint32_t raw=u32(im+0x10);
 GX2SurfaceFormat sf=bflimFmt(fmt);
 if(!w||!h||sf==GX2_SURFACE_FORMAT_INVALID||raw>sz-0x28){WHBFreeWholeFile(r);return false;}
 memset(&o.t,0,sizeof(o.t));
 o.t.surface.dim=GX2_SURFACE_DIM_TEXTURE_2D;
 o.t.surface.width=w;o.t.surface.height=h;o.t.surface.depth=1;o.t.surface.mipLevels=1;
 o.t.surface.format=sf;o.t.surface.aa=GX2_AA_MODE1X;o.t.surface.use=GX2_SURFACE_USE_TEXTURE;
 o.t.surface.tileMode=GX2TileMode(packed&31);o.t.surface.swizzle=packed>>5;
 GX2CalcSurfaceSizeAndAlignment(&o.t.surface);
 if(align)o.t.surface.alignment=align;
 o.image=MEMAllocFromDefaultHeapEx(o.t.surface.imageSize,o.t.surface.alignment);
 if(!o.image){WHBFreeWholeFile(r);return false;}
 memset(o.image,0,o.t.surface.imageSize);
 uint32_t n=raw<o.t.surface.imageSize?raw:o.t.surface.imageSize;
 memcpy(o.image,d,n);
 o.t.surface.image=o.image;o.t.viewFirstMip=0;o.t.viewNumMips=1;o.t.viewFirstSlice=0;o.t.viewNumSlices=1;o.t.compMap=0x00010203;
 GX2InitTextureRegs(&o.t);
 GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,o.image,o.t.surface.imageSize);
 o.file=r;o.ok=true;
 return true;
}

static void freeTex(Tex&o){if(o.image)MEMFreeToDefaultHeap(o.image);if(o.file)WHBFreeWholeFile(o.file);memset(&o,0,sizeof(o));}

static uint32_t parseLayout(Pane *out,uint32_t max,float &lw,float &lh){
 lw=854;lh=480;uint32_t sz=0;char *r=WHBReadWholeFile(LAYOUT,&sz);if(!r||sz<0x14){if(r)WHBFreeWholeFile(r);return 0;}
 uint8_t *d=(uint8_t*)r;if(memcmp(d,"FLYT",4)){WHBFreeWholeFile(r);return 0;}
 uint32_t n=0;
 for(uint32_t o=0x14;o+8<=sz;){
  uint8_t*s=d+o;uint32_t ss=u32(s+4);if(ss<8||o+ss>sz)break;
  if(!memcmp(s,"lyt1",4)&&ss>=0x1c){lw=f32(s+0x0c);lh=f32(s+0x10);}
  if(!memcmp(s,"pic1",4)&&ss>=0x68&&n<max){
   Pane&p=out[n++];memset(&p,0,sizeof(p));
   p.visible=(s[8]&1)!=0;copyname(p.name,s+0x0c,33);
   p.x=f32(s+0x2c);p.y=f32(s+0x30);p.w=f32(s+0x4c);p.h=f32(s+0x50);
   p.material=u16(s+0x64);
   uint8_t uc=s[0x66];
   if(uc>=4&&ss>=0x98)for(int i=0;i<8;i++)p.uv[i]=f32(s+0x68+i*4);
   else {p.uv[0]=0;p.uv[1]=0;p.uv[2]=1;p.uv[3]=0;p.uv[4]=0;p.uv[5]=1;p.uv[6]=1;p.uv[7]=1;}
  }
  o+=(ss+3)&~3u;
 }
 WHBFreeWholeFile(r);return n;
}

static Tex *choose(const Pane&p,Tex&pen,Tex&eraser,Tex&write){
 if(strstr(p.name,"Pen")||strstr(p.name,"pen"))return pen.ok?&pen:0;
 if(strstr(p.name,"Ers")||strstr(p.name,"Eraser")||strstr(p.name,"eraser"))return eraser.ok?&eraser:0;
 if(strstr(p.name,"Write")||strstr(p.name,"write"))return write.ok?&write:0;
 return 0;
}

static void makeQuad(V*v,UV*u,const Pane&p,float lw,float lh){
 float x0=p.x/lw*2.0f-1.0f,x1=(p.x+p.w)/lw*2.0f-1.0f;
 float y0=1.0f-p.y/lh*2.0f,y1=1.0f-(p.y+p.h)/lh*2.0f;
 v[0]={x0,y1};v[1]={x1,y1};v[2]={x1,y0};v[3]={x0,y0};
 u[0]={p.uv[0],p.uv[1]};u[1]={p.uv[2],p.uv[3]};u[2]={p.uv[6],p.uv[7]};u[3]={p.uv[4],p.uv[5]};
}

static void drawPane(const Pane&p,Tex&t,GX2RBuffer&pb,GX2RBuffer&ub,float lw,float lh,WHBGfxShaderGroup&sh,GX2Sampler&samp){
 V v[4];UV u[4];makeQuad(v,u,p,lw,lh);
 memcpy(GX2RLockBufferEx(&pb,GX2R_RESOURCE_USAGE_CPU_WRITE),v,sizeof(v));GX2RUnlockBufferEx(&pb,GX2R_RESOURCE_USAGE_CPU_WRITE);
 memcpy(GX2RLockBufferEx(&ub,GX2R_RESOURCE_USAGE_CPU_WRITE),u,sizeof(u));GX2RUnlockBufferEx(&ub,GX2R_RESOURCE_USAGE_CPU_WRITE);
 GX2RSetAttributeBuffer(&pb,0,sizeof(V),0);GX2RSetAttributeBuffer(&ub,1,sizeof(UV),0);
 uint32_t unit=sh.pixelShader->samplerVars[0].location;
 GX2SetPixelTexture(&t.t,unit);GX2SetPixelSampler(&samp,unit);
 GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS,4,0,1);
}

int main(){
 WHBProcInit();
 if(!WHBGfxInit()){WHBProcShutdown();return -1;}
 WHBLogConsoleInit();
 float lw,lh;Pane panes[256];uint32_t pc=parseLayout(panes,256,lw,lh);
 WHBLogPrintf("Stage 2: MemoTop panes=%u layout=%.0fx%.0f",pc,lw,lh);
 uint32_t shaderSize=0;char *shaderData=WHBReadWholeFile(SHADER,&shaderSize);WHBGfxShaderGroup sh;memset(&sh,0,sizeof(sh));
 bool shader=shaderData&&WHBGfxLoadGFDShaderGroup(&sh,0,shaderData);
 if(shader){WHBGfxInitShaderAttribute(&sh,"position",0,0,GX2_ATTRIB_FORMAT_FLOAT_32_32);WHBGfxInitShaderAttribute(&sh,"tex_coord_in",1,0,GX2_ATTRIB_FORMAT_FLOAT_32_32);WHBGfxInitFetchShader(&sh);}
 GX2RBuffer pb{},ub{};
 pb.flags=GX2R_RESOURCE_BIND_VERTEX_BUFFER|GX2R_RESOURCE_USAGE_CPU_READ|GX2R_RESOURCE_USAGE_CPU_WRITE|GX2R_RESOURCE_USAGE_GPU_READ;pb.elemSize=sizeof(V);pb.elemCount=4;GX2RCreateBuffer(&pb);
 ub.flags=GX2R_RESOURCE_BIND_VERTEX_BUFFER|GX2R_RESOURCE_USAGE_CPU_READ|GX2R_RESOURCE_USAGE_CPU_WRITE|GX2R_RESOURCE_USAGE_GPU_READ;ub.elemSize=sizeof(UV);ub.elemCount=4;GX2RCreateBuffer(&ub);
 Tex pen,eraser,write;
 loadBflim(pen,"/vol/content/memo/layout/Body/Common-extracted/timg/P_PenIcon_00^t.bflim");
 loadBflim(eraser,"/vol/content/memo/layout/Body/Common-extracted/timg/P_EraserIcon_00^t.bflim");
 loadBflim(write,"/vol/content/memo/layout/Body/Common-extracted/timg/P_WriteIcon_00^t.bflim");
 WHBLogPrintf("Textures: pen=%d eraser=%d write=%d shader=%d",pen.ok,eraser.ok,write.ok,shader);
 GX2Sampler samp;GX2InitSampler(&samp,GX2_TEX_CLAMP_MODE_CLAMP,GX2_TEX_XY_FILTER_MODE_LINEAR);
 while(WHBProcIsRunning()){
  VPADStatus st;VPADReadError er;memset(&st,0,sizeof(st));
  if(VPADRead(VPAD_CHAN_0,&st,1,&er)>0){
   if(st.trigger&VPAD_BUTTON_A)WHBLogPrintf("A: UI selection");
   if(st.trigger&VPAD_BUTTON_B)WHBLogPrintf("B: UI back");
   if(st.tpNormal.touched&&st.tpNormal.validity==VPAD_VALID)WHBLogPrintf("Touch %d %d",st.tpNormal.x,st.tpNormal.y);
  }
  WHBGfxBeginRender();
  WHBGfxBeginRenderTV();WHBGfxClearColor(0.92f,0.92f,0.92f,1.0f);
  if(shader){GX2SetFetchShader(&sh.fetchShader);GX2SetVertexShader(sh.vertexShader);GX2SetPixelShader(sh.pixelShader);for(uint32_t i=0;i<pc;i++){Tex*t=choose(panes[i],pen,eraser,write);if(panes[i].visible&&t)drawPane(panes[i],*t,pb,ub,lw,lh,sh,samp);}}
  WHBGfxFinishRenderTV();
  WHBGfxBeginRenderDRC();WHBGfxClearColor(0.92f,0.92f,0.92f,1.0f);
  if(shader){GX2SetFetchShader(&sh.fetchShader);GX2SetVertexShader(sh.vertexShader);GX2SetPixelShader(sh.pixelShader);for(uint32_t i=0;i<pc;i++){Tex*t=choose(panes[i],pen,eraser,write);if(panes[i].visible&&t)drawPane(panes[i],*t,pb,ub,lw,lh,sh,samp);}}
  WHBGfxFinishRenderDRC();WHBGfxFinishRender();
  WHBLogConsoleDraw();OSSleepTicks(OSMillisecondsToTicks(16));
 }
 freeTex(pen);freeTex(eraser);freeTex(write);GX2RDestroyBufferEx(&pb,pb.flags);GX2RDestroyBufferEx(&ub,ub.flags);if(shaderData)WHBFreeWholeFile(shaderData);WHBLogConsoleFree();WHBGfxShutdown();WHBProcShutdown();return 0;
}
