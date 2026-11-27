#include "render/RodRenderer.h"
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include "core/World.h"
#include "game/Game.h"
#include "core/Settings.h"
#include <cmath>
#include <initializer_list>

namespace lf {

static void mult(const Mat4& m) { glMultMatrixf(m.m); }

static Mat4 anglerMatrix(const FishingSystem& f) {
    return Mat4::translate(world::anglerPosition()) * Mat4::rotateY(f.anglerYaw());
}

/// Crank knob position of the reel (world space) - the left hand holds it.
static Vec3 crankKnob(const FishingSystem& f) {
    float a = f.reelAngle();
    return f.rodBaseMatrix().transformPoint(Vec3(0.075f, -0.075f + std::sin(a) * 0.05f, 0.33f + std::cos(a) * 0.05f));
}

// ------------------------------------------------------------------ angler --
void RodRenderer::drawAngler(const Game& g) const {
    const FishingSystem& f = g.fishing;
    Mat4 M = anglerMatrix(f);
    const Color skin(0.93f, 0.74f, 0.58f), jeans(0.12f, 0.24f, 0.36f), vest(0.87f, 0.83f, 0.65f),
        shirt(0.97f, 0.94f, 0.83f), boots(0.62f, 0.44f, 0.27f), hat(0.16f, 0.31f, 0.40f);

    prim::specular(0.08f, 10.0f);
    glPushMatrix();
    mult(M);
    // legs and boots
    for (float s : {-1.0f, 1.0f}) {
        prim::color(boots);
        prim::boxFromTo(Vec3(s * 0.12f - 0.07f, 0.0f, -0.10f), Vec3(s * 0.12f + 0.07f, 0.13f, 0.18f));
        prim::color(jeans);
        prim::cylinderBetween(Vec3(s * 0.11f, 0.12f, 0.0f), Vec3(s * 0.11f, 0.92f, 0.0f), 0.068f, 0.08f, 4);
    }
    // hips, torso (shirt + fishing vest), neck, head, hat
    prim::color(jeans);
    prim::boxFromTo(Vec3(-0.19f, 0.84f, -0.11f), Vec3(0.19f, 1.00f, 0.11f));
    prim::color(shirt);
    prim::boxFromTo(Vec3(-0.21f, 0.98f, -0.12f), Vec3(0.21f, 1.48f, 0.12f));
    prim::color(vest);
    prim::boxFromTo(Vec3(-0.225f, 1.00f, -0.135f), Vec3(0.225f, 1.40f, 0.135f));
    prim::color(Color(0.70f, 0.65f, 0.45f)); // vest pockets
    prim::boxFromTo(Vec3(-0.17f, 1.08f, 0.13f), Vec3(-0.05f, 1.20f, 0.15f));
    prim::boxFromTo(Vec3(0.05f, 1.08f, 0.13f), Vec3(0.17f, 1.20f, 0.15f));
    prim::color(skin);
    prim::cylinderBetween(Vec3(0, 1.46f, 0), Vec3(0, 1.56f, 0), 0.05f, 0.05f, 8);
    glPushMatrix();
    glTranslatef(0, 1.66f, 0.01f);
    glScalef(0.95f, 1.08f, 1.0f);
    prim::box(.22f,.23f,.22f);
    glPopMatrix();
    // Face, hair below the cap and vest stitching remain simple cuboids.
    prim::color(Color(.22f,.12f,.07f));
    prim::boxFromTo(Vec3(-.11f,1.69f,-.108f),Vec3(.11f,1.755f,.08f));
    for(float side : {-1.f,1.f}) {
        prim::color(skin);prim::boxFromTo(Vec3(side*.116f-.025f,1.61f,-.02f),Vec3(side*.116f+.025f,1.68f,.04f));
        prim::color(Color(.99f,.98f,.91f));prim::boxFromTo(Vec3(side*.047f-.022f,1.65f,.119f),Vec3(side*.047f+.022f,1.68f,.128f));
        prim::color(Color(.08f,.12f,.10f));prim::boxFromTo(Vec3(side*.047f-.009f,1.651f,.129f),Vec3(side*.047f+.009f,1.675f,.134f));
        prim::color(Color(.25f,.13f,.07f));prim::boxFromTo(Vec3(side*.047f-.025f,1.69f,.12f),Vec3(side*.047f+.025f,1.704f,.13f));
        prim::color(Color(.35f,.37f,.22f));prim::boxFromTo(Vec3(side*.11f-.06f,1.19f,.151f),Vec3(side*.11f+.06f,1.208f,.16f));
        prim::color(Color(.19f,.16f,.12f));prim::boxFromTo(Vec3(side*.12f-.073f,.015f,-.105f),Vec3(side*.12f+.073f,.045f,.185f));
    }
    prim::color(skin);prim::boxFromTo(Vec3(-.025f,1.613f,.122f),Vec3(.025f,1.659f,.16f));
    prim::color(Color(.55f,.29f,.22f));prim::boxFromTo(Vec3(-.033f,1.598f,.122f),Vec3(.033f,1.609f,.128f));
    prim::color(hat);
    glPushMatrix();
    glTranslatef(0, 1.73f, 0);
    glRotatef(-6, 1, 0, 0);
    prim::cylinder(0.21f, 0.21f, 0.015f, 18, true);   // brim
    prim::cylinder(0.125f, 0.105f, 0.11f, 16, true);  // crown
    prim::color(Color(0.35f, 0.25f, 0.15f));
    glTranslatef(0, 0.015f, 0);
    prim::cylinder(0.127f, 0.124f, 0.03f, 16, false); // band
    glPopMatrix();
    glPopMatrix();

    drawArms(g);

    // soft shadow on the deck
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    Vec3 a = world::anglerPosition();
    prim::blobShadow(a.x + 0.15f, a.y + 0.006f, a.z + 0.12f, 0.42f, 0.38f, 0.45f);
    glPopAttrib();
}

void RodRenderer::drawArms(const Game& g) const {
    const auto& f=g.fishing;
    Mat4 M=anglerMatrix(f);
    const Color shirt(.97f,.94f,.83f),skin(.93f,.74f,.58f);
    // arms: from the shoulders to the hands, which follow the rod
    Vec3 shoulderR = M.transformPoint(Vec3(-0.25f, 1.40f+.006f*std::sin(g.worldTime*1.8f), 0.02f));
    Vec3 shoulderL = M.transformPoint(Vec3(0.25f, 1.40f+.006f*std::sin(g.worldTime*1.8f), 0.02f));
    Vec3 handR = f.handPosition();
    Vec3 handL = crankKnob(f);
    Vec3 sideR = M.transformDir(Vec3(-1, 0, 0)), sideL = M.transformDir(Vec3(1, 0, 0));
    for (int k = 0; k < 2; ++k) {
        Vec3 sh = k ? shoulderL : shoulderR, hd = k ? handL : handR, side = k ? sideL : sideR;
        Vec3 elbow = (sh + hd) * 0.5f + Vec3(0, -0.14f-(g.activeState()==GameState::Reeling?.06f:0.f), 0) + side * 0.10f;
        prim::color(shirt);
        prim::cylinderBetween(sh, elbow, 0.062f, 0.055f, 8);
        prim::color(skin);
        prim::cylinderBetween(elbow, hd, 0.048f, 0.04f, 8);
        glPushMatrix();
        glTranslatef(hd.x, hd.y, hd.z);
        prim::sphere(0.05f, 8, 6);
        glPopMatrix();
    }

    // Four short fingers curl around the cork in the same grip frame.
    glPushMatrix(); mult(f.rodBaseMatrix());
    prim::color(skin);
    for(int i=0;i<4;++i)prim::boxFromTo(Vec3(-.034f,-.033f,.14f+i*.022f),Vec3(.034f,.006f,.155f+i*.022f));
    glPopMatrix();
}

// --------------------------------------------------------------------- rod --
void RodRenderer::drawRod(const Game& g) const {
    const FishingSystem& f = g.fishing;
    // Handle, reel seat and reel live in the rod-base coordinate frame.
    glPushMatrix();
    mult(f.rodBaseMatrix());
    prim::specular(0.15f, 15.0f);
    prim::color(Color(0.12f, 0.12f, 0.12f));
    prim::sphere(0.027f, 10, 8);                                       // butt cap
    prim::color(Color(0.80f, 0.64f, 0.44f));
    prim::cylinderZ(0.024f, 0.020f, FishingSystem::ROD_HANDLE_LEN, 12); // cork grip
    prim::specular(0.8f, 70.0f);
    glPushMatrix();
    glTranslatef(0, 0, 0.27f);
    prim::color(Color(0.18f, 0.18f, 0.20f));
    prim::cylinderZ(0.027f, 0.027f, 0.12f, 12);                         // reel seat
    glPopMatrix();
    // reel: foot, body (spool housing), spool and rotating crank
    glPushMatrix();
    glTranslatef(0, -0.035f, 0.33f);
    prim::color(Color(0.25f, 0.25f, 0.28f));
    prim::box(0.014f, 0.04f, 0.06f);
    glTranslatef(0, -0.04f, 0);
    glPushMatrix();
    glRotatef(90, 0, 0, 1);         // spool axis along X
    glTranslatef(0, -0.035f, 0);
    prim::color(g.session.reel==3?Color(.66f,.48f,.85f):g.session.reel==2?Color(.92f,.64f,.19f):g.session.reel==1?Color(.43f,.62f,.69f):Color(.50f,.47f,.39f));
    prim::cylinder(0.045f, 0.045f, 0.07f, 18, true);
    prim::color(Color(0.92f, 0.92f, 0.88f));   // line wound on the spool
    glTranslatef(0, 0.07f, 0);
    prim::cylinder(0.033f, 0.033f, 0.012f, 16, true);
    glPopMatrix();
    // crank: rotates around the spool axis by the reel angle
    glTranslatef(0.04f, 0, 0);
    glRotatef(rad2deg(f.reelAngle()), 1, 0, 0);
    prim::color(Color(0.20f, 0.20f, 0.22f));
    glPushMatrix();
    glTranslatef(0.01f, 0, 0.025f);
    prim::box(0.012f, 0.012f, 0.06f);
    glPopMatrix();
    glTranslatef(0.035f, 0, 0.05f);
    prim::color(Color(0.08f, 0.08f, 0.08f));
    glPushMatrix();
    glRotatef(90, 0, 0, 1);
    prim::cylinder(0.012f, 0.012f, 0.04f, 10, true);  // knob
    glPopMatrix();
    glPopMatrix();
    glPopMatrix();

    // Blank: each segment drawn in its own (bent) frame - hierarchical chain.
    prim::specular(0.9f, 80.0f);
    for (int i = 0; i < FishingSystem::ROD_SEGMENTS; ++i) {
        float r0 = 0.035f - i * 0.006f, r1 = r0 - 0.005f;
        glPushMatrix();
        mult(f.rodSegmentMatrix(i));
        prim::color(g.session.rod==0?Color(.55f,.29f,.10f):g.session.rod==1?Color(.20f,.55f,.43f):g.session.rod==2?Color(.30f,.41f,.51f):Color(.90f,.66f,.16f));
        prim::cylinderZ(r0, std::fmax(r1, 0.003f), FishingSystem::ROD_SEG_LEN, 4, true);
        // decorative wrap and line guide at the end of the segment
        glTranslatef(0, 0, FishingSystem::ROD_SEG_LEN - 0.03f);
        prim::color(Color(0.75f, 0.70f, 0.45f));
        prim::cylinderZ(r1 + 0.002f, r1 + 0.002f, 0.02f, 8, false);
        prim::color(Color(0.80f, 0.80f, 0.82f));
        float gr = 0.008f + 0.004f * (3 - i);
        prim::cylinderBetween(Vec3(0, -r1, 0.01f), Vec3(0, -r1 - gr, 0.01f), 0.002f, 0.002f, 4);
        glTranslatef(0, -r1 - gr * 1.6f, 0.01f);
        prim::torus(gr * 0.75f, gr, 6, 12);
        glPopMatrix();
    }
    prim::specular(0.1f, 10.0f);
}

// -------------------------------------------------------------------- line --
void RodRenderer::drawLine(const Game& g) const {
    const FishingSystem& f = g.fishing;
    Vec3 tip = f.rodTip();
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_CURRENT_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glLineWidth(1.6f);
    if(g.session.line)glColor4f(.77f,1.f,.80f,.9f);
    else glColor4f(.95f,.95f,.90f,.9f);

    // Line running from the reel spool through each guide to the tip.
    glBegin(GL_LINE_STRIP);
    Vec3 spool = f.rodBaseMatrix().transformPoint(Vec3(0, -0.075f, 0.33f));
    glVertex3f(spool.x, spool.y, spool.z);
    for (int i = 0; i < FishingSystem::ROD_SEGMENTS; ++i) {
        float r1 = 0.016f - i * 0.003f - 0.003f;
        float gr = 0.008f + 0.004f * (3 - i);
        Vec3 p = f.rodSegmentMatrix(i).transformPoint(Vec3(0, -r1 - gr * 1.6f, FishingSystem::ROD_SEG_LEN - 0.02f));
        glVertex3f(p.x, p.y, p.z);
    }
    glVertex3f(tip.x, tip.y, tip.z);
    glEnd();

    if (f.lineVisible()) {
        // Quadratic Bezier from tip to bobber; the control point sags with slack.
        Vec3 end = f.lineEnd();
        float len = (end - tip).length();
        Vec3 mid = (tip + end) * 0.5f - Vec3(0, f.lineSlack() * len * 0.18f + 0.01f, 0);
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 32; ++i) {
            float t = i / 32.0f;
            Vec3 p = tip * ((1 - t) * (1 - t)) + mid * (2 * t * (1 - t)) + end * (t * t);
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
        // Short leader from the bobber down to the hooked fish's mouth.
        const Fish* fish = f.engagedFishPtr();
        if (fish && (fish->mode == FishMode::Hooked || fish->mode == FishMode::Landed || fish->mode == FishMode::Bite)) {
            float len2 = fishSpec(fish->type).length * fish->sizeScale * FISH_VISUAL_SCALE * 0.5f;
            Vec3 fwd(std::sin(fish->yaw) * std::cos(fish->pitch), -std::sin(fish->pitch), std::cos(fish->yaw) * std::cos(fish->pitch));
            Vec3 mouth = fish->pos + fwd * len2;
            glBegin(GL_LINES);
            glVertex3f(end.x, end.y, end.z);
            glVertex3f(mouth.x, mouth.y, mouth.z);
            glEnd();
        }
    } else {
        // Snapped line: a short curly piece dangling from the tip.
        glBegin(GL_LINE_STRIP);
        for (int i = 0; i <= 12; ++i) {
            float t = i / 12.0f;
            glVertex3f(tip.x + std::sin(t * 9) * 0.05f, tip.y - t * 0.6f, tip.z + std::cos(t * 9) * 0.05f);
        }
        glEnd();
    }
    glPopAttrib();
}

// ------------------------------------------------------------------ bobber --
void RodRenderer::drawBobber(const Game& g, float time) const {
    const FishingSystem& f = g.fishing;
    if (!f.bobberVisible()) return;
    Vec3 p = f.bobberPos();
    Vec3 up = f.bobberTilt();
    glPushMatrix();
    glTranslatef(p.x, p.y, p.z);
    // Orient the bobber's axis (+Y) along the tilt vector.
    Vec3 axis(up.z, 0, -up.x);
    float al = axis.length();
    if (al > 1e-4f) glRotatef(rad2deg(std::acos(clampf(up.y, -1, 1))), axis.x / al, 0, axis.z / al);
    float scale=f.bobberInWater()?1.8f:.85f;
    glScalef(scale,scale*(g.session.bobber?1.3f:1.f),scale); // stylised: larger than life so it reads at 30 m
    prim::specular(0.9f, 90.0f);
    prim::color(g.session.bobber?Color(.96f,.64f,.12f):Color(.92f,.10f,.08f));
    prim::hemisphere(0.09f, true, 16, 6);
    prim::color(Color(0.96f, 0.96f, 0.93f));
    prim::hemisphere(0.09f, false, 16, 6);
    prim::color(Color(0.10f, 0.10f, 0.10f));
    glPushMatrix(); glRotatef(90, 1, 0, 0); prim::torus(0.086f, 0.096f, 6, 20); glPopMatrix();
    prim::color(Color(0.96f, 0.96f, 0.92f));
    glPushMatrix(); glTranslatef(0, 0.08f, 0); prim::cylinder(0.012f, 0.009f, 0.22f, 8, true); glPopMatrix();
    // bright antenna tip (slightly emissive so it is visible at sunset)
    GLfloat em[4] = {g.session.bobber==2?.9f:.5f, g.session.bobber==2?.8f:.22f, g.session.bobber==2?.3f:0.f, 1.0f}, none[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, em);
    prim::color(Color(1.0f, 0.55f, 0.05f));
    glPushMatrix(); glTranslatef(0, 0.31f, 0); prim::sphere(0.024f, 10, 8); glPopMatrix();
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
    prim::color(Color(0.25f, 0.25f, 0.25f));
    glPushMatrix(); glTranslatef(0, -0.2f, 0); prim::cylinder(0.006f, 0.008f, 0.12f, 6, false); glPopMatrix();
    // Small silhouette of the bait used by the current cast.
    glPushMatrix();glTranslatef(0,-.34f,0);
    prim::color(g.session.bait==2?Color(.90f,.71f,.40f):g.session.bait==3?Color(.48f,.70f,.75f):Color(.79f,.32f,.23f));
    if(g.session.bait==0){for(int i=0;i<3;++i){glTranslatef(.012f*std::sin(i+time),-.025f,0);prim::sphere(.018f,6,4);}}
    else if(g.session.bait==1){prim::box(.035f,.05f,.025f);prim::color(Color(.84f,.88f,.68f));prim::box(.09f,.008f,.045f);}
    else if(g.session.bait==2)prim::sphere(.035f,6,4);
    else {prim::box(.025f,.035f,.11f);glTranslatef(0,0,-.06f);prim::box(.05f,.04f,.018f);}
    glPopMatrix();
    glPopMatrix();
    (void)time;
    prim::specular(0.1f, 10.0f);
}

// ------------------------------------------------------------ aim helpers --
void RodRenderer::drawAimHelpers(const Game& g, float time) {
    if(g.uiBlocked()) return; // no targeting helpers behind modal windows
    const FishingSystem& f = g.fishing;
    GameState st = g.activeState();

    // Record the bobber's flight path for a fading trail.
    if (st == GameState::Casting && f.castReleased()) {
        if (trail_.empty() || (trail_.back() - f.bobberPos()).length() > 0.25f) {
            if(trail_.size()>=settings::castTrailLimit) trail_.erase(trail_.begin());
            trail_.push_back(f.bobberPos());
        }
    } else if (st != GameState::Waiting) {
        trail_.clear();
    }

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_POINT_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POINT_SMOOTH);

    if (st == GameState::Aiming || st == GameState::Charging) {
        // Before charge, preview the shortest cast; during charge use actual power.
        float pw = st == GameState::Charging ? -1.0f : settings::aimPreviewPower;
        Vec3 t = f.predictedLanding(pw);
        float pulse = 0.5f + 0.5f * std::sin(time * 5.0f);
        float rad = settings::aimRingRadius + (st == GameState::Charging ? 0.0f : 0.15f * pulse);
        // dashed rotating target ring on the water
        glLineWidth(3.0f);
        glBegin(GL_LINES);
        for (int i = 0; i < 24; ++i) {
            if (i % 2) continue;
            for (int k = 0; k < 2; ++k) {
                float a = TWO_PI * (i + k) / 24 + time * 0.8f;
                float x = t.x + std::cos(a) * rad, z = t.z + std::sin(a) * rad;
                glColor4f(1.0f, 0.92f, 0.35f, 0.85f);
                glVertex3f(x, world::waveHeight(x, z, g.worldTime) + 0.05f, z);
            }
        }
        glEnd();
        glPointSize(7.0f);
        glBegin(GL_POINTS);
        glColor4f(1.0f, 0.95f, 0.5f, 0.9f);
        glVertex3f(t.x, world::waveHeight(t.x, t.z, g.worldTime) + 0.06f, t.z);
        glEnd();

        // dotted predicted trajectory
        glPointSize(st == GameState::Charging ? 5.0f : 3.5f);
        glBegin(GL_POINTS);
        for (int i = 1; i < settings::previewArcSamples; ++i) {
            float u = float(i) / settings::previewArcSamples;
            Vec3 p = f.previewArcPoint(u, pw);
            glColor4f(1.0f, 1.0f, 0.85f, (st == GameState::Charging ? 0.85f : 0.45f) * (1.0f - 0.5f * u));
            glVertex3f(p.x, p.y, p.z);
        }
        glEnd();
    }

    if (trail_.size() > 1 && (st == GameState::Casting || st == GameState::Waiting)) {
        glLineWidth(2.5f);
        glBegin(GL_LINE_STRIP);
        float fade = st == GameState::Waiting ? clampf(1.0f - g.stateTimer * 1.5f, 0.0f, 1.0f) : 1.0f;
        for (size_t i = 0; i < trail_.size(); ++i) {
            float a = float(i) / trail_.size();
            glColor4f(1.0f, 1.0f, 1.0f, 0.55f * a * fade);
            glVertex3f(trail_[i].x, trail_[i].y, trail_[i].z);
        }
        glEnd();
        if (fade <= 0) trail_.clear();
    }
    glPopAttrib();
}

} // namespace lf
