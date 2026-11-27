#include "render/Pier.h"
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include "render/Textures.h"
#include "core/World.h"
#include "game/Game.h"
#include <cmath>
#include <initializer_list>

namespace lf {

using namespace world;

static const float POST_X = PIER_HALF_W + 0.05f;
static const float RAIL_START_Z = 30.0f;

Vec3 Pier::lanternLightPos() { return pierPoint(-1.62f, 2.30f, 27.0f); }

static void woodColor(float r, float g, float b) { glColor3f(r, g, b); }

void Pier::buildPier() {
    const float PIER_X=0,PIER_Z_START=22,PIER_Z_END=22+world::PIER_Z_END-world::PIER_Z_START;
    auto terrainHeight=[](float x,float z){return world::terrainHeight(x+world::PIER_X,z+world::PIER_Z_START-22);};
    Rng rng(1234);
    pierList_ = glGenLists(1);
    glNewList(pierList_, GL_COMPILE);
    glPushMatrix();glTranslatef(world::PIER_X,0,world::PIER_Z_START-22);
    prim::specular(0.08f, 12.0f);
    tex::bind(tex::WOOD);

    // ---- deck planks: individual boards with gaps and colour variation
    const float plank = 0.26f, gap = 0.045f, thick = 0.075f;
    for (float z = PIER_Z_START + plank * 0.5f; z < PIER_Z_END; z += plank + gap) {
        float v = rng.range(0.78f, 1.08f);
        bool weathered = rng.chance(0.12f);
        float r = 0.88f * v, g = 0.65f * v, b = 0.36f * v;
        if (weathered) { r = 0.58f * v; g = 0.52f * v; b = 0.45f * v; } // grey, sun-bleached board
        woodColor(r, g, b);
        float xo = rng.range(-0.06f, 0.06f);
        float tilt = rng.range(-0.012f, 0.012f);
        glPushMatrix();
        glTranslatef(PIER_X + xo, PIER_DECK_Y - thick * 0.5f + tilt, z);
        glRotatef(rng.range(-0.6f, 0.6f), 0, 1, 0);
        prim::box(PIER_HALF_W * 2.0f + rng.range(-0.05f, 0.1f), thick, plank, 0.7f);
        glPopMatrix();
    }
    // End fascia board across the lake-side end.
    woodColor(0.50f, 0.36f, 0.24f);
    prim::boxFromTo(Vec3(-PIER_HALF_W - 0.05f, PIER_DECK_Y - 0.30f, PIER_Z_START - 0.08f),
                    Vec3(PIER_HALF_W + 0.05f, PIER_DECK_Y - 0.02f, PIER_Z_START + 0.02f), 0.7f);

    // ---- stringers (long beams under the planks)
    woodColor(0.45f, 0.32f, 0.22f);
    for (float x : {-1.5f, 0.0f, 1.5f})
        prim::boxFromTo(Vec3(x - 0.08f, PIER_DECK_Y - thick - 0.24f, PIER_Z_START + 0.1f),
                        Vec3(x + 0.08f, PIER_DECK_Y - thick, PIER_Z_END), 0.7f);

    // ---- bearers, support posts, bracing
    for (float z = PIER_Z_START + 1.0f; z <= PIER_Z_END; z += 3.0f) {
        woodColor(0.42f, 0.30f, 0.21f);
        prim::boxFromTo(Vec3(-POST_X - 0.15f, PIER_DECK_Y - 0.50f, z - 0.09f),
                        Vec3(POST_X + 0.15f, PIER_DECK_Y - 0.31f, z + 0.09f), 0.7f);
        for (float side : {-1.0f, 1.0f}) {
            float x = side * POST_X;
            float bed = terrainHeight(x, z) - 0.3f;
            bool rail = z >= RAIL_START_Z;
            float top = rail ? PIER_DECK_Y + 1.05f : PIER_DECK_Y - 0.02f;
            glPushMatrix();
            glTranslatef(x, bed, z);
            if (bed < 0.0f) {
                // Submerged part: darker with an algae tint.
                glColor3f(0.20f, 0.26f, 0.17f);
                prim::cylinder(0.15f, 0.15f, -bed + 0.12f, 10, true);
                glColor3f(0.40f, 0.29f, 0.20f);
                glTranslatef(0, -bed + 0.12f, 0);
                prim::cylinder(0.14f, 0.14f, top - 0.12f, 10, true);
            } else {
                glColor3f(0.40f, 0.29f, 0.20f);
                prim::cylinder(0.14f, 0.14f, top - bed, 10, true);
            }
            glPopMatrix();
            if (rail) { // post cap
                glColor3f(0.36f, 0.26f, 0.18f);
                prim::boxFromTo(Vec3(x - 0.17f, top, z - 0.17f), Vec3(x + 0.17f, top + 0.06f, z + 0.17f), 1.0f);
            }
        }
        // X-bracing between the posts on the water side
        float bedL = terrainHeight(-POST_X, z), bedR = terrainHeight(POST_X, z);
        if (bedL < -0.5f && bedR < -0.5f) {
            glColor3f(0.33f, 0.25f, 0.18f);
            prim::cylinderBetween(Vec3(-POST_X, -0.3f, z), Vec3(POST_X, PIER_DECK_Y - 0.5f, z), 0.06f, 0.06f, 6);
            prim::cylinderBetween(Vec3(POST_X, -0.3f, z), Vec3(-POST_X, PIER_DECK_Y - 0.5f, z), 0.06f, 0.06f, 6);
        }
    }

    // ---- railings (land half of the pier; the fishing end stays open)
    for (float side : {-1.0f, 1.0f}) {
        float x = side * POST_X;
        glColor3f(0.55f, 0.40f, 0.27f);
        prim::boxFromTo(Vec3(x - 0.06f, PIER_DECK_Y + 0.95f, RAIL_START_Z + 1.0f), Vec3(x + 0.06f, PIER_DECK_Y + 1.05f, PIER_Z_END), 0.7f);
        glColor3f(0.50f, 0.36f, 0.25f);
        prim::boxFromTo(Vec3(x - 0.04f, PIER_DECK_Y + 0.50f, RAIL_START_Z + 1.0f), Vec3(x + 0.04f, PIER_DECK_Y + 0.58f, PIER_Z_END), 0.7f);
    }

    // ---- ladder down into the water at the end of the pier
    glColor3f(0.42f, 0.31f, 0.22f);
    for (float dx : {0.0f, 0.5f}) {
        float lx = PIER_HALF_W + 0.12f;
        prim::boxFromTo(Vec3(lx - 0.04f, -1.2f, 24.0f + dx - 0.04f), Vec3(lx + 0.04f, PIER_DECK_Y + 0.55f, 24.0f + dx + 0.04f), 1.0f);
    }
    for (float y = -0.9f; y < PIER_DECK_Y; y += 0.32f)
        prim::boxFromTo(Vec3(PIER_HALF_W + 0.09f, y, 24.0f), Vec3(PIER_HALF_W + 0.15f, y + 0.05f, 24.5f), 1.0f);

    // ---- wooden crate / stool
    for (int s = 0; s < 4; ++s) {
        glColor3f(0.66f - s * 0.02f, 0.50f, 0.33f);
        prim::boxFromTo(Vec3(-1.55f, PIER_DECK_Y + 0.02f + s * 0.12f, 25.7f), Vec3(-1.05f, PIER_DECK_Y + 0.12f + s * 0.12f, 26.2f), 1.2f);
    }
    glColor3f(0.72f, 0.55f, 0.36f);
    prim::boxFromTo(Vec3(-1.58f, PIER_DECK_Y + 0.50f, 25.67f), Vec3(-1.02f, PIER_DECK_Y + 0.55f, 26.23f), 1.2f);

    // ---- bench near the land end
    glColor3f(0.60f, 0.45f, 0.30f);
    prim::boxFromTo(Vec3(-1.75f, PIER_DECK_Y + 0.45f, 44.0f), Vec3(-1.25f, PIER_DECK_Y + 0.52f, 46.5f), 0.8f);
    for (float bz : {44.2f, 46.3f}) prim::boxFromTo(Vec3(-1.70f, PIER_DECK_Y, bz - 0.05f), Vec3(-1.30f, PIER_DECK_Y + 0.45f, bz + 0.05f), 1.0f);

    tex::unbind();

    // ---- tackle box (painted metal)
    prim::specular(0.5f, 40.0f);
    glColor3f(0.16f, 0.45f, 0.32f);
    prim::boxFromTo(Vec3(-1.45f, PIER_DECK_Y, 24.40f), Vec3(-0.95f, PIER_DECK_Y + 0.22f, 24.72f), 1.0f);
    glColor3f(0.20f, 0.55f, 0.38f);
    prim::boxFromTo(Vec3(-1.47f, PIER_DECK_Y + 0.22f, 24.38f), Vec3(-0.93f, PIER_DECK_Y + 0.27f, 24.74f), 1.0f);
    glColor3f(0.75f, 0.75f, 0.72f);
    prim::cylinderBetween(Vec3(-1.30f, PIER_DECK_Y + 0.27f, 24.56f), Vec3(-1.30f, PIER_DECK_Y + 0.35f, 24.56f), 0.015f, 0.015f, 5);
    prim::cylinderBetween(Vec3(-1.10f, PIER_DECK_Y + 0.27f, 24.56f), Vec3(-1.10f, PIER_DECK_Y + 0.35f, 24.56f), 0.015f, 0.015f, 5);
    prim::cylinderBetween(Vec3(-1.31f, PIER_DECK_Y + 0.35f, 24.56f), Vec3(-1.09f, PIER_DECK_Y + 0.35f, 24.56f), 0.018f, 0.018f, 5);
    // a couple of lures on top
    glColor3f(0.95f, 0.30f, 0.15f);
    glPushMatrix(); glTranslatef(-1.25f, PIER_DECK_Y + 0.29f, 24.47f); glScalef(1, 0.5f, 2); prim::sphere(0.025f, 6, 4); glPopMatrix();

    // ---- bucket (galvanised)
    glColor3f(0.62f, 0.64f, 0.66f);
    glPushMatrix();
    glTranslatef(1.25f, PIER_DECK_Y, 24.2f);
    prim::cylinder(0.17f, 0.21f, 0.38f, 16, false);
    glColor3f(0.50f, 0.52f, 0.54f);
    prim::disc(0.17f, 16);
    glTranslatef(0, 0.38f, 0);
    glPushMatrix(); glRotatef(90, 1, 0, 0); prim::torus(0.20f, 0.23f, 6, 20); glPopMatrix();
    glColor3f(0.10f, 0.25f, 0.35f);
    glTranslatef(0, -0.08f, 0);
    prim::disc(0.20f, 16); // water inside the bucket
    glPopMatrix();
    glColor3f(0.55f, 0.55f, 0.55f);
    for (int i = 0; i < 8; ++i) { // wire handle arc
        float a0 = PI * i / 8, a1 = PI * (i + 1) / 8;
        prim::cylinderBetween(Vec3(1.25f + std::cos(a0) * 0.21f, PIER_DECK_Y + 0.38f + std::sin(a0) * 0.2f, 24.2f),
                              Vec3(1.25f + std::cos(a1) * 0.21f, PIER_DECK_Y + 0.38f + std::sin(a1) * 0.2f, 24.2f), 0.008f, 0.008f, 4);
    }

    // ---- coiled rope
    prim::specular(0.05f, 8.0f);
    glColor3f(0.82f, 0.74f, 0.55f);
    for (int k = 0; k < 3; ++k) {
        glPushMatrix();
        glTranslatef(1.30f, PIER_DECK_Y + 0.035f + k * 0.055f, 27.2f);
        glRotatef(90, 1, 0, 0);
        prim::torus(0.12f - k * 0.015f, 0.18f - k * 0.015f, 6, 18);
        glPopMatrix();
    }

    // ---- lantern post (the lantern glass is drawn dynamically)
    tex::bind(tex::WOOD);
    glColor3f(0.40f, 0.29f, 0.20f);
    prim::boxFromTo(Vec3(-2.12f, 0.0f, 26.93f), Vec3(-1.98f, 2.75f, 27.07f), 1.0f);
    prim::boxFromTo(Vec3(-2.05f, 2.62f, 26.97f), Vec3(-1.55f, 2.70f, 27.03f), 1.0f);
    tex::unbind();
    prim::specular(0.6f, 50.0f);
    glColor3f(0.12f, 0.12f, 0.12f);
    prim::boxFromTo(Vec3(-1.72f, 2.42f, 26.90f), Vec3(-1.52f, 2.46f, 27.10f), 1.0f);  // lantern top
    prim::boxFromTo(Vec3(-1.72f, 2.12f, 26.90f), Vec3(-1.52f, 2.16f, 27.10f), 1.0f);  // lantern base
    prim::cylinderBetween(Vec3(-1.62f, 2.46f, 27.0f), Vec3(-1.62f, 2.62f, 27.0f), 0.01f, 0.01f, 4);

    // ---- life ring on the first railing post
    glPushMatrix();
    glTranslatef(POST_X + 0.1f, PIER_DECK_Y + 0.55f, RAIL_START_Z + 1.0f);
    glRotatef(90, 0, 1, 0);
    glColor3f(0.92f, 0.22f, 0.15f);
    prim::specular(0.3f, 30.0f);
    prim::torus(0.22f, 0.40f, 8, 24);
    glColor3f(0.96f, 0.96f, 0.94f);
    for (int k = 0; k < 4; ++k) {
        glPushMatrix();
        glRotatef(45.0f + 90.0f * k, 0, 0, 1);
        glTranslatef(0.31f, 0, 0);
        prim::box(0.06f, 0.10f, 0.11f);
        glPopMatrix();
    }
    glPopMatrix();

    // ---- spare rod leaning on the railing
    glColor3f(0.15f, 0.18f, 0.25f);
    prim::cylinderBetween(Vec3(-1.7f, PIER_DECK_Y, 34.2f), Vec3(-1.95f, PIER_DECK_Y + 2.4f, 35.6f), 0.02f, 0.006f, 6);
    glColor3f(0.75f, 0.62f, 0.42f);
    prim::cylinderBetween(Vec3(-1.7f, PIER_DECK_Y, 34.2f), Vec3(-1.74f, PIER_DECK_Y + 0.4f, 34.43f), 0.026f, 0.024f, 8);

    // ---- mooring cleat for the boat
    glColor3f(0.2f, 0.2f, 0.22f);
    prim::boxFromTo(Vec3(1.65f, PIER_DECK_Y, 31.6f), Vec3(1.75f, PIER_DECK_Y + 0.06f, 32.0f), 1.0f);
    glPopMatrix();glEndList();
}

void Pier::buildBoat() {
    // Hull from cross-sections ("stations") along Z: bow at +Z.
    const int ST = 12;
    auto halfW = [](float t) { return 0.68f * (1.0f - 0.85f * t * t * t * t) * (t > 0 ? 1.0f - 0.55f * t * t : 1.0f); };
    boatList_ = glGenLists(1);
    glNewList(boatList_, GL_COMPILE);
    prim::specular(0.25f, 25.0f);
    for (float side : {-1.0f, 1.0f}) {
        glBegin(GL_QUAD_STRIP);
        for (int i = 0; i <= ST; ++i) {
            float t = -1.0f + 2.0f * i / ST;
            float z = t * 1.8f;
            float w = halfW(t);
            float topY = 0.32f + 0.10f * t * t;
            glNormal3f(side, -0.4f, 0);
            glColor3f(0.88f, 0.88f, 0.84f);
            glVertex3f(side * w * 0.55f, -0.22f, z);
            glColor3f(0.78f, 0.17f, 0.13f);
            glVertex3f(side * w, topY, z);
        }
        glEnd();
    }
    // bottom
    glColor3f(0.60f, 0.24f, 0.18f);
    glBegin(GL_QUAD_STRIP);
    for (int i = 0; i <= ST; ++i) {
        float t = -1.0f + 2.0f * i / ST;
        float w = halfW(t) * 0.55f;
        glNormal3f(0, -1, 0);
        glVertex3f(-w, -0.22f, t * 1.8f);
        glVertex3f(w, -0.22f, t * 1.8f);
    }
    glEnd();
    // inner floor boards (wood)
    tex::bind(tex::WOOD);
    glColor3f(0.65f, 0.48f, 0.30f);
    glBegin(GL_QUADS);
    glNormal3f(0, 1, 0);
    glTexCoord2f(0, 0); glVertex3f(-0.33f, -0.12f, -1.5f);
    glTexCoord2f(1, 0); glVertex3f(0.33f, -0.12f, -1.5f);
    glTexCoord2f(1, 3); glVertex3f(0.33f, -0.12f, 1.4f);
    glTexCoord2f(0, 3); glVertex3f(-0.33f, -0.12f, 1.4f);
    glEnd();
    // stern transom
    glColor3f(0.60f, 0.44f, 0.28f);
    prim::boxFromTo(Vec3(-halfW(-1.0f), -0.2f, -1.84f), Vec3(halfW(-1.0f), 0.42f, -1.76f), 1.0f);
    // seats
    glColor3f(0.70f, 0.53f, 0.34f);
    for (float z : {-0.9f, 0.3f}) prim::boxFromTo(Vec3(-halfW(z / 1.8f) * 0.95f, 0.12f, z - 0.15f), Vec3(halfW(z / 1.8f) * 0.95f, 0.17f, z + 0.15f), 1.0f);
    // oars resting inside
    for (float s : {-1.0f, 1.0f}) {
        glColor3f(0.72f, 0.58f, 0.38f);
        prim::cylinderBetween(Vec3(s * 0.25f, 0.2f, -1.2f), Vec3(s * 0.3f, 0.22f, 1.1f), 0.025f, 0.025f, 6);
        glPushMatrix();
        glTranslatef(s * 0.3f, 0.22f, 1.25f);
        prim::box(0.14f, 0.02f, 0.45f);
        glPopMatrix();
    }
    tex::unbind();
    // Warm contrasting gunwale trim.
    glColor3f(1.0f, 0.69f, 0.22f);
    for (float side : {-1.0f, 1.0f})
        for (int i = 0; i < ST; ++i) {
            float t0 = -1.0f + 2.0f * i / ST, t1 = -1.0f + 2.0f * (i + 1) / ST;
            prim::cylinderBetween(Vec3(side * halfW(t0), 0.32f + 0.1f * t0 * t0, t0 * 1.8f),
                                  Vec3(side * halfW(t1), 0.32f + 0.1f * t1 * t1, t1 * 1.8f), 0.035f, 0.035f, 5);
        }
    glEndList();
}

void Pier::init() {
    if(pierList_)glDeleteLists(pierList_,1);
    if(boatList_)glDeleteLists(boatList_,1);
    buildPier();
    buildBoat();
}

void Pier::draw(const Game& game, const Atmosphere& atm, float time) const {
    (void)game;
    glCallList(pierList_);

    // ---- boat floating on the waves: height and tilt follow the water surface
    const Vec3 boatPos=pierPoint(4.6f, 0.0f, 28.0f);
    float h = waveHeight(boatPos.x, boatPos.z, time);
    Vec3 n = waveNormal(boatPos.x, boatPos.z, time);
    glPushMatrix();
    glTranslatef(boatPos.x, h - 0.02f, boatPos.z);
    glRotatef(-8.0f + 2.0f * std::sin(time * 0.3f), 0, 1, 0);
    glRotatef(rad2deg(n.z) * 1.5f, 1, 0, 0);
    glRotatef(-rad2deg(n.x) * 1.5f, 0, 0, 1);
    glScalef(1.25f, 1.15f, 1.25f);
    glCallList(boatList_);
    glPopMatrix();

    // mooring rope from the cleat to the bow (sagging curve)
    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glLineWidth(2.0f);
    glColor3f(0.75f, 0.68f, 0.50f);
    Vec3 a=pierPoint(1.7f, PIER_DECK_Y + 0.05f, 31.8f);
    Vec3 b(boatPos.x - 0.25f, h + 0.35f, boatPos.z + 1.6f);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i <= 16; ++i) {
        float t = i / 16.0f;
        Vec3 p = lerpv(a, b, t);
        p.y -= std::sin(t * PI) * 0.45f;
        glVertex3f(p.x, p.y, p.z);
    }
    glEnd();
    glPopAttrib();

    // ---- lantern glass: emissive when the evening comes
    float glow = atm.lampGlow;
    GLfloat em[4] = {1.0f * glow, 0.75f * glow, 0.35f * glow, 1.0f};
    GLfloat none[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, em);
    prim::color(lerpc(Color(.65f,.64f,.48f),Color(1,.72f,.28f),glow));
    prim::boxFromTo(pierPoint(-1.70f, 2.16f, 26.92f), pierPoint(-1.54f, 2.42f, 27.08f), 1.0f);
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
}

void Pier::drawShadows() const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    // Shadow of the deck on the lake bed / beach (sun from the front-right).
    for (float z = PIER_Z_START + 0.5f; z < PIER_Z_END; z += 1.6f) {
        float x = PIER_X + 0.9f;
        prim::blobShadow(x, terrainHeight(x, z) + 0.12f, z, 2.6f, 1.4f, 0.30f);
    }
    glPushMatrix();glTranslatef(PIER_X,0,PIER_Z_START-22);
    // Props on the deck
    prim::blobShadow(1.30f, PIER_DECK_Y + 0.005f, 24.25f, 0.30f, 0.30f, 0.35f);
    prim::blobShadow(-1.20f, PIER_DECK_Y + 0.005f, 24.60f, 0.38f, 0.26f, 0.35f);
    prim::blobShadow(-1.28f, PIER_DECK_Y + 0.005f, 26.0f, 0.42f, 0.42f, 0.35f);
    glPopMatrix();glPopAttrib();
}

} // namespace lf
