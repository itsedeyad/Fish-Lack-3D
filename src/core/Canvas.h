#pragma once
#include <algorithm>

namespace lf {
struct UiRect {
    float x,y,w,h;
    bool contains(float px,float py) const {
        return px>=x && py>=y && px<x+w && py<y+h;
    }
};

struct Canvas {
    static constexpr float width=1600,height=900;
    float scale,ox,oy;
    Canvas(int w,int h):scale(std::min(w/width,h/height)),
        ox((w-width*scale)/2),oy((h-height*scale)/2) {}
    void point(float x,float y,float& px,float& py) const {
        px=(x-ox)/scale;py=(y-oy)/scale;
    }
    bool contains(float x,float y) const {
        float px,py;point(x,y,px,py);
        return px>=0 && py>=0 && px<width && py<height;
    }
};
inline constexpr UiRect soundSlider{1030,447,124,4};
} // namespace lf
