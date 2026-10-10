#include "ssh_panel.hpp"
#include "../../region_canvas.hpp"
#include "../stack_depth.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {
constexpr float pi = std::numbers::pi_v<float>;
constexpr unsigned ink = 0xf2f0e9;
struct Point { float x, y; };

float smooth(float t) noexcept {
    t = std::clamp(t, 0.f, 1.f);
    return t*t*(3.f-2.f*t);
}
float blink(float t, float start, float duration) noexcept {
    if (t < start || t > start + duration) return 1;
    const float u = (t-start)/duration;
    // Quick closing, a brief shut hold, then a softer reopening.
    if (u < .32f) return 1-smooth(u/.32f);
    if (u < .46f) return 0;
    return smooth((u-.46f)/.54f);
}

// Reference coordinates: 1254-square artwork centered at (627,627).
// These two L strokes and two diagonal strokes are repeated at quarter turns.
constexpr std::array<Point,6> cornerOuter{{
    {194,195},{407,195},{390,229},{228,229},{228,672},{194,637}
}};
constexpr std::array<Point,6> cornerInner{{
    {242,242},{361,242},{337,277},{277,277},{277,691},{242,655}
}};
constexpr std::array<Point,6> diagonalOuter{{
    {310,330},{627,24},{771,175},{717,175},{627,80},{310,386}
}};
constexpr std::array<Point,6> diagonalInner{{
    {310,400},{627,96},{702,175},{651,175},{627,148},{310,454}
}};
// Eight angular crescent blades: long clean facets and tapered spear tips.
// The concave inner edge gives each blade its hook without serrations.
constexpr std::array<Point,9> crescent{{
    {548,300},{661,288},{739,345},{782,423},{780,481},
    {674,550},{702,447},{672,390},{611,343}
}};

// Narrow inset follows the same angular hook, leaving joined pointed ends.
constexpr std::array<Point,12> crescentInset{{
    {582,313},{652,310},{717,364},{748,428},{747,469},{704,515},
    {697,511},{737,465},{737,432},{708,372},{649,320},{582,323}
}};

template<std::size_t N>
void polygon(RegionCanvas& canvas, const std::array<Point,N>& source,
             float angle, float scale, int cx, int cy, unsigned color=ink) noexcept {
    StackDepth::observe();
    std::array<Point,N> points;
    const float co=std::cos(angle), si=std::sin(angle);
    float low=1e9f, high=-1e9f;
    for (std::size_t i=0;i<N;++i) {
        const float x=(source[i].x-627)*scale, y=(source[i].y-627)*scale;
        points[i]={cx+x*co-y*si,cy+x*si+y*co};
        low=std::min(low,points[i].y); high=std::max(high,points[i].y);
    }
    // Scanline spans avoid a trig calculation or canvas call for every pixel.
    for (int y=std::max(0,int(std::floor(low))); y<=std::min(int(canvas.height())-1,int(std::ceil(high)));++y) {
        std::array<float,N> hits{}; unsigned count=0;
        const float scan=y+.5f;
        for (std::size_t i=0,j=N-1;i<N;j=i++) {
            const auto a=points[j], b=points[i];
            if ((a.y<=scan && b.y>scan)||(b.y<=scan && a.y>scan))
                hits[count++]=a.x+(scan-a.y)*(b.x-a.x)/(b.y-a.y);
        }
        // At most one crossing per edge. Insertion sort fits these small arrays
        // and avoids GCC 13's bounds warning in std::sort's 16-element fast path.
        for (unsigned i=1;i<count;++i) {
            const float value=hits[i];
            unsigned j=i;
            while (j && value<hits[j-1]) {
                hits[j]=hits[j-1];
                --j;
            }
            hits[j]=value;
        }
        for (unsigned i=0;i+1<count;i+=2) {
            const int left=std::max(0,int(std::ceil(hits[i]-.5f)));
            const int right=std::min(int(canvas.width())-1,int(std::floor(hits[i+1]-.5f)));
            if (right>=left) canvas.fillRectangle({unsigned(left),unsigned(y),unsigned(right-left+1),1},color);
        }
    }
}
}

void SshPanel::setConnected(bool connected, Clock::time_point now) noexcept
{
    if (connected_ == connected) return;
    // Preserve the current pose so a connection closes the existing eyelid
    // rather than jumping immediately to a different image.
    transitionOpening_ = openness_;
    transitionRotation_ = rotation_;
    connected_ = connected;
    changed_ = true;
    epoch_ = now;
    started_ = true;
    next_ = now;
}

bool SshPanel::update(Clock::time_point now) noexcept
{
    StackDepth::observe();
    if (!started_) { epoch_ = now; started_ = true; }
    const float t=std::max(0.f,std::chrono::duration<float>(now-epoch_).count());
    float opening=0, angle=transitionRotation_, gaze=0, vertical=0;
    if (connected_) {
        if (t < .18f) opening=transitionOpening_*(1-smooth(t/.18f));
        else if (t < .30f) opening=0;
        else opening=smooth((t-.30f)/.65f);
        if (t >= .95f) {
            const float active=t-.95f;
            const float cycle=std::fmod(active,8.7f);
            opening *= blink(cycle,2.1f,.24f)*blink(cycle,6.7f,.30f)*blink(cycle,7.12f,.18f);
            // Ease the wheel up to speed over its first second.
            const float travel=active<1 ? active*active*.5f : active-.5f;
            angle += travel*.8f;
            gaze=.35f*std::sin(active*.63f);
            vertical=.13f*std::sin(active*.91f);
        }
    } else if (t < .35f) {
        opening=transitionOpening_*(1-smooth(t/.35f));
    } else {
        const float cycle=std::fmod(t-.35f,11.4f);
        if (cycle>=1.8f && cycle<6.3f) {
            opening=.48f*smooth((cycle-1.8f)/.55f)*(1-smooth((cycle-5.85f)/.45f));
            opening *= blink(cycle,4.2f,.20f)*blink(cycle,4.53f,.15f);
        } else if (cycle>=7.4f && cycle<9.8f) {
            opening=.60f*smooth((cycle-7.4f)/.35f)*(1-smooth((cycle-9.4f)/.4f));
        }
        // Short saccades between held glances, with two longer closed pauses.
        constexpr std::array<float,10> times{0,2.4f,2.95f,3.7f,4.9f,5.5f,7.6f,8.1f,8.65f,9.3f};
        constexpr std::array<float,10> targets{0,-.75f,.65f,-.25f,.8f,-.6f,.45f,-.85f,.7f,0};
        for (std::size_t i=1;i<times.size();++i) {
            if (cycle<times[i]) break;
            gaze=targets[i-1]+(targets[i]-targets[i-1])*smooth((cycle-times[i])/.12f);
        }
        vertical=.12f*std::sin(cycle*1.7f);
    }
    angle=std::fmod(angle,2*pi);
    const bool changed=changed_ || opening!=openness_ || angle!=rotation_ || gaze!=gaze_ || vertical!=gazeY_;
    openness_=opening; rotation_=angle; gaze_=gaze; gazeY_=vertical;
    next_=now+std::chrono::milliseconds(33);
    changed_=false;
    return changed;
}

void SshPanel::paint(RegionCanvas& canvas) const noexcept
{
    const int cx=int(canvas.width())/2;
    const int cy=int(canvas.height())/2-20;
    const float scale=std::min(canvas.width(),canvas.height())/1254.f*.95f;
    for (int i=0;i<4;++i) {
        const float angle=i*pi/2;
        polygon(canvas,cornerOuter,angle,scale,cx,cy);
        polygon(canvas,cornerInner,angle,scale,cx,cy);
        polygon(canvas,diagonalOuter,angle,scale,cx,cy);
        polygon(canvas,diagonalInner,angle,scale,cx,cy);
    }
    for (int i=0;i<8;++i) polygon(canvas,crescent,rotation_+i*pi/4,scale,cx,cy);
    for (int i=0;i<8;++i) polygon(canvas,crescentInset,rotation_+i*pi/4,scale,cx,cy,0);

    // Almond eye: the upper lid travels down to meet the curved lower lid.
    // The iris remains circular and is clipped by the aperture, not squashed.
    const int halfWidth=std::max(1,int(198*scale));
    const float line=std::max(1.2f,13*scale);
    const float irisRadius=91*scale;
    const float irisX=gaze_*65*scale, irisY=gazeY_*55*scale;
    for (int x=-halfWidth;x<=halfWidth;++x) {
        const float u=float(x)/halfWidth;
        const float curve=std::pow(std::max(0.f,1-u*u),1.05f);
        const float top=(55-140*openness_)*scale*curve;
        const float bottom=(55+30*openness_)*scale*curve;
        for (int y=int(std::floor(top-line));y<=int(std::ceil(bottom+line));++y) {
            const float dy=y-irisY, dx=x-irisX;
            const float distance=std::hypot(dx,dy);
            const bool outline=std::abs(y-top)<line || std::abs(y-bottom)<line*.75f;
            const bool aperture=y>top+line && y<bottom-line*.75f;
            const bool iris=aperture && distance<irisRadius && distance>irisRadius-line;
            const bool pupil=aperture && distance<irisRadius*.46f;
            // Clear the whole eye silhouette, then render its outlines and iris.
            canvas.pixel(cx+x,cy+y,(outline || iris || pupil) ? ink : 0);
        }
    }
    if (connected_) {
        // Five-by-seven bitmap glyphs for the literal label "$kal".
        constexpr std::array<std::array<unsigned,7>,4> glyphs{{
            {{4,15,20,14,5,30,4}}, {{16,16,18,20,24,20,18}},
            {{0,0,14,1,15,17,15}}, {{12,4,4,4,4,4,14}}
        }};
        constexpr unsigned scale = 3;
        const unsigned left = (canvas.width() - 23*scale) / 2;
        for (unsigned g=0; g<4; ++g)
            for (unsigned y=0; y<7; ++y)
                for (unsigned x=0; x<5; ++x)
                    if (glyphs[g][y] & (1u << (4-x)))
                        canvas.fillRectangle({left+(g*6+x)*scale,
                            canvas.height()-40+y*scale, scale, scale}, ink);
    }
}
