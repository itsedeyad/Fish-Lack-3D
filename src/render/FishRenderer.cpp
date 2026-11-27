#include "render/FishRenderer.h"
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include "game/Fish.h"
#include "game/Types.h"
#include "core/World.h"
#include <cmath>
#include <initializer_list>

namespace lf {

static const float RENDER_SCALE = FISH_VISUAL_SCALE;

// Body proportions relative to the body length: half-width and half-height.
struct BodyShape { float w, h; };
static BodyShape shapeOf(int type) {
    switch (type) {
    case FISH_PERCH: return {.08f,.14f};
    case FISH_CATFISH: return {.115f,.12f};
    case FISH_PIKE: return {.055f,.10f};
    case FISH_TROUT: return {0.072f, 0.125f};
    case FISH_CARP:  return {0.105f, 0.195f};
    default:         return {0.085f, 0.150f};
    }
}

// Radius profile along the body, t = 0 at the tail, 1 at the nose.
static float profile(float t) {
    return 0.12f + 0.88f * std::pow(std::sin(PI * std::pow(clampf(t, 0, 1), 1.4f)), 0.8f);
}

static Color fishVertexColor(int type, float t, float sa, int i, int j) {
    const FishSpec& sp = fishSpec(type);
    float s = smoothstep(-0.35f, 0.45f, sa);
    Color c = lerpc(sp.belly, sp.back, s);
    switch (type) {
    case FISH_BLUE: {
        float bar = 0.5f + 0.5f * std::sin(t * 38.0f);
        if (sa > 0.15f && bar > 0.72f && t > 0.1f && t < 0.85f) c = scalec(c, 0.62f);
        if (std::fabs(sa) < 0.08f) c = lerpc(c, sp.accent, 0.5f);
        break;
    }
    case FISH_TROUT: {
        if (std::fabs(sa) < 0.24f && t > 0.08f && t < 0.9f) c = lerpc(c, sp.accent, 0.75f);
        if (sa > -0.2f && hash2i(i, j, 99) > 0.80f) c = scalec(c, 0.35f);
        break;
    }
    case FISH_PERCH: if(sa>-.3f && std::sin(t*35)> .6f)c=scalec(c,.45f); break;
    case FISH_CATFISH: if(sa>0)c=scalec(c,.82f); break;
    case FISH_PIKE: if(hash2i(i,j,173)>.73f)c=lerpc(c,sp.belly,.6f); break;
    case FISH_CARP: {
        float scale = 0.5f + 0.5f * std::sin(t * 70.0f + ((j % 2) ? PI : 0.0f));
        c = scalec(c, 0.86f + 0.16f * scale);
        if (sa > 0.6f) c = lerpc(c, sp.accent, 0.35f);
        break;
    }
    }
    if (std::fabs(t - 0.79f) < 0.018f && sa < 0.6f) c = scalec(c, 0.7f); // gill line
    return c;
}

void FishRenderer::init() {
    const int RINGS = 26, SEGS = 18;
    bodyLists_ = glGenLists(FISH_TYPE_COUNT);
    for (int type = 0; type < FISH_TYPE_COUNT; ++type) {
        BodyShape bs = shapeOf(type);
        glNewList(bodyLists_ + type, GL_COMPILE);
        for (int i = 0; i < RINGS; ++i) {
            glBegin(GL_QUAD_STRIP);
            for (int j = 0; j <= SEGS; ++j) {
                float a = TWO_PI * j / SEGS;
                float ca = std::cos(a), sa = std::sin(a);
                for (int k = 1; k >= 0; --k) {
                    int ri = i + k;
                    float t = float(ri) / RINGS;
                    float r = profile(t);
                    float dr = (profile(t + 0.01f) - profile(t - 0.01f)) / 0.02f;
                    float z = -0.5f + t;
                    float yoff = bs.h * r * 0.08f; // back slightly more arched than belly
                    Vec3 n(ca / bs.w, sa / bs.h, -dr * 0.12f);
                    n = n.normalized();
                    Color c = fishVertexColor(type, t, sa, ri, j);
                    glColor3f(c.r, c.g, c.b);
                    glNormal3f(n.x, n.y, n.z);
                    glVertex3f(ca * bs.w * r, sa * bs.h * r + yoff, z);
                }
            }
            glEnd();
        }
        // Nose cap and tail cap.
        const FishSpec& sp = fishSpec(type);
        for (int end = 0; end < 2; ++end) {
            float t = end ? 1.0f : 0.0f;
            float r = profile(t);
            float z = -0.5f + t;
            glBegin(GL_TRIANGLE_FAN);
            glColor3f(sp.back.r * 0.9f, sp.back.g * 0.9f, sp.back.b * 0.9f);
            glNormal3f(0, 0, end ? 1.0f : -1.0f);
            glVertex3f(0, bs.h * r * 0.08f, end ? z + 0.035f : z - 0.01f);
            for (int j = 0; j <= SEGS; ++j) {
                float a = TWO_PI * (end ? j : SEGS - j) / SEGS;
                Color c = fishVertexColor(type, t, std::sin(a), end ? RINGS : 0, j);
                glColor3f(c.r, c.g, c.b);
                glNormal3f(std::cos(a) * 0.6f, std::sin(a) * 0.6f, end ? 0.8f : -0.8f);
                glVertex3f(std::cos(a) * bs.w * r, std::sin(a) * bs.h * r + bs.h * r * 0.08f, z);
            }
            glEnd();
        }
        glEndList();
    }
}

static void tri(const Vec3& a, const Vec3& b, const Vec3& c) {
    Vec3 n = cross(b - a, c - a).normalized();
    glNormal3f(n.x, n.y, n.z);
    glVertex3f(a.x, a.y, a.z);
    glVertex3f(b.x, b.y, b.z);
    glVertex3f(c.x, c.y, c.z);
}

void FishRenderer::drawModel(int type, float length, float tailAngle, float finPhase) const {
    const FishSpec& sp = fishSpec(type);
    BodyShape bs = shapeOf(type);
    Color fin = lerpc(sp.back, sp.accent, 0.55f);
    if (type == FISH_CARP || type == FISH_PERCH) fin = Color(0.95f, 0.45f, 0.10f);

    glPushMatrix();
    glScalef(length, length, length);
    // Slight whole-body counter-swing makes the swimming read as an S-wave.
    glRotatef(-rad2deg(tailAngle) * 0.18f, 0, 1, 0);
    glCallList(bodyLists_ + type);

    float H = bs.h, W = bs.w;
    glColor3f(fin.r, fin.g, fin.b);
    glBegin(GL_TRIANGLES);
    // dorsal fin
    float dz0 = type == FISH_CARP ? -0.22f : -0.12f, dz1 = type == FISH_CARP ? 0.20f : 0.14f;
    tri(Vec3(0, H * 0.92f, dz1), Vec3(0, H * 0.92f, dz0), Vec3(0, H * 1.65f, dz0 + 0.03f));
    // anal fin
    tri(Vec3(0, -H * 0.80f, -0.16f), Vec3(0, -H * 0.80f, -0.30f), Vec3(0, -H * 1.30f, -0.30f));
    // pelvic fins
    for (float s : {-1.0f, 1.0f}) tri(Vec3(s * W * 0.5f, -H * 0.85f, 0.10f), Vec3(s * W * 0.5f, -H * 0.85f, -0.02f), Vec3(s * W * 1.4f, -H * 1.25f, -0.06f));
    glEnd();

    // pectoral fins: flap with the fin phase
    for (float s : {-1.0f, 1.0f}) {
        glPushMatrix();
        glTranslatef(s * W * 0.9f, -H * 0.30f, 0.22f);
        glRotatef(s * (25.0f + 20.0f * std::sin(finPhase)), 0, 1, 0);
        glBegin(GL_TRIANGLES);
        tri(Vec3(0, 0, 0.03f), Vec3(0, 0, -0.06f), Vec3(s * W * 1.6f, -H * 0.25f, -0.12f));
        glEnd();
        glPopMatrix();
    }

    // tail: hierarchical joints  body -> peduncle -> caudal fin
    glPushMatrix();
    glTranslatef(0, H * 0.01f, -0.47f);
    glRotatef(rad2deg(tailAngle) * 0.5f, 0, 1, 0);       // peduncle joint
    glColor3f(sp.back.r, sp.back.g, sp.back.b);
    glPushMatrix();
    glTranslatef(0, 0, -0.06f);
    glScalef(W, H, 1.0f);
    prim::cylinderZ(0.08f, 0.14f, 0.07f, 8, false);
    glPopMatrix();
    glTranslatef(0, 0, -0.05f);
    glRotatef(rad2deg(tailAngle) * 0.8f, 0, 1, 0);       // caudal fin joint
    glColor3f(fin.r, fin.g, fin.b);
    float tailH = H * (type == FISH_CARP ? 1.25f : 1.4f);
    glBegin(GL_TRIANGLES);
    tri(Vec3(0, 0, 0.02f), Vec3(0, tailH, -0.26f), Vec3(0, 0, -0.13f));
    tri(Vec3(0, 0, 0.02f), Vec3(0, 0, -0.13f), Vec3(0, -tailH, -0.26f));
    glEnd();
    glPopMatrix();

    // eyes
    for (float s : {-1.0f, 1.0f}) {
        float r = profile(0.88f);
        glPushMatrix();
        glTranslatef(s * W * r * 0.85f, H * 0.28f, 0.37f);
        glColor3f(0.95f, 0.95f, 0.9f);
        prim::sphere(0.032f, 8, 6);
        glTranslatef(s * 0.016f, 0.003f, 0.008f);
        glColor3f(0.02f, 0.02f, 0.02f);
        prim::sphere(0.019f, 8, 6);
        glPopMatrix();
    }
    if(type==FISH_PIKE) {
        prim::color(sp.back);prim::boxFromTo(Vec3(-.035f,-.025f,.45f),Vec3(.035f,.025f,.65f));
        prim::color(sp.belly);prim::boxFromTo(Vec3(-.036f,-.027f,.46f),Vec3(.036f,-.016f,.65f));
    }
    if(type==FISH_CATFISH) {
        prim::color(Color(.75f,.66f,.48f));
        for(float side : {-1.f,1.f})for(int i=0;i<2;++i)
            prim::cylinderBetween(Vec3(side*.03f,-.02f,.48f),Vec3(side*(.15f+i*.05f),-.07f-i*.04f,.55f),.005f,.002f,4);
    }
    // carp barbels
    if (type == FISH_CARP) {
        glColor3f(0.80f, 0.55f, 0.25f);
        for (float s : {-1.0f, 1.0f})
            prim::cylinderBetween(Vec3(s * 0.02f, -0.03f, 0.49f), Vec3(s * 0.07f, -0.09f, 0.56f), 0.006f, 0.003f, 4);
    }
    glPopMatrix();
}

void FishRenderer::drawSchool(const FishSchool& school, float time) const {
    (void)time;
    prim::specular(0.55f, 45.0f);
    for (const Fish& f : school.fish) {
        if (!f.active()) continue;
        const FishSpec& sp = fishSpec(f.type);
        float tailAngle = std::sin(f.tailPhase) * f.tailAmp;
        glPushMatrix();
        glTranslatef(f.pos.x, f.pos.y, f.pos.z);
        glRotatef(rad2deg(f.yaw), 0, 1, 0);
        glRotatef(rad2deg(f.pitch), 1, 0, 0);
        glRotatef(rad2deg(f.roll), 0, 0, 1);
        // A faint emissive tint keeps the fish readable through the water.
        GLfloat em[4] = {sp.back.r * 0.22f, sp.back.g * 0.22f, sp.back.b * 0.22f, 1.0f};
        glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, em);
        drawModel(f.type, sp.length * f.sizeScale * RENDER_SCALE, tailAngle, f.tailPhase * 0.7f);
        if(f.suspicious){
            glPushMatrix();glScalef(sp.length*f.sizeScale*RENDER_SCALE,sp.length*f.sizeScale*RENDER_SCALE,sp.length*f.sizeScale*RENDER_SCALE);
            prim::color(Color(.58f,.23f,.67f));
            for(int side:{-1,1})for(int i=0;i<4;++i){glPushMatrix();glTranslatef(side*.16f,.045f+(i%2)*.045f,-.18f+i*.13f);prim::sphere(.024f,5,4);glPopMatrix();}
            glPopMatrix();
        }
        glPopMatrix();
    }
    GLfloat none[4] = {0, 0, 0, 1};
    glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, none);
    prim::specular(0.1f, 10.0f);
}

void FishRenderer::drawShadows(const FishSchool& school) const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    for (const Fish& f : school.fish) {
        if (!f.active() || f.pos.y > 0.2f) continue;
        float bed = world::terrainHeight(f.pos.x, f.pos.z);
        float gap = f.pos.y - bed;
        float a = 0.32f * (1.0f - smoothstep(0.5f, 4.0f, gap));
        float len = fishSpec(f.type).length * f.sizeScale * RENDER_SCALE;
        glPushMatrix();
        glTranslatef(f.pos.x + 0.3f, bed + 0.05f, f.pos.z + 0.3f);
        glRotatef(rad2deg(f.yaw), 0, 1, 0);
        prim::blobShadow(0, 0, 0, len * 0.22f, len * 0.55f, a);
        glPopMatrix();
    }
    glPopAttrib();
}

} // namespace lf
