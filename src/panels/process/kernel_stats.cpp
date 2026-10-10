#include "kernel_stats.hpp"
#include "../../stack_depth.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace {
FILE* openProc(const char* root, const char* name) noexcept {
    char path[1024];
    const int n=std::snprintf(path,sizeof(path),"%s/%s",root,name);
    return n>0 && n<int(sizeof(path)) ? std::fopen(path,"r") : nullptr;
}
void addRegion(MemoryRegion& region, std::uint64_t low, std::uint64_t high) noexcept {
    if (high<=low) return;
    if (!region.bytes || low<region.low) region.low=low;
    region.high=std::max(region.high,high);
    region.bytes+=high-low;
}
}

//QA
KernelStats::KernelStats(const char* root) noexcept
    : root_(root), ticksPerSecond_(sysconf(_SC_CLK_TCK)) {}

KernelSnapshot KernelStats::sample(Panel::Clock::time_point now) noexcept
{
    StackDepth::observe();
    KernelSnapshot out;
    char line[4096];
    if (FILE* file=openProc(root_,"stat")) {
        unsigned long long user,nice,system,idle,wait,irq,soft,steal;
        if (std::fgets(line,sizeof(line),file) &&
            std::sscanf(line,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                &user,&nice,&system,&idle,&wait,&irq,&soft,&steal)==8) {
            // guest counters are already included in user/nice; do not add them.
            const std::uint64_t total=user+nice+system+idle+wait+irq+soft+steal;
            const std::uint64_t inactive=idle+wait;
            out.cpuValid=true;
            if (previousCpu_ && total>total_ && inactive>=idle_) {
                const auto elapsed=total-total_;
                const auto quiet=std::min(elapsed,inactive-idle_);
                out.cpuPercent=100.*double(elapsed-quiet)/elapsed;
                out.cpuReady=true;
            }
            total_=total; idle_=inactive; previousCpu_=true;
        } else previousCpu_=false;
        std::fclose(file);
    } else previousCpu_=false;

    if (FILE* file=openProc(root_,"meminfo")) {
        unsigned long long total=0,available=0; bool haveTotal=false,haveAvailable=false;
        while (std::fgets(line,sizeof(line),file)) {
            if (std::sscanf(line,"MemTotal: %llu kB",&total)==1) haveTotal=true;
            if (std::sscanf(line,"MemAvailable: %llu kB",&available)==1) haveAvailable=true;
        }
        out.ramValid=haveTotal && haveAvailable && total>0 && !std::ferror(file);
        if (out.ramValid) {
            out.ramTotal=total*1024;
            out.ramUsed=(total-std::min(total,available))*1024;
        }
        std::fclose(file);
    }
    if (FILE* file=openProc(root_,"self/status")) {
        bool rss=false,vm=false; unsigned long long value;
        while (std::fgets(line,sizeof(line),file)) {
            if (std::sscanf(line,"VmRSS: %llu kB",&value)==1) { out.rss=value*1024; rss=true; }
            if (std::sscanf(line,"VmSize: %llu kB",&value)==1) { out.virtualBytes=value*1024; vm=true; }
            if (std::sscanf(line,"VmSwap: %llu kB",&value)==1) out.swap=value*1024;
            std::sscanf(line,"Pid: %u",&out.pid);
            std::sscanf(line,"Threads: %u",&out.threads);
        }
        out.processValid=rss && vm && !std::ferror(file);
        std::fclose(file);
    }
    if (FILE* file=openProc(root_,"self/stat")) {
        if (std::fgets(line,sizeof(line),file)) {
            // comm can contain spaces and ')' characters. Fields follow its last ')'.
            if (char* end=std::strrchr(line,')')) {
                char* cursor=end+1;
                std::uint64_t user=0,system=0,minor=0,major=0;
                bool valid=true;
                for (unsigned field=3;field<=15;++field) {
                    while (*cursor==' ') ++cursor;
                    if (!*cursor || *cursor=='\n') { valid=false; break; }
                    char* next=cursor;
                    while (*next && *next!=' ' && *next!='\n') ++next;
                    if (field==10 || field==12 || field==14 || field==15) {
                        char* parsed=nullptr;
                        const auto value=std::strtoull(cursor,&parsed,10);
                        if (parsed!=next) { valid=false; break; }
                        if (field==10) minor=value;
                        if (field==12) major=value;
                        if (field==14) user=value;
                        if (field==15) system=value;
                    }
                    cursor=next;
                }
                if (valid && ticksPerSecond_>0) {
                    const auto ticks=user+system;
                    const double seconds=std::chrono::duration<double>(now-previousTime_).count();
                    if (previousProcess_ && seconds>0 && ticks>=processTicks_ && minor>=minor_ && major>=major_) {
                        // 100% means one CPU core, not the whole machine.
                        out.processCpuPercent=100.*double(ticks-processTicks_)/ticksPerSecond_/seconds;
                        out.minorFaultsPerSecond=double(minor-minor_)/seconds;
                        out.majorFaultsPerSecond=double(major-major_)/seconds;
                        out.faultsReady=true;
                    }
                    previousProcess_=true; processTicks_=ticks; minor_=minor; major_=major;
                } else previousProcess_=false;
            } else previousProcess_=false;
        } else previousProcess_=false;
        std::fclose(file);
    } else previousProcess_=false;
    previousTime_=now;

    if (FILE* file=openProc(root_,"self/maps")) {
        char executable[1024]{};
        char path[1024]; std::snprintf(path,sizeof(path),"%s/self/exe",root_);
        const auto n=readlink(path,executable,sizeof(executable)-1);
        if (n>=0) executable[n]=0;
        out.mapsValid=true;
        while (std::fgets(line,sizeof(line),file)) {
            unsigned long long low,high; char permissions[5]{}; int offset=0;
            if (std::sscanf(line,"%llx-%llx %4s %*s %*s %*s %n",&low,&high,permissions,&offset)!=3 || high<=low) {
                out.mapsValid=false; continue;
            }
            ++out.mapCount;
            const char* name=line+offset;
            const auto length=std::strcspn(name,"\n"); line[offset+length]=0;
            if (std::strcmp(name,"[stack]")==0) addRegion(out.stack,low,high);
            else if (std::strcmp(name,"[heap]")==0) addRegion(out.heap,low,high);
            else if (permissions[2]=='x' && executable[0] && std::strcmp(name,executable)==0)
                addRegion(out.text,low,high);
            else addRegion(out.mappings,low,high);
        }
        if (std::ferror(file) || !out.mapCount) out.mapsValid=false;
        std::fclose(file);
    }
    if (FILE* file=openProc(root_,"sys/vm/max_map_count")) {
        out.mapLimitValid=std::fscanf(file,"%u",&out.mapLimit)==1 && out.mapLimit>0; std::fclose(file);
    }
    if (FILE* file=openProc(root_,"loadavg")) {
        out.loadValid=std::fscanf(file,"%lf",&out.load)==1; std::fclose(file);
    }
    if (FILE* file=openProc(root_,"uptime")) {
        out.uptimeValid=std::fscanf(file,"%lf",&out.uptime)==1; std::fclose(file);
    }
    return out;
}
