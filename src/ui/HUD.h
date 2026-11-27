#pragma once
namespace lf {
class Game; class Renderer; class Camera;
class HUD {
public:
    int hovered=-1, pressed=-1;
    void draw(const Game&,const Renderer&,const Camera&,int,int,float);
    int inspectAt(const Game&,int,int,int,int) const;
    int actionAt(const Game&,int,int,int,int) const;
    bool actionCenter(const Game&,int,int,int,int&,int&) const;
    bool menuItemCenter(const Game&,int,int,int,int&,int&) const;
    int menuItemAt(const Game&,int,int,int,int) const;
};
}
