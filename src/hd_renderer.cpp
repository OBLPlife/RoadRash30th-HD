// SPDX-License-Identifier: PolyForm-Noncommercial-1.0.0
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#include <string>
#include <cstdio>
#include <intrin.h>
#include <array>
#include "hd_raster.hpp"
#include "hd_geometry.hpp"

namespace {
using rrhd::Vertex;
using rrhd::Texture;
uintptr_t base;
template<class T> T& at(uintptr_t va) { return *reinterpret_cast<T*>(base+va-0x400000); }
template<class T> T fn(uintptr_t va) { return reinterpret_cast<T>(base+va-0x400000); }
using DrawList = int (__cdecl*)();
using Normalize = bool (__fastcall*)(int*, int);
using Actor = void (__fastcall*)(void*);
using HorizonLoad = int (__fastcall*)(const char*);
using HorizonDraw = void (__fastcall*)(int, int);
using MenuDecode = uint16_t* (__fastcall*)(const char*);
using Present8 = void (__fastcall*)(const uint8_t*, int, const uint8_t*);
using Present16 = void (__fastcall*)(const void*, int, int, int, int);
using Stretch = decltype(&StretchDIBits);
Stretch originalStretch;
using Fill = decltype(&FillRect);
Fill originalFill;
HMODULE rendererModule;
HWND hookedWindow;
WNDPROC previousWindowProc;
bool completedFrame=false;
int renderHeight=0, width=0, height=0, clientWidth=0, clientHeight=0, logicalWidth=320;
double sceneScale=1, sceneOffset=0;
bool enabled=true, frameActive=false, begun=false, worldReady=false, displayReady=false;
uint64_t frames=0, primitives=0, batches=0;
std::vector<uint8_t> world, baseline, horizon;
int horizonWidth=0, horizonRows=0, horizonScroll=0, horizonY=0;
std::vector<uint32_t> rgba, beforeText;
std::vector<int> mapX, mapY, sceneX, sceneY;
std::vector<int> backdropX, panoramaX;
int uiX[641]{},uiY[481]{};
std::vector<uint16_t> menuBright, menuDim;
std::vector<uint32_t> menuBackdrop;
bool menuBackdropDirty=true;
bool menuBackdropDim=false;
uint32_t color16[65536]{};
int colorMasks[3]{};
std::wstring configPath, logPath;

struct ResolutionChoice {int height,width;std::wstring label;};
std::vector<ResolutionChoice> resolutionChoices(int cw,int ch,int selected) {
    std::vector<ResolutionChoice> result;
    result.push_back({0,cw,L"自动（原生 "+std::to_wstring(cw)+L" × "+std::to_wstring(ch)+L"）"});
    std::vector<int> heights={ch,2160,1600,1440,1080,900,720,540,480};
    if(selected>0)heights.push_back(selected);
    std::sort(heights.begin(),heights.end(),std::greater<int>());
    heights.erase(std::unique(heights.begin(),heights.end()),heights.end());
    for(int h:heights)if(h<=ch || h==selected) {
        int w=std::max(1,int(std::lround(double(cw)*h/ch)));
        result.push_back({h,w,std::to_wstring(w)+L" × "+std::to_wstring(h)});
    }
    return result;
}
struct ResolutionDialog {int cw,ch;std::vector<ResolutionChoice> choices;int chosen=-1;};
INT_PTR CALLBACK resolutionDialogProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    auto state=reinterpret_cast<ResolutionDialog*>(GetWindowLongPtrW(window,DWLP_USER));
    if(message==WM_INITDIALOG) {
        state=reinterpret_cast<ResolutionDialog*>(lp);SetWindowLongPtrW(window,DWLP_USER,lp);
        std::wstring title=L"当前窗口："+std::to_wstring(state->cw)+L" × "+std::to_wstring(state->ch);
        SetDlgItemTextW(window,1001,title.c_str());int selected=0;
        for(size_t i=0;i<state->choices.size();++i) {
            SendDlgItemMessageW(window,1002,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(state->choices[i].label.c_str()));
            if(state->choices[i].height==renderHeight)selected=int(i);
        }
        SendDlgItemMessageW(window,1002,CB_SETCURSEL,selected,0);
        RECT owner{},r{};GetWindowRect(GetParent(window),&owner);GetWindowRect(window,&r);
        SetWindowPos(window,nullptr,(owner.left+owner.right-r.right+r.left)/2,
                     (owner.top+owner.bottom-r.bottom+r.top)/2,0,0,SWP_NOSIZE|SWP_NOZORDER);
        return TRUE;
    }
    if(message==WM_COMMAND && state) {
        if(LOWORD(wp)==IDOK) {
            int selected=int(SendDlgItemMessageW(window,1002,CB_GETCURSEL,0,0));
            if(selected>=0 && selected<int(state->choices.size()))state->chosen=selected;
            EndDialog(window,IDOK);return TRUE;
        }
        if(LOWORD(wp)==IDCANCEL){EndDialog(window,IDCANCEL);return TRUE;}
    }
    if(message==WM_CLOSE){EndDialog(window,IDCANCEL);return TRUE;}
    return FALSE;
}
int __cdecl resolutionMenuHook() {
    HWND owner=at<HWND>(0x5ce858);RECT r{};
    if(!owner || !GetClientRect(owner,&r) || r.right<=0 || r.bottom<=0)return 0;
    ResolutionDialog dialog{r.right,r.bottom,resolutionChoices(r.right,r.bottom,renderHeight)};
    if(DialogBoxParamW(rendererModule,MAKEINTRESOURCEW(101),owner,resolutionDialogProc,reinterpret_cast<LPARAM>(&dialog))==IDOK && dialog.chosen>=0) {
        renderHeight=dialog.choices[dialog.chosen].height;
        std::wstring value=std::to_wstring(renderHeight);
        WritePrivateProfileStringW(L"Graphics",L"RenderHeight",value.c_str(),configPath.c_str());
        // Original resolution setter keeps its registry preference consistent.
        fn<void(__fastcall*)(int)>(0x45bd20)(1);
        InvalidateRect(owner,nullptr,FALSE);
    }
    return 0;
}
LRESULT CALLBACK renderWindowProc(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    if(message==WM_ERASEBKGND && completedFrame)return 1;
    WNDPROC previous=previousWindowProc;
    LRESULT result=CallWindowProcA(previous,window,message,wp,lp);
    if(message==WM_NCDESTROY){hookedWindow=nullptr;previousWindowProc=nullptr;completedFrame=false;}
    return result;
}
void hookWindow(HWND window) {
    if(!window || hookedWindow==window)return;
    auto previous=reinterpret_cast<WNDPROC>(SetWindowLongPtrA(window,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(&renderWindowProc)));
    if(previous){hookedWindow=window;previousWindowProc=previous;}
}

struct RasterJob {Vertex points[4];Texture texture;uint8_t color;size_t snapshot=SIZE_MAX;};
std::vector<RasterJob> rasterJobs;
std::vector<uint8_t> textureSnapshots;
void enqueueQuad(const Vertex (&points)[4],Texture texture,uint8_t color,bool changing=false) {
    RasterJob job{};std::memcpy(job.points,points,sizeof job.points);job.texture=texture;job.color=color;
    if(changing && texture.pixels) {
        double ul=points[0].u,ur=ul,vt=points[0].v,vb=vt;
        for(const auto& p:points){ul=std::min(ul,p.u);ur=std::max(ur,p.u);vt=std::min(vt,p.v);vb=std::max(vb,p.v);}
        int x0=std::clamp(int(ul),0,texture.width-1),x1=std::clamp(int(ur)+1,1,texture.width);
        int y0=std::clamp(int(vt),0,texture.height-1),y1=std::clamp(int(vb)+1,1,texture.height);
        job.snapshot=textureSnapshots.size();job.texture.width=x1-x0;job.texture.height=y1-y0;
        for(auto& p:job.points){p.u-=x0;p.v-=y0;}
        // The actor can occupy only a small rectangle in a large animation
        // atlas. Snapshot that rectangle, not every unused animation frame.
        for(int y=y0;y<y1;++y) {
            auto row=texture.pixels+size_t(y)*texture.width+x0;
            textureSnapshots.insert(textureSnapshots.end(),row,row+(x1-x0));
        }
    }
    rasterJobs.push_back(job);
}
// Disjoint row strips preserve the original draw order at every pixel. Sprite
// images modified by the engine are snapshotted before workers start reading.
struct RasterWorkers {
    struct Strip {int index;RasterWorkers* owner;PTP_WORK work;};
    std::array<Strip,8> strips{};int count=0;
    static void CALLBACK render(PTP_CALLBACK_INSTANCE,void* context,PTP_WORK) {
        auto s=static_cast<Strip*>(context);
        int top=height*s->index/s->owner->count,bottom=height*(s->index+1)/s->owner->count;
        for(const auto& job:rasterJobs)rrhd::quad(world.data(),width,height,job.points,job.texture,job.color,top,bottom);
    }
    void initialize() {
        count=std::max(1,std::min(8,int(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS)/2)));
        for(int i=0;i<count;++i) {strips[i]={i,this,nullptr};strips[i].work=CreateThreadpoolWork(render,&strips[i],nullptr);}
    }
    ~RasterWorkers() {for(auto& s:strips)if(s.work)CloseThreadpoolWork(s.work);}
} rasterWorkers;
void runRasterJobs() {
    for(auto& job:rasterJobs)if(job.snapshot!=SIZE_MAX)job.texture.pixels=textureSnapshots.data()+job.snapshot;
    if(rasterJobs.size()<32 || height<480) {
        for(const auto& job:rasterJobs)rrhd::quad(world.data(),width,height,job.points,job.texture,job.color);
    } else {
        if(!rasterWorkers.count)rasterWorkers.initialize();
        for(int i=0;i<rasterWorkers.count;++i) {
            auto& s=rasterWorkers.strips[i];
            if(s.work)SubmitThreadpoolWork(s.work);else RasterWorkers::render(nullptr,&s,nullptr);
        }
        for(int i=0;i<rasterWorkers.count;++i)if(rasterWorkers.strips[i].work)
            WaitForThreadpoolWorkCallbacks(rasterWorkers.strips[i].work,FALSE);
    }
    rasterJobs.clear();textureSnapshots.clear();
}

void status(const char* message) {
    if(logPath.empty()) return;
    FILE* f=nullptr;
    if(_wfopen_s(&f,logPath.c_str(),L"a")==0 && f) { std::fprintf(f,"%s\n",message);std::fclose(f); }
}
bool writeMemory(uintptr_t address,const void* bytes,size_t length) {
    void* dest=reinterpret_cast<void*>(address);DWORD oldProtect;
    if(!VirtualProtect(dest,length,PAGE_EXECUTE_READWRITE,&oldProtect)) return false;
    std::memcpy(dest,bytes,length);FlushInstructionCache(GetCurrentProcess(),dest,length);
    DWORD ignored;VirtualProtect(dest,length,oldProtect,&ignored);return true;
}
bool branch(uintptr_t va,void* target,uint8_t opcode) {
    uint8_t code[5]={opcode};uint32_t rel=uint32_t(reinterpret_cast<uintptr_t>(target)-(base+va-0x400000+5));
    std::memcpy(code+1,&rel,4);return writeMemory(base+va-0x400000,code,sizeof code);
}
bool isBranch(uintptr_t va,uint8_t opcode,uintptr_t target) {
    return at<uint8_t>(va)==opcode && uintptr_t(intptr_t(va+5)+at<int32_t>(va+1))==target;
}

// Physical client dimensions, without an aspect whitelist or fixed row table.
void viewport(int cw,int ch) {
    clientWidth=cw;clientHeight=ch;
    int h=renderHeight>0?renderHeight:ch;
    int w=renderHeight>0?std::max(1,int(std::lround(double(cw)*h/ch))):cw;
    logicalWidth=(int(std::ceil(240.0*w/h))+1)&~1;
    sceneScale=h/480.0;sceneOffset=w/2.0-logicalWidth*sceneScale;
    if(w==width && h==height) return;
    width=w;height=h;
    world.resize(size_t(w)*h);rgba.resize(size_t(w)*h);
    baseline.resize(640*480);beforeText.resize(640*480);
    mapX.resize(w);mapY.resize(h);sceneX.resize(640);sceneY.resize(480);
    backdropX.resize(w);panoramaX.resize(w);menuBackdropDirty=true;
    double ui=std::min(w/640.0,h/480.0);
    double ox=(w-640*ui)/2,oy=(h-480*ui)/2;
    for(int x=0;x<=640;++x)uiX[x]=std::clamp(int(std::ceil(ox+x*ui-0.5)),0,w);
    for(int y=0;y<=480;++y)uiY[y]=std::clamp(int(std::ceil(oy+y*ui-0.5)),0,h);
    for(int x=0;x<w;++x) {
        int p=int(std::floor((x+0.5-ox)/ui));mapX[x]=(p>=0 && p<640)?p:-1;
        backdropX[x]=int(std::floor((x+0.5-w/2.0)/sceneScale+320));
    }
    for(int y=0;y<h;++y) {
        int p=int(std::floor((y+0.5-oy)/ui));mapY[y]=(p>=0 && p<480)?p:-1;
    }
    for(int x=0;x<640;++x) sceneX[x]=int(std::floor((x+0.5-320)*sceneScale+w/2.0));
    for(int y=0;y<480;++y) sceneY[y]=std::min(h-1,int((y+0.5)*sceneScale));
    char msg[160];std::snprintf(msg,sizeof msg,"native viewport %dx%d; logical camera %dx240; uniform scale %.6f",w,h,logicalWidth,sceneScale);status(msg);
}
bool windowViewport() {
    RECT r{};HWND hwnd=at<HWND>(0x5ce858);
    if(hwnd && GetClientRect(hwnd,&r)) {
        if(r.right<=0 || r.bottom<=0) return false;
        hookWindow(hwnd);
        viewport(r.right,r.bottom);
    } else viewport(GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN));
    return true;
}
bool highMode() {
    return enabled && at<uint8_t>(0x5a292a) && at<int>(0x5a2934)==640 && at<int>(0x5a2938)==480;
}
void startFrame() {
    frameActive=true;begun=false;worldReady=false;displayReady=false;
    primitives=0;batches=0;
}
int __cdecl frameHook() {
    if(!highMode() || !windowViewport()) { worldReady=false;displayReady=false;return fn<DrawList>(0x45ac50)(); }
    LARGE_INTEGER begin{},end{},frequency{};QueryPerformanceCounter(&begin);
    startFrame();int previous=at<int>(0x591c0c);
    // Projection centre AND scene culling, only during the rendering phase.
    at<int>(0x591c0c)=logicalWidth;
    int result=fn<DrawList>(0x45ac50)();
    at<int>(0x591c0c)=previous;frameActive=false;
    QueryPerformanceCounter(&end);QueryPerformanceFrequency(&frequency);
    static LONGLONG interval=0,total=0,lastEnd=0;static unsigned samples=0;
    if(!interval || begin.QuadPart-lastEnd>frequency.QuadPart/2) {interval=begin.QuadPart;total=0;samples=0;}
    lastEnd=end.QuadPart;
    total+=end.QuadPart-begin.QuadPart;++samples;
    if(end.QuadPart-interval>=frequency.QuadPart*5) {
        char message[180];
        std::snprintf(message,sizeof message,"render cadence %.2f fps; rendering %.3f ms/frame; %dx%d; %u samples",
            samples*double(frequency.QuadPart)/(end.QuadPart-interval),total*1000.0/frequency.QuadPart/samples,width,height,samples);
        status(message);interval=end.QuadPart;total=0;samples=0;
    }
    return result;
}
int __cdecl worldPresentHook() {
    int previous=at<int>(0x591c0c);
    if(frameActive) at<int>(0x591c0c)=320;
    int result=fn<DrawList>(0x467b80)();
    at<int>(0x591c0c)=previous;return result;
}

int __fastcall horizonLoadHook(const char* path) {
    int result=fn<HorizonLoad>(0x468560)(path);
    horizon.clear();horizonWidth=at<int>(0x5ce340);horizonRows=0;
    FILE* f=nullptr;
    if(path && horizonWidth>0 && fopen_s(&f,path,"rb")==0 && f) {
        std::fseek(f,0,SEEK_END);long size=std::ftell(f);std::rewind(f);
        if(size>0) {
            horizonRows=int(size/horizonWidth);horizon.resize(size_t(horizonRows)*horizonWidth);
            if(std::fread(horizon.data(),1,horizon.size(),f)!=horizon.size()) {horizon.clear();horizonRows=0;}
        }
        std::fclose(f);
    }
    return result;
}
void __fastcall horizonDrawHook(int scroll,int y) {
    horizonScroll=scroll;horizonY=y;fn<HorizonDraw>(0x469770)(scroll,y);
}
int wrap(int x,int n) { int r=x%n;return r<0?r+n:r; }
void paintBackdrop(const uint8_t* pixels,int pitch) {
    if(horizonRows>0)for(int x=0;x<width;++x)panoramaX[x]=wrap(backdropX[x]-horizonScroll,horizonWidth);
    int previous=-1;
    for(int y=0;y<height;++y) {
        int oldY=std::clamp(int((y+0.5)/sceneScale),0,479);
        uint8_t* out=world.data()+size_t(y)*width;
        if(oldY==previous) {std::memcpy(out,out-width,width);continue;}
        previous=oldY;
        int hy=oldY-horizonY;
        if(horizonRows>0 && hy>=0 && hy<horizonRows) {
            auto src=horizon.data()+size_t(hy)*horizonWidth;
            for(int x=0;x<width;++x)out[x]=src[panoramaX[x]];
        } else {
            auto src=pixels+size_t(oldY)*pitch;
            for(int x=0;x<width;++x)out[x]=src[std::clamp(backdropX[x],0,639)];
        }
    }
}
bool beginWorld() {
    if(begun) return true;
    if(!frameActive || !at<uint8_t>(0x5ce34c)) return false;
    auto pixels=at<const uint8_t*>(0x6036bc);int pitch=at<int>(0x6036b8);
    if(!pixels || pitch<640) return false;
    paintBackdrop(pixels,pitch);begun=true;return true;
}
Vertex vertex(double x,double y,double u=0,double v=0) {
    return {x*sceneScale+sceneOffset,y*sceneScale,u,v};
}
void drawNode(uint8_t* node) {
    auto resource=*reinterpret_cast<uint8_t**>(node+4);
    if(!resource) return;
    int flags=*reinterpret_cast<int*>(resource+12);
    const auto source=reinterpret_cast<const int32_t*>(node-0x68+0x44);
    Vertex p[4];
    for(int i=0;i<4;++i)p[i]={source[i*2]/32768.0,source[i*2+1]/32768.0};
    bool flat=(flags & at<int>(0x5a29f4))!=0,flipX=node[61]!=0;
    if(!rrhd::normalize(p,flat?0:flags,flipX) && !flat)return;
    node[61]=uint8_t(flipX);
    for(auto& v:p)v=vertex(v.x,v.y);
    if(flat) {
        uint8_t color=*reinterpret_cast<int*>(resource)==16?node[60]:resource[44];
        enqueueQuad(p,{},color);++primitives;return;
    }
    if(flags & 0x2000) fn<Actor>(0x458f30)(node);
    resource=*reinterpret_cast<uint8_t**>(node+4);
    auto bounds=reinterpret_cast<const int*>(node+8);
    int tw=*reinterpret_cast<int*>(resource+32),th=*reinterpret_cast<int*>(resource+36);
    int levels=*reinterpret_cast<int*>(resource+40);
    if(tw<=0 || tw>256 || th<=0 || levels<=0 || levels>16) return;
    double ul=node[61]?bounds[2]-1:bounds[0]+1,ur=node[61]?bounds[0]+1:bounds[2]-1;
    double vt=node[62]?bounds[3]-1:bounds[1]+1,vb=node[62]?bounds[1]+1:bounds[3]-1;
    p[0].u=ul;p[0].v=vt;p[1].u=ur;p[1].v=vt;
    p[2].u=ur;p[2].v=vb;p[3].u=ul;p[3].v=vb;
    double xmin=p[0].x,xmax=xmin,ymin=p[0].y,ymax=ymin;
    for(const auto& v:p) {xmin=std::min(xmin,v.x);xmax=std::max(xmax,v.x);ymin=std::min(ymin,v.y);ymax=std::max(ymax,v.y);}
    auto choose=[levels](double dst,int src) {
        int l=0;double size=(std::abs(src)+1)*0.5;
        while(l+1<levels && dst<size){++l;size*=0.5;}return l;
    };
    int mip=std::min(choose(xmax-xmin+1,bounds[2]-bounds[0]),choose(ymax-ymin+1,bounds[3]-bounds[1]));
    auto data=*reinterpret_cast<const uint8_t* const*>(resource+56+12*mip);
    if(data) {
        double divisor=double(1<<mip);for(auto& v:p){v.u/=divisor;v.v/=divisor;}
        Texture t{data,std::max(1,tw>>mip),std::max(1,th>>mip),(flags&2)!=0};
        enqueueQuad(p,t,0,(flags&0x2000)!=0);++primitives;
    }
}
void copyLegacyView() {
    auto pixels=at<uint8_t*>(0x6036bc);int pitch=at<int>(0x6036b8);
    for(int y=0;y<480;++y)for(int x=0;x<640;++x) {
        int sx=sceneX[x];uint8_t c=(sx>=0 && sx<width)?world[size_t(sceneY[y])*width+sx]:0;
        pixels[size_t(y)*pitch+x]=c;baseline[y*640+x]=c;
    }
    worldReady=true;
}
int __cdecl listHook() {
    if(!frameActive) return fn<DrawList>(0x4688e0)();
    // Retain DirectDraw locking, bypass the legacy 640-pixel cull and raster.
    auto head=at<uint8_t*>(0x6185e0);at<uint8_t*>(0x6185e0)=nullptr;
    int result=fn<DrawList>(0x4688e0)();at<uint8_t*>(0x6185e0)=head;
    if(!beginWorld()) return result;
    while(auto node=at<uint8_t*>(0x6185e0)) {
        drawNode(node);at<uint8_t*>(0x6185e0)=*reinterpret_cast<uint8_t**>(node);
    }
    runRasterJobs();++batches;copyLegacyView();return result;
}

void uiSpan(int y,int left,int right,uint32_t color) {
    int x0=uiX[left],x1=uiX[right];
    for(int yy=uiY[y];yy<uiY[y+1];++yy)
        std::fill(rgba.begin()+size_t(yy)*width+x0,rgba.begin()+size_t(yy)*width+x1,color);
}
void captionOverlay(const uint32_t* finalText) {
    for(int y=0;y<480;++y) {
        auto src=finalText+y*640;auto before=beforeText.data()+y*640;
        if(std::memcmp(src,before,640*4)==0)continue;
        for(int x=0;x<640;) {
            if(src[x]==before[x]) {++x;continue;}
            uint32_t c=src[x];int end=x+1;
            while(end<640 && src[end]==c && src[end]!=before[end])++end;
            uiSpan(y,x,end,c);x=end;
        }
    }
}
void compose(const uint8_t* pixels,int pitch,const uint8_t* palette) {
    uint32_t colors[256];
    for(int i=0;i<256;++i)colors[i]=(uint32_t(palette[i*4])<<16)|(uint32_t(palette[i*4+1])<<8)|palette[i*4+2];
    for(int y=0;y<480;++y)for(int x=0;x<640;++x)beforeText[y*640+x]=colors[pixels[size_t(y)*pitch+x]];
    size_t i=0,count=world.size();
    for(;i+4<=count;i+=4) {
        rgba[i]=colors[world[i]];rgba[i+1]=colors[world[i+1]];
        rgba[i+2]=colors[world[i+2]];rgba[i+3]=colors[world[i+3]];
    }
    for(;i<count;++i)rgba[i]=colors[world[i]];
    for(int y=0;y<480;++y) {
        auto src=pixels+size_t(y)*pitch;auto before=baseline.data()+y*640;
        if(std::memcmp(src,before,640)==0)continue;
        for(int x=0;x<640;) {
            if(src[x]==before[x]) {++x;continue;}
            uint8_t c=src[x];int end=x+1;
            while(end<640 && src[end]==c && src[end]!=before[end])++end;
            uiSpan(y,x,end,colors[c]);x=end;
        }
    }
    displayReady=true;
}
void __fastcall present8Hook(const uint8_t* pixels,int pitch,const uint8_t* palette) {
    displayReady=false;
    if(worldReady && pixels && palette && pitch>=640) {
        compose(pixels,pitch,palette);++frames;
        if(frames==1) {char msg[180];std::snprintf(msg,sizeof msg,"first native frame: %dx%d, %llu scene primitives, %llu batches",width,height,primitives,batches);status(msg);}
    }
    fn<Present8>(0x46e630)(pixels,pitch,palette);
}

// Capture artwork before text. The complete original menu stays fitted in the
// centre; extra space reflects actual artwork, with gradually softened detail.
uint16_t* __fastcall menuDecodeHook(const char* name) {
    auto raw=fn<MenuDecode>(0x435980)(name);
    if(raw) {
        menuBackdropDirty=true;
        menuBright.resize(640*480);menuDim.resize(640*480);
        auto convert=fn<const uint16_t*>(0x5b0170);
        for(size_t i=0;i<menuBright.size();++i){menuBright[i]=convert[raw[i]];menuDim[i]=convert[(raw[i]>>1)&0x3def];}
    }
    return raw;
}
uint32_t rgb16(uint16_t p,int r,int g,int b) {
    auto channel=[p](int mask) {if(!mask)return 0u;int shift=0;while(!(mask&1)){mask>>=1;++shift;}return ((p>>shift)&mask)*255u/mask;};
    return (channel(r)<<16)|(channel(g)<<8)|channel(b);
}
void prepare16(int r,int g,int b) {
    if(colorMasks[0]==r && colorMasks[1]==g && colorMasks[2]==b)return;
    colorMasks[0]=r;colorMasks[1]=g;colorMasks[2]=b;menuBackdropDirty=true;
    for(int i=0;i<65536;++i)color16[i]=rgb16(uint16_t(i),r,g,b);
}
uint32_t mixColor(uint32_t a,uint32_t b,unsigned weight) {
    unsigned inverse=256-weight;
    return ((((a&0xff00ff)*inverse+(b&0xff00ff)*weight)>>8)&0xff00ff)|
           ((((a&0x00ff00)*inverse+(b&0x00ff00)*weight)>>8)&0x00ff00);
}
void prepareMenuBackdrop(bool dim) {
    if(!menuBackdropDirty && menuBackdropDim==dim && menuBackdrop.size()==rgba.size())return;
    const auto& source=dim?menuDim:menuBright;
    std::array<std::vector<uint32_t>,4> softened;
    for(auto& level:softened)level.resize(640*480);
    std::vector<std::array<unsigned,3>> integral(641*481);
    for(int y=0;y<480;++y) {
        std::array<unsigned,3> sum{};
        for(int x=0;x<640;++x) {
            uint32_t c=color16[source[y*640+x]];softened[0][y*640+x]=c;
            for(int channel=0;channel<3;++channel) {
                sum[channel]+=(c>>(channel*8))&255;
                integral[(y+1)*641+x+1][channel]=integral[y*641+x+1][channel]+sum[channel];
            }
        }
    }
    const int radii[]={0,2,5,10};
    for(int level=1;level<4;++level)for(int y=0;y<480;++y)for(int x=0;x<640;++x) {
        int radius=radii[level],x0=std::max(0,x-radius),x1=std::min(640,x+radius+1);
        int y0=std::max(0,y-radius),y1=std::min(480,y+radius+1);
        unsigned count=(x1-x0)*(y1-y0);uint32_t color=0;
        for(int channel=0;channel<3;++channel) {
            unsigned sum=integral[y1*641+x1][channel]+integral[y0*641+x0][channel]
                        -integral[y0*641+x1][channel]-integral[y1*641+x0][channel];
            color|=(sum/count)<<(channel*8);
        }
        softened[level][y*640+x]=color;
    }
    // Reflect at the exact fitted artwork boundary, not a stretched edge strip.
    // Zero blur at the join avoids a rectangular brightness/detail discontinuity.
    // Coordinates repeat for unusually wide/tall windows without clipping art.
    struct Sample {int pixel;double distance;};
    auto samples=[](int count,int original,double scale,double offset) {
        std::vector<Sample> result(count);
        for(int i=0;i<count;++i) {
            double p=(i+0.5-offset)/scale;
            int pixel=wrap(int(std::floor(p)),original*2);
            result[i]={pixel<original?pixel:original*2-1-pixel,std::max({0.0,-p,p-original})};
        }
        return result;
    };
    double scale=std::min(width/640.0,height/480.0);
    auto xs=samples(width,640,scale,(width-640*scale)/2);
    auto ys=samples(height,480,scale,(height-480*scale)/2);
    menuBackdrop.assign(rgba.size(),0);
    for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
        if(mapX[x]>=0 && mapY[y]>=0)continue;
        double softness=std::min(3.0,std::max(xs[x].distance,ys[y].distance)/64.0);
        int level=std::min(2,int(softness));unsigned weight=unsigned((softness-level)*256);
        size_t src=ys[y].pixel*640+xs[x].pixel;
        menuBackdrop[size_t(y)*width+x]=mixColor(softened[level][src],softened[level+1][src],weight);
    }
    menuBackdropDim=dim;menuBackdropDirty=false;
}
void composeMenu(const void* pixels,int pitch,int r,int g,int b) {
    if(menuBright.size()!=640*480) return;
    // Match the original bright/dim background state without copying captions
    // or hover effects into the extension. Unchanged art dominates this sample.
    int brightMatches=0,dimMatches=0;
    for(int y=8;y<480;y+=16)for(int x=8;x<640;x+=16) {
        uint16_t p=reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(pixels)+size_t(y)*pitch)[x];
        brightMatches+=p==menuBright[y*640+x];dimMatches+=p==menuDim[y*640+x];
    }
    prepare16(r,g,b);prepareMenuBackdrop(dimMatches>brightMatches);
    for(int y=0;y<480;++y)for(int x=0;x<640;++x) {
        size_t i=y*640+x;uint16_t p=reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(pixels)+size_t(y)*pitch)[x];
        beforeText[i]=color16[p];
    }
    std::memcpy(rgba.data(),menuBackdrop.data(),rgba.size()*4);
    int previous=-1,left=uiX[0],right=uiX[640];
    for(int y=uiY[0];y<uiY[480];++y) {
        auto dst=rgba.data()+size_t(y)*width;
        if(mapY[y]==previous) {std::memcpy(dst+left,dst-width+left,(right-left)*4);continue;}
        previous=mapY[y];auto src=beforeText.data()+previous*640;
        for(int x=left;x<right;++x)dst[x]=src[mapX[x]];
    }
    displayReady=true;
}
void __fastcall present16Hook(const void* pixels,int pitch,int r,int g,int b) {
    worldReady=false;displayReady=false;
    if(enabled && pixels && pitch>=1280 && at<int>(0x5ce85c)==640 && at<int>(0x5ce860)==480 && windowViewport())
        composeMenu(pixels,pitch,r,g,b);
    fn<Present16>(0x46e470)(pixels,pitch,r,g,b);
}
int WINAPI stretchHook(HDC dc,int x,int y,int w,int h,int sx,int sy,int sw,int sh,
                       const void* bits,const BITMAPINFO* bmi,UINT usage,DWORD rop) {
    if(displayReady && bits==at<void*>(0x5ce864) && sw==640 && sh==480 && !sx && !sy) {
        captionOverlay(static_cast<const uint32_t*>(bits));
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        int rows=originalStretch(dc,0,0,clientWidth,clientHeight,0,0,width,height,rgba.data(),&info,DIB_RGB_COLORS,rop);
        if(rows>0)completedFrame=true;
        return rows;
    }
    return originalStretch(dc,x,y,w,h,sx,sy,sw,sh,bits,bmi,usage,rop);
}

int WINAPI fillHook(HDC dc,const RECT* rect,HBRUSH brush) {
    uintptr_t caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-base+0x400000;
    // Both original presentation routines clear their old letterbox directly
    // on the visible DC. Suppress exactly those clears, never HUD/control fills.
    if(displayReady && ((caller>=0x46ded0 && caller<0x46e074) ||
                        (caller>=0x46e630 && caller<0x46e920)))return 1;
    return originalFill(dc,rect,brush);
}

bool initialize(HMODULE module) {
    base=reinterpret_cast<uintptr_t>(module);
    struct Hook {uintptr_t thunk,original;void* hook;};
    Hook hooks[]={
        {0x4058da,0x45ac50,reinterpret_cast<void*>(&frameHook)},
        {0x40132f,0x4688e0,reinterpret_cast<void*>(&listHook)},
        {0x4049a3,0x467b80,reinterpret_cast<void*>(&worldPresentHook)},
        {0x4017b7,0x468560,reinterpret_cast<void*>(&horizonLoadHook)},
        {0x4028ab,0x469770,reinterpret_cast<void*>(&horizonDrawHook)},
        {0x4050d8,0x435980,reinterpret_cast<void*>(&menuDecodeHook)},
        {0x40492b,0x46e630,reinterpret_cast<void*>(&present8Hook)},
        {0x401118,0x46e470,reinterpret_cast<void*>(&present16Hook)}
    };
    for(auto h:hooks)if(!isBranch(h.thunk,0xe9,h.original)){status("unsupported executable; no hooks installed");return false;}
    if(at<uint16_t>(0x45b3c0)!=0xc033 || at<uint16_t>(0x45b3c2)!=0x05c7 ||
       at<uintptr_t>(0x45b3c4)!=base+0x1cd224 || at<uintptr_t>(0x45b3c8)!=base+0x5bd20) {
        status("unsupported resolution menu; no hooks installed");return false;
    }
    originalStretch=at<Stretch>(0x61c15c);originalFill=at<Fill>(0x61c728);
    if(!originalStretch)return false;
    bool ok=true;for(auto h:hooks)ok=branch(h.thunk,h.hook,0xe9)&&ok;
    Stretch hook=&stretchHook;ok=writeMemory(base+0x21c15c,&hook,sizeof hook)&&ok;
    Fill fill=&fillHook;ok=writeMemory(base+0x21c728,&fill,sizeof fill)&&ok;
    ok=branch(0x45b3c0,reinterpret_cast<void*>(&resolutionMenuHook),0xe9)&&ok;
    status(ok?"RoadRashHD v4: subpixel geometry, textured menu extension, parallel native raster and resolution menu":"hook installation failed");return ok;
}
}

extern "C" __declspec(dllexport) void __cdecl RRHD_Init(HMODULE module) {
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&RRHD_Init),&rendererModule);
    wchar_t path[32768]{};GetModuleFileNameW(module,path,32768);
    std::wstring directory(path);directory.resize(directory.find_last_of(L"\\/")+1);
    configPath=directory+L"RoadRashHD.ini";logPath=directory+L"RoadRashHD.log";
    enabled=GetPrivateProfileIntW(L"Graphics",L"Enabled",1,configPath.c_str())!=0;
    renderHeight=GetPrivateProfileIntW(L"Graphics",L"RenderHeight",0,configPath.c_str());
    if(enabled)initialize(module);
}
