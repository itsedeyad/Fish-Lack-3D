#pragma once
#include <string>
namespace lf {
class Game;
class Renderer;
class Camera;
class HUD;
struct InputState;
struct InputSources;
namespace diagnostics {
struct Context {
    Game*& game;Renderer* renderer;Camera& camera;HUD& hud;InputState& input;InputSources& sources;
    int width,height;bool hasProgress;
    void (*mouse)(int,int,int,int);
    void (*keyboard)(unsigned char,int,int);
    void (*keyboardUp)(unsigned char,int,int);
    void (*updateMouseKeys)(float);
    void (*updateCameraInput)(float);
    void (*clearGameplayInput)();
};
bool saveScreenshotBMP(const std::string& path,int w,int h);
int checkInputCallbacks(Context& context);
int benchmarkRender(Context& context);
}
}
