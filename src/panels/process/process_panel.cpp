#include "process_panel.hpp"
#include "../../region_canvas.hpp"
#include "../../stack_depth.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// Match the SSH ornament: warm ivory strokes and black negative space.
constexpr unsigned white=0xf2f0e9, dim=white, cyan=white;
constexpr unsigned stackColor=white, mapColor=white, heapColor=white, textColor=white;
void outline(RegionCanvas& canvas, unsigned x, unsigned y, unsigned w,
             unsigned h, unsigned color=white) noexcept {
    canvas.fillRectangle({x,y,w,1},color);
    canvas.fillRectangle({x,y+h-1,w,1},color);
    canvas.fillRectangle({x,y,1,h},color);
    canvas.fillRectangle({x+w-1,y,1,h},color);
}
void divider(RegionCanvas& canvas, unsigned y) noexcept {
    canvas.fillRectangle({12,y,136,1},white);
    canvas.fillRectangle({168,y,136,1},white);
    for (int i=0;i<5;++i) {
        canvas.pixel(158-i,int(y)-4+i,white);
        canvas.pixel(158+i,int(y)-4+i,white);
        canvas.pixel(158-i,int(y)+4-i,white);
        canvas.pixel(158+i,int(y)+4-i,white);
    }
}
void sizeLabel(char* out, std::size_t capacity, std::uint64_t bytes) noexcept {
    if (bytes>=1024ull*1024*1024) std::snprintf(out,capacity,"%.1F GIB",double(bytes)/(1024*1024*1024));
    else if (bytes>=1024*1024) std::snprintf(out,capacity,"%.1F MIB",double(bytes)/(1024*1024));
    else std::snprintf(out,capacity,"%.0F KIB",double(bytes)/1024);
}
void meter(RegionCanvas& canvas, unsigned y, double percent, unsigned color) noexcept {
    const double level=std::clamp(percent,0.,100.)*32/100;
    for(unsigned i=0;i<32;++i) {
        const unsigned x=12+i*9;
        outline(canvas,x,y,6,7,color);
        if (double(i)<level) canvas.fillRectangle({x+2,y+2,2,3},color);
    }
}
void band(RegionCanvas& canvas, unsigned y, unsigned height, const char* name,
          const MemoryRegion& region, unsigned color, bool valid) noexcept {
    char value[40];
    if (valid && region.bytes) std::snprintf(value,sizeof(value),"%012llX",static_cast<unsigned long long>(region.low));
    else std::snprintf(value,sizeof(value),"--");
    canvas.text(12,int(y)+4,value,dim);
    outline(canvas,105,y,199,height,color);
    outline(canvas,108,y+3,193,height-6,color);
    canvas.text(115,int(y)+7,name,color);
    if (valid) sizeLabel(value,sizeof(value),region.bytes);
    else std::snprintf(value,sizeof(value),"UNAVAILABLE");
    canvas.text(115,int(y)+23,value,color);
}
void gap(RegionCanvas& canvas,unsigned y,unsigned height,bool down) noexcept {
    for (unsigned row=0;row<height;row+=4) {
        canvas.fillRectangle({105,y+row,1,2},dim);
        canvas.fillRectangle({303,y+row,1,2},dim);
    }
    const unsigned x=284;
    canvas.fillRectangle({x,y+2,1,height-4},dim);
    for(unsigned i=0;i<4;++i)
        canvas.fillRectangle({x-i,down ? y+height-5-i : y+3+i,1+2*i,1},dim);
}
const char* roleName(DrmDevice::BufferRole role) noexcept {
    switch(role) {
    case DrmDevice::BufferRole::Front: return "FRONT";
    case DrmDevice::BufferRole::Pending: return "PENDING";
    case DrmDevice::BufferRole::Drawing: return "DRAWING";
    case DrmDevice::BufferRole::Available: return "BACK";
    default: return "UNMAPPED";
    }
}
}

bool ProcessPanel::update(Clock::time_point now) noexcept
{
    StackDepth::observe();
    const bool previousValid=snapshot_.stackDepthValid;
    const auto previousDepth=snapshot_.stackDepth;
    bool changed=false;
    if (!animationStarted_ || now>=nextSample_) {
        snapshot_=reader_.sample(now);
        buffers_=device_.bufferInfo();
        for(std::size_t i=1;i<history_.size();++i) history_[i-1]=history_[i];
        history_.back()=snapshot_.cpuReady ? float(snapshot_.cpuPercent) : 0;
        nextSample_=now+std::chrono::seconds(1);
        changed=true;
    }

    // Reset each interval so the displayed depth can shrink after calls return.
    // These are sampled application frames, not an exhaustive stack peak.
    const auto pointer=StackDepth::takeLowest();
    const auto& stack=snapshot_.stack;
    snapshot_.stackDepthValid=snapshot_.mapsValid && pointer &&
        pointer>=stack.low && pointer<stack.high;
    snapshot_.stackDepth=snapshot_.stackDepthValid ? stack.high-pointer : 0;
    const double target=double(snapshot_.stackDepth);
    // A visible zoom scale, not the stack limit. Keep it stable after expansion.
    while (target>stackScale_) stackScale_*=2;
    const double elapsed=animationStarted_ ?
        std::max(0.,std::chrono::duration<double>(now-previousAnimation_).count()) : 0.;
    const double previousDisplay=displayedStackDepth_;
    if (snapshot_.stackDepthValid) {
        displayedStackDepth_+=(target-displayedStackDepth_)*(1-std::exp(-elapsed/.12));
        if (std::abs(target-displayedStackDepth_)<8) displayedStackDepth_=target;
    } else displayedStackDepth_=0;
    previousAnimation_=now;
    animationStarted_=true;
    next_=now+std::chrono::milliseconds(33);
    return changed || previousValid!=snapshot_.stackDepthValid ||
        previousDepth!=snapshot_.stackDepth || previousDisplay!=displayedStackDepth_;
}

void ProcessPanel::paint(RegionCanvas& canvas) const noexcept
{
    // Telemetry is consumed by the next update; this paint's snapshot stays fixed.
    StackDepth::observe();
    const auto& s=snapshot_;
    char line[80], value[32];
    canvas.text(117,12,"PROCESS",white,2);
    // Paired angular corners echo the SSH panel's split ornament strokes.
    for(unsigned inset : {0u,5u}) {
        canvas.fillRectangle({12+inset,10+inset,27-inset,2},white);
        canvas.fillRectangle({12+inset,10+inset,2,17-inset},white);
        canvas.fillRectangle({277,10+inset,27-inset,2},white);
        canvas.fillRectangle({302-inset,10+inset,2,17-inset},white);
    }
    std::snprintf(line,sizeof(line),"LIBREGLASS / PID %u",s.pid);
    canvas.text(12,34,line,dim);
    if(s.cpuReady) std::snprintf(line,sizeof(line),"SYSTEM CPU %.1F%%",s.cpuPercent);
    else std::snprintf(line,sizeof(line),"SYSTEM CPU %s",s.cpuValid ? "WARMUP" : "UNAVAILABLE");
    canvas.text(12,56,line,white);
    meter(canvas,72,s.cpuReady ? s.cpuPercent : 0,cyan);
    const double ramPercent=s.ramValid ? 100.*double(s.ramUsed)/s.ramTotal : 0;
    if(s.ramValid) {
        sizeLabel(value,sizeof(value),s.ramUsed);
        std::snprintf(line,sizeof(line),"RAM %.1F%% / %s",ramPercent,value);
    } else std::snprintf(line,sizeof(line),"RAM UNAVAILABLE");
    canvas.text(12,90,line,white); meter(canvas,106,ramPercent,stackColor);
    if(s.faultsReady) std::snprintf(line,sizeof(line),"SELF CPU %.1F%% / %u THREADS",s.processCpuPercent,s.threads);
    else std::snprintf(line,sizeof(line),"SELF CPU -- / %u THREADS",s.threads);
    canvas.text(12,124,line,dim);
    divider(canvas,140);
    canvas.text(12,150,"ADDRESS",dim); canvas.text(105,150,"VIRTUAL SPACE / SCHEMATIC",dim);
    band(canvas,171,39,"STACK DEPTH",s.stack,stackColor,s.mapsValid);
    canvas.fillRectangle({114,193,162,9},0);
    if (s.stackDepthValid) {
        std::snprintf(value,sizeof(value),"%.1FK / %.0FK VIEW",
            double(s.stackDepth)/1024,stackScale_/1024);
        const unsigned depth=static_cast<unsigned>(std::lround(
            std::clamp(displayedStackDepth_/stackScale_,0.,1.)*24));
        // The occupied span grows downward, matching the stack's direction.
        outline(canvas,282,177,16,26,stackColor);
        if (depth) canvas.fillRectangle({284,178,12,depth},stackColor);
        canvas.fillRectangle({278,178+depth,23,1},stackColor);
    } else std::snprintf(value,sizeof(value),"DEPTH UNAVAILABLE");
    canvas.text(115,194,value,stackColor);
    gap(canvas,210,16,true);
    band(canvas,226,49,"MMAP / LIBS / DATA",s.mappings,mapColor,s.mapsValid);
    gap(canvas,275,24,false);
    band(canvas,299,39,"HEAP [BRK]",s.heap,heapColor,s.mapsValid);
    gap(canvas,338,13,false);
    band(canvas,351,39,"TEXT [EXECUTABLE]",s.text,textColor,s.mapsValid);
    // Latest 32 system CPU samples, oldest at the left.
    for(unsigned i=0;i<history_.size();++i) {
        const unsigned height=static_cast<unsigned>(std::clamp(history_[i],0.f,100.f)*.16f);
        if(height) outline(canvas,12+i*9,414-height,6,height,cyan);
    }
    sizeLabel(value,sizeof(value),s.rss);
    if(s.processValid) std::snprintf(line,sizeof(line),"RSS %s",value);
    else std::snprintf(line,sizeof(line),"RSS UNAVAILABLE");
    canvas.text(12,423,line,white);
    sizeLabel(value,sizeof(value),s.virtualBytes);
    if(s.processValid) std::snprintf(line,sizeof(line),"VIRTUAL %s",value);
    else std::snprintf(line,sizeof(line),"VIRTUAL UNAVAILABLE");
    canvas.text(166,423,line,dim);
    if(s.mapsValid && s.mapLimitValid) std::snprintf(line,sizeof(line),"VMAS %u / %u",s.mapCount,s.mapLimit);
    else if(s.mapsValid) std::snprintf(line,sizeof(line),"VMAS %u / --",s.mapCount);
    else std::snprintf(line,sizeof(line),"VMAS UNAVAILABLE");
    canvas.text(12,440,line,dim);
    sizeLabel(value,sizeof(value),s.swap);
    std::snprintf(line,sizeof(line),"SWAP %s",s.processValid ? value : "--");
    canvas.text(166,440,line,dim);
    if(s.faultsReady) std::snprintf(line,sizeof(line),"FAULTS/S MIN %.0F MAJ %.0F",s.minorFaultsPerSecond,s.majorFaultsPerSecond);
    else std::snprintf(line,sizeof(line),"FAULTS/S MIN -- MAJ --");
    canvas.text(12,457,line,dim);
    for(unsigned i=0;i<buffers_.size();++i) {
        const auto& b=buffers_[i]; const unsigned y=476+i*39;
        outline(canvas,12,y,292,34);
        sizeLabel(value,sizeof(value),b.bytes);
        std::snprintf(line,sizeof(line),"DRM %c / %s / %s",'A'+i,roleName(b.role),value);
        canvas.text(19,int(y)+5,line,cyan);
        if(b.role!=DrmDevice::BufferRole::Unmapped)
            std::snprintf(line,sizeof(line),"%012llX-%012llX",
                static_cast<unsigned long long>(b.address),static_cast<unsigned long long>(b.address+b.bytes));
        else std::snprintf(line,sizeof(line),"NO DEVICE MAPPING");
        canvas.text(19,int(y)+19,line,dim);
    }
    if(s.loadValid && s.uptimeValid) std::snprintf(line,sizeof(line),"LOAD %.2F / UP %.1F H",s.load,s.uptime/3600);
    else std::snprintf(line,sizeof(line),"LOAD / UPTIME UNAVAILABLE");
    divider(canvas,557);
    canvas.text(12,566,line,dim);
    canvas.text(12,583,"1 HZ METRICS / 30 HZ STACK",dim);
}
