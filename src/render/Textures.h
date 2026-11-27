// =============================================================================
//  Textures.h - procedural textures generated pixel by pixel at start-up.
//  No image files are loaded: wood grain, water, ground detail and bark are
//  computed from tileable value noise and uploaded with glTexImage2D and 2x2 averaged mipmaps.
// =============================================================================
#pragma once

namespace tex {

enum Id { WOOD = 0, WATER, GROUND, BARK, COUNT };

void init();           // generate and upload all textures (needs a GL context)
void bind(Id id);      // enable texturing and bind the texture
void unbind();         // disable texturing

} // namespace tex
