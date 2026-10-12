#include "process_panel.hpp"
#include "../../region_canvas.hpp"
#include "../../renderer.hpp"
#include "../stack_depth.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
// Match the SSH ornament: warm ivory strokes and black negative space.
constexpr unsigned white=0xf5f5f7, dim=0x929298, accent=0x8eafc2;
constexpr unsigned card=0x121214, track=0x28282c;
using Style=RegionCanvas::TextStyle;

void rounded(RegionCanvas& canvas, int x, int y, int width, int height,
             double radius, unsigned color) noexcept
{
    if (width<=0 || height<=0) return;
    radius=std::min({radius,width*.5,height*.5});
    const int corner=static_cast<int>(std::ceil(radius));
    const int middle=height-2*corner;
    if (middle>0)
        canvas.fillRectangle({unsigned(x),unsigned(y+corner),unsigned(width),unsigned(middle)},color);
    for (int row=0; row<corner; ++row) {
        if (width>2*corner) {
            canvas.fillRectangle({unsigned(x+corner),unsigned(y+row),unsigned(width-2*corner),1},color);
            canvas.fillRectangle({unsigned(x+corner),unsigned(y+height-1-row),unsigned(width-2*corner),1},color);
        }
        for (int col=0; col<corner; ++col) {
            const double dx=std::max(radius-col-.5,0.);
            const double dy=std::max(radius-row-.5,0.);
            const auto opacity=static_cast<unsigned char>(
                std::clamp(radius+.5-std::sqrt(dx*dx+dy*dy),0.,1.)*255);
            canvas.blendPixel(x+col,y+row,color,opacity);
            if (width-1-col!=col) canvas.blendPixel(x+width-1-col,y+row,color,opacity);
            if (height-1-row!=row) {
                canvas.blendPixel(x+col,y+height-1-row,color,opacity);
                if (width-1-col!=col)
                    canvas.blendPixel(x+width-1-col,y+height-1-row,color,opacity);
            }
        }
    }
}

void right(RegionCanvas& canvas, int edge, int y, const char* value,
           unsigned color=white, Style style=Style::Body) noexcept
{
    canvas.label(edge-RegionCanvas::textWidth(value,style),y,value,color,style);
}

void sizeLabel(char* out, std::size_t capacity, std::uint64_t bytes) noexcept
{
    if (bytes>=1024ull*1024*1024) std::snprintf(out,capacity,"%.1f GiB",double(bytes)/(1024*1024*1024));
    else if (bytes>=1024*1024) std::snprintf(out,capacity,"%.1f MiB",double(bytes)/(1024*1024));
    else std::snprintf(out,capacity,"%.0f KiB",double(bytes)/1024);
}

void band(RegionCanvas& canvas, int y, const char* name, const MemoryRegion& region,
          bool valid) noexcept
{
    rounded(canvas,16,y,284,43,7,card);
    canvas.label(25,y+5,name,white);
    char value[80];
    if (valid && region.bytes) sizeLabel(value,sizeof(value),region.bytes);
    else std::snprintf(value,sizeof(value),"--");
    right(canvas,291,y+5,value,dim);
    if (valid && region.bytes)
        std::snprintf(value,sizeof(value),"0x%llx - 0x%llx",
            static_cast<unsigned long long>(region.low),
            static_cast<unsigned long long>(region.high));
    else std::snprintf(value,sizeof(value),"--");
    canvas.label(25,y+23,value,dim,Style::Address);
}

const char* roleName(DrmDevice::BufferRole role) noexcept
{
    switch(role) {
    case DrmDevice::BufferRole::Front: return "Front";
    case DrmDevice::BufferRole::Pending: return "Pending";
    case DrmDevice::BufferRole::Drawing: return "Drawing";
    case DrmDevice::BufferRole::Available: return "Back";
    default: return "Unmapped";
    }
}

void bufferMap(RegionCanvas& canvas, int x, int y) noexcept
{
    constexpr int width=132, height=75;
    rounded(canvas,x,y,width,height,5,track);
    for (std::size_t i=0; i<Renderer::PanelCount; ++i) {
        const auto bounds=Renderer::panelBounds(i);
        const int left=x+int(bounds.y)*width/1920;
        const int top=y+int(1080-bounds.x-bounds.width)*height/1080;
        const int w=int(bounds.height)*width/1920;
        const int h=int(bounds.width)*height/1080;
        rounded(canvas,left,top,w,h,3,card);
        char number[4]; std::snprintf(number,sizeof(number),"%zu",i);
        canvas.label(left+(w-RegionCanvas::textWidth(number))/2,top+(h-14)/2,number,white);
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
    char line[96], value[32], total[32];
    canvas.label(16,22,"CPU",white,Style::Heading);
    if (s.cpuReady) std::snprintf(value,sizeof(value),"%.1f%%",s.cpuPercent);
    else std::snprintf(value,sizeof(value),"--");
    right(canvas,300,12,value,white,Style::Value);
    // Latest 32 system CPU samples, oldest at the left.
    for (std::size_t i=1; i<history_.size(); ++i) {
        const double start=std::clamp(double(history_[i-1]),0.,100.)*.30;
        const double end=std::clamp(double(history_[i]),0.,100.)*.30;
        for (int x=0; x<9; ++x) {
            const double y=78-(start+(end-start)*x/9);
            const int row=int(std::floor(y));
            const double fraction=y-row;
            canvas.blendPixel(16+int(i-1)*9+x,row,accent,
                static_cast<unsigned char>((1-fraction)*255));
            canvas.blendPixel(16+int(i-1)*9+x,row+1,accent,
                static_cast<unsigned char>(fraction*255));
        }
    }
    canvas.label(16,102,"RAM",white,Style::Heading);
    const double ramPercent=s.ramValid ? 100.*double(s.ramUsed)/s.ramTotal : 0;
    if (s.ramValid) std::snprintf(value,sizeof(value),"%.1f%%",ramPercent);
    else std::snprintf(value,sizeof(value),"--");
    right(canvas,300,92,value,white,Style::Value);
    rounded(canvas,16,128,284,5,2.5,track);
    const int filled=int(std::lround(std::clamp(ramPercent,0.,100.)*284/100));
    if (filled) rounded(canvas,16,128,filled,5,std::min(2.5,filled*.5),accent);
    if (s.ramValid) {
        sizeLabel(value,sizeof(value),s.ramUsed);
        sizeLabel(total,sizeof(total),s.ramTotal);
        std::snprintf(line,sizeof(line),"%s / %s",value,total);
    } else std::snprintf(line,sizeof(line),"--");
    canvas.label(16,139,line,dim);
    // Paired angular corners echo the SSH panel's split ornament strokes.
    canvas.fillRectangle({16,169,284,1},track);
    canvas.label(16,181,"Process memory",white,Style::Heading);
    if (s.processValid) {
        sizeLabel(value,sizeof(value),s.rss);
        std::snprintf(line,sizeof(line),"%s resident",value);
    } else std::snprintf(line,sizeof(line),"--");
    right(canvas,300,183,line,dim);
    band(canvas,209,"Stack",s.stack,s.mapsValid);
        // The occupied span grows downward, matching the stack's direction.
    const unsigned depth=static_cast<unsigned>(std::lround(
        std::clamp(s.stack.bytes ? displayedStackDepth_/s.stack.bytes : 0.,0.,1.)*268));
    canvas.fillRectangle({24,247,268,2},track);
    if (s.stackDepthValid && depth) canvas.fillRectangle({24,247,depth,2},accent);
    band(canvas,258,"Mapped memory",s.mappings,s.mapsValid);
    band(canvas,307,"Heap",s.heap,s.mapsValid);
    band(canvas,356,"Executable",s.text,s.mapsValid);
    canvas.label(16,416,"Buffers",white,Style::Heading);
    for (std::size_t i=0; i<buffers_.size(); ++i) {
        const auto& buffer=buffers_[i];
        const int x=16+int(i)*152;
        std::snprintf(line,sizeof(line),"Buffer %c",'A'+int(i));
        canvas.label(x,444,line,white);
        right(canvas,x+132,444,roleName(buffer.role),dim);
        bufferMap(canvas,x,465);
        if (buffer.role!=DrmDevice::BufferRole::Unmapped) {
            sizeLabel(value,sizeof(value),buffer.bytes);
            canvas.label(x,547,value,dim);
            std::snprintf(line,sizeof(line),"0x%llx",static_cast<unsigned long long>(buffer.address));
            canvas.label(x,564,line,dim,Style::Address);
            std::snprintf(line,sizeof(line),"0x%llx",static_cast<unsigned long long>(buffer.address+buffer.bytes));
            canvas.label(x,578,line,dim,Style::Address);
        } else {
            canvas.label(x,547,"--",dim);
            canvas.label(x,564,"--",dim,Style::Address);
        }
    }
}
