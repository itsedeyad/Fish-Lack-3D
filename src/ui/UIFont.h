// Baked alpha-atlas typography drawn with OpenGL; built-in FreeGLUT fallback.
#pragma once
#include <string>

namespace uifont {

enum Size { TINY = 0, SMALL, MED, BIG, TITLE, XL, COUNT };

void  init(const std::string& assetDirectory);                 // needs a current OpenGL context
bool  ready();
void  setCanvasScale(float scale);
float width(int size, const std::string& s);
/// Draw with the left edge at x and the text baseline at y (y grows down).
void  draw(int size, float x, float y, const std::string& s);

} // namespace uifont
