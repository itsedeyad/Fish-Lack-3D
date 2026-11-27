#include "core/ViewMath.h"
#include "core/Canvas.h"
#include "render/Renderer.h"
#include "render/GLHeaders.h"
#include "render/Camera.h"
#include "render/Primitives.h"
#include "render/Textures.h"
#include "core/World.h"
#include "game/Game.h"
#include <cmath>
#include <initializer_list>

namespace lf {

void Renderer::init() {
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);
    glEnable(GL_NORMALIZE);          // re-normalise normals after glScale
    glEnable(GL_MULTISAMPLE);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
    glHint(GL_FOG_HINT, GL_NICEST);

    // Materials: glColor drives ambient + diffuse (GL_COLOR_MATERIAL).
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);    // thin fins, leaves, lily pads
    glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE); // better specular highlights
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_LIGHT1);

    tex::init();
    sky_.init();
    rebuildWorld();
    fish_.init();
}

void Renderer::rebuildWorld() {
    terrain_.init();pier_.init();water_.init();loadedMap_=int(world::activeMap());++rebuilds_;
}

void Renderer::setupLights(const Atmosphere& atm, bool topDown) {
    // Global ambient light: the sky colour lighting everything a little.
    GLfloat amb[4] = {atm.ambient.r, atm.ambient.g, atm.ambient.b, 1.0f};
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, amb);

    // LIGHT0: the sun, a directional light (w = 0). Its position is given in
    // world space because the view matrix is already loaded.
    float s = atm.sunStrength * (topDown ? 1.05f : 1.0f);
    GLfloat sunPos[4] = {atm.sunDir.x, std::fmax(atm.sunDir.y, 0.08f), atm.sunDir.z, 0.0f};
    GLfloat sunDif[4] = {atm.sun.r * s, atm.sun.g * s, atm.sun.b * s, 1.0f};
    GLfloat sunSpec[4] = {atm.sun.r, atm.sun.g, atm.sun.b, 1.0f};
    GLfloat zero[4] = {0, 0, 0, 1};
    glLightfv(GL_LIGHT0, GL_POSITION, sunPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, sunDif);
    glLightfv(GL_LIGHT0, GL_SPECULAR, sunSpec);
    glLightfv(GL_LIGHT0, GL_AMBIENT, zero);

    // LIGHT1: soft bluish fill light from the opposite side of the sky.
    GLfloat fillPos[4] = {-atm.sunDir.x, 0.8f, -atm.sunDir.z, 0.0f};
    GLfloat fillDif[4] = {atm.zenith.r * 0.35f, atm.zenith.g * 0.35f, atm.zenith.b * 0.40f, 1.0f};
    glLightfv(GL_LIGHT1, GL_POSITION, fillPos);
    glLightfv(GL_LIGHT1, GL_DIFFUSE, fillDif);
    glLightfv(GL_LIGHT1, GL_SPECULAR, zero);
    glLightfv(GL_LIGHT1, GL_AMBIENT, zero);

    // LIGHT2: the pier lantern, a positional light with attenuation that
    // fades in as the sun sets.
    if (atm.lampGlow > 0.01f) {
        Vec3 lp = Pier::lanternLightPos();
        GLfloat pos[4] = {lp.x, lp.y, lp.z, 1.0f};
        float g = atm.lampGlow * 1.6f;
        GLfloat dif[4] = {1.0f * g, 0.72f * g, 0.38f * g, 1.0f};
        glLightfv(GL_LIGHT2, GL_POSITION, pos);
        glLightfv(GL_LIGHT2, GL_DIFFUSE, dif);
        glLightfv(GL_LIGHT2, GL_SPECULAR, dif);
        glLightfv(GL_LIGHT2, GL_AMBIENT, zero);
        glLightf(GL_LIGHT2, GL_CONSTANT_ATTENUATION, 1.0f);
        glLightf(GL_LIGHT2, GL_LINEAR_ATTENUATION, 0.25f);
        glLightf(GL_LIGHT2, GL_QUADRATIC_ATTENUATION, 0.12f);
        glEnable(GL_LIGHT2);
    } else {
        glDisable(GL_LIGHT2);
    }
}

void Renderer::drawBucketCatch(const Game& game, float time) const {
    // Caught fish poke out of the bucket on the pier, nose up.
    int shown = 0;
    for (int type = FISH_TYPE_COUNT - 1; type >= 0; --type)
        for (int k = 0; k < game.ledger.keptCount(type) && shown < 6; ++k, ++shown) {
            float a = shown * 2.1f;
            glPushMatrix();
            glTranslatef(world::PIER_X+1.25f + std::cos(a) * 0.08f, world::PIER_DECK_Y + 0.33f, world::PIER_Z_START+2.2f + std::sin(a) * 0.08f);
            glRotatef(rad2deg(a), 0, 1, 0);
            glRotatef(-75.0f + 6.0f * std::sin(time * 3 + shown), 1, 0, 0);
            fish_.drawModel(type, 0.42f, 0.25f * std::sin(time * 6.0f + shown), time * 4.0f);
            glPopMatrix();
        }
}

// Small living details: a flock of birds gliding over the lake and fireflies
// blinking along the shore at dusk (simple lines / points, additive glow).
static void drawWildlife(float time, float progress) {
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    for (int i = 0; i < 7; ++i) {
        float a = time * 0.06f + i * 0.35f;                 // flock circles the lake
        Vec3 c(std::cos(a) * 38.0f + std::sin(i * 2.1f) * 4.0f, 34.0f + 3.0f * std::sin(i * 1.7f + time * 0.3f),
               -8.0f + std::sin(a) * 26.0f + std::cos(i * 1.3f) * 4.0f);
        Vec3 dir(-std::sin(a), 0, std::cos(a)), side(dir.z, 0, -dir.x);
        float flap = 0.45f * std::sin(time * 7.0f + i * 1.9f);
        Vec3 l = c + side * 0.9f - dir * 0.35f + Vec3(0, flap, 0), r = c - side * 0.9f - dir * 0.35f + Vec3(0, flap, 0);
        glColor4f(0.12f, 0.12f, 0.15f, 0.85f);
        glVertex3f(l.x, l.y, l.z); glVertex3f(c.x, c.y, c.z);
        glVertex3f(c.x, c.y, c.z); glVertex3f(r.x, r.y, r.z);
    }
    glEnd();
    float dusk = smoothstep(0.70f, 0.95f, progress);
    if (dusk > 0.0f) {
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        glDepthMask(GL_FALSE);
        glEnable(GL_POINT_SMOOTH);
        glPointSize(5.0f);
        glBegin(GL_POINTS);
        for (int i = 0; i < 45; ++i) {
            float ang = hash2i(i, 3) * TWO_PI, rad = 7.0f + hash2i(i, 5) * 22.0f;
            Vec3 p(std::cos(ang) * rad + std::sin(time * 0.4f + i) * 0.8f, 0.6f + hash2i(i, 7) * 2.0f + 0.3f * std::sin(time * 0.9f + i),
                   34.0f + std::sin(ang) * rad * 0.5f + std::cos(time * 0.3f + i * 2) * 0.8f);
            float blink = std::fmax(0.0f, std::sin(time * (1.5f + hash2i(i, 9)) + i * 1.3f));
            glColor4f(0.85f, 1.0f, 0.35f, dusk * blink);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
    }
    glPopAttrib();
}

void Renderer::render(const Game& game, const Camera& cam, int w, int h) {
    if(loadedMap_!=int(world::activeMap()))rebuildWorld();
    float time = game.worldTime;
    atm_ = Atmosphere::at(game.dayProgress());
    bool topDown = cam.isOrtho();
    if (game.state == GameState::MainMenu)
        atm_.ambient = lerpc(atm_.ambient, Color(0.68f, 0.57f, 0.48f), 0.45f);

    glViewport(0, 0, w, h);
    glClearColor(atm_.horizon.r, atm_.horizon.g, atm_.horizon.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Fit the 3D view to the very same 16:9 canvas as the HUD and hit tests.
    Canvas canvas(w,h);int viewW=int(Canvas::width*canvas.scale),viewH=int(Canvas::height*canvas.scale);
    glViewport((w-viewW)/2,(h-viewH)/2,viewW,viewH);
    cam.apply(viewW, viewH);
    glGetDoublev(GL_MODELVIEW_MATRIX, mv_);
    glGetDoublev(GL_PROJECTION_MATRIX, proj_);
    glGetIntegerv(GL_VIEWPORT, vp_);

    setupLights(atm_, topDown);

    if (!topDown) sky_.draw(cam.eye(), atm_, time);

    // Distance fog blends far geometry into the horizon colour (aerial
    // perspective). Disabled for the orthographic map view.
    if (!topDown) {
        GLfloat fogCol[4] = {atm_.horizon.r, atm_.horizon.g, atm_.horizon.b, 1.0f};
        glEnable(GL_FOG);
        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogfv(GL_FOG_COLOR, fogCol);
        glFogf(GL_FOG_START, 70.0f);
        glFogf(GL_FOG_END, 420.0f);
    } else {
        glDisable(GL_FOG);
    }

    glEnable(GL_LIGHTING);
    terrain_.drawGround();
    terrain_.drawShadows();
    terrain_.drawScenery(time,atm_.lampGlow);
    pier_.draw(game, atm_, time);
    pier_.drawShadows();
    drawBucketCatch(game, time);

    fish_.drawShadows(game.school);
    fish_.drawSchool(game.school, time);

    // The menu fish stays over water relative to the current dock. The old
    // fixed Forest position put an oversized fish through the Marsh angler.
    float menuLeap=std::fmod(time,8.f);
    if (game.state == GameState::MainMenu && menuLeap<1.4f) {
        glPushMatrix();
        float leap = .1f+.8f*std::sin(menuLeap/1.4f*PI);
        glTranslatef(world::PIER_X+6.f,leap,world::PIER_Z_START-8.f);
        glRotatef(-35, 0, 1, 0);
        glRotatef(-48 + 8 * std::sin(time), 1, 0, 0);
        fish_.drawModel(FISH_TROUT, 1.2f, 0.18f * std::sin(time * 3), time * 3);
        glPopMatrix();
    }
    if (cam.mode() != CameraMode::FirstPerson || game.state == GameState::MainMenu) rod_.drawAngler(game);
    else rod_.drawArms(game);
    rod_.drawRod(game);
    rod_.drawBobber(game, time);
    rod_.drawLine(game);

    water_.drawPlants(time);
    water_.draw(game.fx, atm_, cam.eye(), time, topDown);
    glEnable(GL_LIGHTING);
    water_.drawLilyPads(time);
    water_.drawRipples(game.fx, time);
    water_.drawParticles(game.fx);
    rod_.drawAimHelpers(game, time);
    if (!topDown) drawWildlife(time, atm_.progress);

    glDisable(GL_FOG);
}

void Renderer::screenDirection(const Vec3& p,float& dx,float& dy) const {
    double world[]={p.x,p.y,p.z,1},camera[4]={};
    for(int r=0;r<4;++r)for(int c=0;c<4;++c)camera[r]+=mv_[c*4+r]*world[c];
    dx=float(camera[0]*proj_[0]);dy=float(-camera[1]*proj_[5]);
    if(camera[2]>=0){dx=dx>=0?1.f:-1.f;dy=0;}
    if(std::fabs(dx)+std::fabs(dy)<.001f)dy=1;
}

bool Renderer::project(const Vec3& p, float& sx, float& sy) const {
    return projectPoint(p, mv_, proj_, vp_, sx, sy);
}

} // namespace lf
