#include "render/Terrain.h"
#include "render/GLHeaders.h"
#include "render/Primitives.h"
#include "render/Textures.h"
#include "core/World.h"
#include <cmath>
#include <initializer_list>

namespace lf {

using world::terrainHeight;
using world::lakeDistance;

static const float GROUND_STEP = 1.6f;

// --------------------------------------------------------- ground colours --
static Color groundColor(float x, float z, float h, const Vec3& n) {
    const Color bedShallow(0.80f, 0.72f, 0.50f), bedDeep(0.12f, 0.30f, 0.32f);
    const Color sand(0.96f, 0.86f, 0.62f), wetSand(0.72f, 0.62f, 0.44f);
    const Color grass1(0.32f, 0.62f, 0.20f), grass2(0.56f, 0.76f, 0.28f), dry(0.84f, 0.74f, 0.38f);
    const Color dirt(0.52f, 0.41f, 0.28f), rock(0.64f, 0.62f, 0.58f);
    if (h < -0.12f) return lerpc(bedShallow, bedDeep, smoothstep(0.1f, 3.6f, -h));

    float n1 = fbm(x * 0.05f, z * 0.05f, 3, 101);
    Color g = lerpc(grass1, grass2, n1);
    g = lerpc(g, dry, smoothstep(0.55f, 0.8f, fbm(x * 0.02f + 5, z * 0.02f, 3, 103)) * 0.6f);
    float sandW = 1.0f - smoothstep(0.45f, 1.25f, h + (n1 - 0.5f) * 0.6f);
    Color c = lerpc(g, sand, sandW);
    if(world::activeMap()==MapId::Highland)c=lerpc(c,rock,.65f*smoothstep(1.1f,1.8f,lakeDistance(x,z)));
    if(world::activeMap()==MapId::Marsh)c=lerpc(c,Color(.40f,.37f,.18f),.4f);
    if (h < 0.30f) c = lerpc(c, wetSand, 1.0f - smoothstep(0.0f, 0.30f, h));
    if (n.y < 0.86f) c = lerpc(c, rock, smoothstep(0.86f, 0.66f, n.y));
    // Dirt path leading to the pier.
    float px = std::fabs(x - world::PIER_X);
    if (z > world::PIER_Z_END - 4.0f && px < 3.0f && h > 0.3f)
        c = lerpc(c, dirt, (1.0f - smoothstep(1.6f, 3.0f, px + (n1 - 0.5f))) * 0.9f);
    if(world::activeMap()==MapId::Highland && h>17)c=lerpc(c,Color(.90f,.94f,.96f),smoothstep(17,27,h));
    if (h > 16.0f) c = scalec(c, 1.0f - 0.25f * smoothstep(16.0f, 35.0f, h));
    return c;
}

void Terrain::buildGround() {
    const float half = world::TERRAIN_HALF;
    const int N = int(2 * half / GROUND_STEP);
    // Pre-compute one row ahead so every vertex is evaluated once per strip.
    std::vector<Vec3> pos((N + 1) * (N + 1)), nor((N + 1) * (N + 1));
    std::vector<Color> col((N + 1) * (N + 1));
    for (int j = 0; j <= N; ++j)
        for (int i = 0; i <= N; ++i) {
            float x = -half + i * GROUND_STEP, z = -half + j * GROUND_STEP;
            float h = terrainHeight(x, z);
            int k = j * (N + 1) + i;
            pos[k] = Vec3(x, h, z);
            nor[k] = world::terrainNormal(x, z);
            col[k] = groundColor(x, z, h, nor[k]);
        }

    groundList_ = glGenLists(1);
    glNewList(groundList_, GL_COMPILE);
    tex::bind(tex::GROUND);
    prim::specular(0.05f, 8.0f);
    for (int j = 0; j < N; ++j) {
        glBegin(GL_TRIANGLE_STRIP);
        for (int i = 0; i <= N; ++i)
            for (int r = 1; r >= 0; --r) {
                int k = (j + r) * (N + 1) + i;
                glColor3f(col[k].r, col[k].g, col[k].b);
                glNormal3f(nor[k].x, nor[k].y, nor[k].z);
                glTexCoord2f(pos[k].x * 0.22f, pos[k].z * 0.22f);
                glVertex3f(pos[k].x, pos[k].y, pos[k].z);
            }
        glEnd();
    }
    tex::unbind();
    glEndList();
}

// ------------------------------------------------------------ rock shapes --
// Reusable cuboid rocks vary their height for the block-style scenery.
void Terrain::buildRockShapes() {
    rockShapes_=glGenLists(ROCK_SHAPES);
    for(int i=0;i<ROCK_SHAPES;++i) {
        glNewList(rockShapes_+i,GL_COMPILE);
        prim::boxFromTo(Vec3(-1,-.2f,-.8f),Vec3(1,.65f+(i%3)*.13f,.8f));
        glEndList();
    }
}
static void drawPine(float s,const Color& tint,Rng& rng) {
    (void)rng;
    prim::color(Color(.37f,.23f,.12f));prim::boxFromTo(Vec3(-.21f*s,0,-.21f*s),Vec3(.21f*s,4.5f*s,.21f*s));
    for(int tier=0;tier<5;++tier) {
        float radius=(2.05f-tier*.34f)*s, y=(1.5f+tier*.84f)*s;
        prim::color(Color(.16f+tier*.017f+tint.r*.25f,.34f+tier*.025f+tint.g*.25f,.12f+tier*.012f));
        prim::boxFromTo(Vec3(-radius,y,-radius),Vec3(radius,y+.64f*s,radius));
        prim::boxFromTo(Vec3(-radius*.64f,y+.64f*s,-radius*.64f),Vec3(radius*.64f,y+.85f*s,radius*.64f));
    }
}
static void drawBroadleaf(float s,const Color& tint,bool autumn,bool birch,Rng& rng,unsigned blobLists) {
    (void)blobLists;(void)autumn;
    prim::color(birch?Color(.76f,.71f,.56f):Color(.39f,.25f,.12f));
    prim::boxFromTo(Vec3(-.27f*s,0,-.27f*s),Vec3(.27f*s,3.9f*s,.27f*s));
    for(int k=0;k<9;++k) {
        float x=float(k%3-1)*1.05f*s,z=float(k/3-1)*1.05f*s,y=(3.5f+rng.range(0,.7f))*s;
        float shade=rng.range(.82f,1.1f);
        prim::color(Color((.28f+tint.r*.2f)*shade,(.48f+tint.g*.25f)*shade,.13f*shade));
        prim::boxFromTo(Vec3(x-1.0f*s,y,z-1.0f*s),Vec3(x+1.0f*s,y+1.5f*s,z+1.0f*s));
    }
}

static void drawWillow(float s,const Color&) {
    prim::color(Color(.30f,.24f,.12f));prim::boxFromTo(Vec3(-.25f*s,0,-.25f*s),Vec3(.25f*s,4.5f*s,.25f*s));
    prim::color(Color(.29f,.43f,.15f));prim::boxFromTo(Vec3(-2.1f*s,3.5f*s,-1.8f*s),Vec3(2.1f*s,4.7f*s,1.8f*s));
    for(int k=0;k<12;++k){float a=TWO_PI*k/12,x=std::cos(a)*1.9f*s,z=std::sin(a)*1.6f*s;
        prim::color(Color(.34f+(k%3)*.025f,.48f,.17f));prim::boxFromTo(Vec3(x-.28f*s,1.4f*s+(k%3)*.3f,z-.28f*s),Vec3(x+.28f*s,4.2f*s,z+.28f*s));}
}
void Terrain::buildScenery() {
    Rng rng(world::mapSpec(world::activeMap()).seed);
    auto tooClose = [](const std::vector<Placed>& v, float x, float z, float minD) {
        for (const Placed& p : v)
            if (std::fabs(p.pos.x - x) < minD && std::fabs(p.pos.z - z) < minD &&
                distXZ(p.pos, Vec3(x, 0, z)) < minD) return true;
        return false;
    };
    auto nearPier = [](float x, float z, float m) {
        return std::fabs(x - world::PIER_X) < 4.5f + m && z > world::PIER_Z_START - 4.0f - m;
    };
    auto cameraZone = [](float x, float z) { return distXZ(Vec3(x,0,z),world::anglerPosition())<15 || (std::fabs(x-world::PIER_X)<14 && z<-world::LAKE_RZ+5); };

    // ---- trees: clustered using a low-frequency noise "forest density" map
    for (int i = 0; i < 2500 && trees_.size() < 190; ++i) {
        float x = rng.range(-150, 150), z = rng.range(-150, 150);
        float h = terrainHeight(x, z);
        float d = lakeDistance(x, z);
        if (d < 1.14f || h < 0.9f || nearPier(x, z, 2) || cameraZone(x, z)) continue;
        if (world::terrainNormal(x, z).y < 0.78f) continue;
        float density = fbm(x * 0.03f, z * 0.03f, 3, 909);
        if (!rng.chance(0.15f + 0.85f * smoothstep(0.42f, 0.62f, density))) continue;
        if (world::activeMap()==MapId::Highland && !rng.chance(.42f))continue;
        if (tooClose(trees_, x, z, 4.5f)) continue;
        Placed p;
        p.pos = Vec3(x, h - 0.1f, z);
        p.scale = rng.range(0.75f, 1.35f) * (d > 2.0f ? 1.25f : 1.0f);
        p.rot = rng.range(0, 360);
        float k = rng.uniform();
        p.kind = k < 0.35f ? 0 : (k < 0.90f ? 1 : 2);
        p.tint = Color(rng.range(-0.04f, 0.04f), rng.range(-0.06f, 0.06f), rng.range(-0.03f, 0.03f));
        trees_.push_back(p);
    }
    // Deliberate shoreline forest: enough height and density to frame first-person fishing.
    for(int i=0;i<54;++i) {
        if(world::activeMap()==MapId::Highland && i%2)continue;
        float a=TWO_PI*i/54, radius=1.20f+0.09f*(i%3);
        float x=std::cos(a)*world::LAKE_RX*radius,z=std::sin(a)*world::LAKE_RZ*radius;
        float h=terrainHeight(x,z);
        if(h<.5f || nearPier(x,z,8) || cameraZone(x,z))continue;
        Placed p;p.pos=Vec3(x,h-.1f,z);p.scale=1.45f+(i%4)*.15f;p.rot=0;p.kind=0;p.tint=Color(0,.02f*(i%3),0);trees_.push_back(p);
    }
    // ---- bushes near the shore
    for (int i = 0; i < 1500 && bushes_.size() < 110; ++i) {
        float x = rng.range(-110, 110), z = rng.range(-100, 110);
        float d = lakeDistance(x, z);
        float h = terrainHeight(x, z);
        if (d < 1.05f || d > 1.9f || h < 0.4f || nearPier(x, z, 0) || cameraZone(x, z)) continue;
        if (tooClose(bushes_, x, z, 2.2f) || tooClose(trees_, x, z, 1.8f)) continue;
        Placed p;
        p.pos = Vec3(x, h - 0.1f, z);
        p.scale = rng.range(0.6f, 1.2f);
        p.rot = rng.range(0, 360);
        p.kind = rng.irange(0, 2);
        p.tint = Color(rng.range(-0.05f, 0.05f), rng.range(-0.05f, 0.08f), 0);
        bushes_.push_back(p);
    }
    // ---- rocks: along the shoreline, in the fields, and a few boulders
    for (int i = 0; i < 4000 && rocks_.size() < (world::activeMap()==MapId::Highland?130u:world::activeMap()==MapId::Marsh?45u:95u); ++i) {
        float x = rng.range(-140, 140), z = rng.range(-140, 140);
        float d = lakeDistance(x, z);
        float h = terrainHeight(x, z);
        bool shore = d > 0.96f && d < 1.09f;
        bool field = d > 1.15f && rng.chance(0.25f);
        if (!(shore || field) || nearPier(x, z, 1.5f) || cameraZone(x, z)) continue;
        if (tooClose(rocks_, x, z, 2.5f) || tooClose(trees_, x, z, 2.0f)) continue;
        Placed p;
        p.pos = Vec3(x, h - 0.15f, z);
        p.scale = shore ? rng.range(0.4f, 1.3f) : rng.range(0.5f, 2.2f);
        if (rng.chance(0.06f)) p.scale = rng.range(2.5f, 3.8f);
        p.rot = rng.range(0, 360);
        p.kind = rng.irange(0, ROCK_SHAPES - 1);
        float g = rng.range(0.42f, 0.62f);
        p.tint = Color(g, g * rng.range(0.95f, 1.02f), g * rng.range(0.9f, 1.0f));
        rocks_.push_back(p);
    }

    sceneryList_ = glGenLists(1);
    glNewList(sceneryList_, GL_COMPILE);
    glShadeModel(GL_FLAT);   // low-poly look: one colour per facet
    prim::specular(0.04f, 8.0f);

    Rng tr(77);
    for (const Placed& p : trees_) {
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, p.pos.z);
        glRotatef(p.rot, 0, 1, 0);
        if(world::activeMap()==MapId::Marsh) drawWillow(p.scale,p.tint);
        else if (p.kind == 0 || world::activeMap()==MapId::Highland) drawPine(p.scale, p.tint, tr);
        else drawBroadleaf(p.scale, p.tint, p.kind == 1 && tr.chance(0.60f), p.kind == 2, tr, rockShapes_);
        glPopMatrix();
    }
    for (const Placed& p : bushes_) {
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, p.pos.z);
        glRotatef(p.rot, 0, 1, 0);
        int n = 3 + p.kind;
        for (int k = 0; k < n; ++k) {
            float a = TWO_PI * k / n;
            float sh = 0.85f + 0.08f * k;
            glColor3f((0.20f + p.tint.r) * sh, (0.42f + p.tint.g) * sh, (0.15f + p.tint.b) * sh);
            glPushMatrix();
            glTranslatef(std::cos(a) * 0.5f * p.scale, 0.45f * p.scale, std::sin(a) * 0.5f * p.scale);
            glScalef(1.0f, 0.75f, 1.0f);
            prim::box(1.1f*p.scale,.9f*p.scale,1.1f*p.scale);
            glPopMatrix();
        }
        // a few berries / flowers on some bushes
        if (p.kind == 2) {
            glColor3f(0.85f, 0.15f, 0.20f);
            for (int k = 0; k < 6; ++k) {
                glPushMatrix();
                float a = k * 1.1f;
                glTranslatef(std::cos(a) * 0.8f * p.scale, (0.6f + 0.15f * (k % 3)) * p.scale, std::sin(a) * 0.8f * p.scale);
                prim::sphere(0.07f, 5, 4);
                glPopMatrix();
            }
        }
        glPopMatrix();
    }

    tex::bind(tex::GROUND);
    prim::specular(0.12f, 20.0f);
    for (const Placed& p : rocks_) {
        glPushMatrix();
        glTranslatef(p.pos.x, p.pos.y, p.pos.z);
        glRotatef(p.rot, 0, 1, 0);
        glScalef(p.scale, p.scale, p.scale);
        glColor3f(p.tint.r, p.tint.g, p.tint.b);
        glCallList(rockShapes_ + p.kind);
        glPopMatrix();
    }
    tex::unbind();
    prim::specular(0.04f, 8.0f);

    // ---- reeds / cattails in the shallow water along the shore
    Rng rr(515);
    int clusters = 0;
    for (int i = 0; i < 3000 && clusters < (world::activeMap()==MapId::Marsh?80:46); ++i) {
        float x = rr.range(-60, 60), z = rr.range(-50, 50);
        float d = lakeDistance(x, z);
        if (d < 0.95f || d > 1.01f || nearPier(x, z, 3) || cameraZone(x, z)) continue;
        ++clusters;
        int stalks = rr.irange(6, 13);
        for (int k = 0; k < stalks; ++k) {
            float sx = x + rr.range(-0.8f, 0.8f), sz = z + rr.range(-0.8f, 0.8f);
            float base = terrainHeight(sx, sz);
            float hgt = rr.range(1.2f, 2.2f) - std::fmin(base, 0.0f);
            Vec3 a(sx, base, sz);
            Vec3 b(sx + rr.range(-0.25f, 0.25f), base + hgt, sz + rr.range(-0.25f, 0.25f));
            glColor3f(0.32f + rr.range(0, 0.1f), 0.50f + rr.range(0, 0.1f), 0.18f);
            prim::cylinderBetween(a, b, 0.035f, 0.015f, 4);
            if (rr.chance(0.45f)) {
                Vec3 dir = (b - a).normalized();
                glColor3f(0.40f, 0.25f, 0.14f);
                prim::cylinderBetween(b - dir * 0.45f, b - dir * 0.12f, 0.07f, 0.07f, 6);
            }
        }
    }

    // ---- wild flowers and grass tufts in the meadows
    Rng fr(909);
    const Color petals[] = {Color(1.0f, 0.9f, 0.25f), Color(0.97f, 0.97f, 0.95f), Color(0.70f, 0.45f, 0.90f),
                            Color(0.95f, 0.35f, 0.35f)};
    for (int i = 0; i < 2600; ++i) {
        float x = fr.range(-90, 90), z = fr.range(-80, 100);
        float d = lakeDistance(x, z);
        float h = terrainHeight(x, z);
        if (d < 1.06f || h < 0.8f || nearPier(x, z, 0.5f)) continue;
        if (fr.chance(0.35f)) {
            const Color& c = petals[fr.irange(0, 3)];
            glColor3f(c.r, c.g, c.b);
            glPushMatrix();
            glTranslatef(x, h + 0.18f, z);
            glScalef(1, 0.4f, 1);
            prim::sphere(0.09f, 5, 3);
            glPopMatrix();
        } else {
            // grass tuft: three thin blades fanning out, tinted like the ground
            if (!fr.chance(0.5f)) continue;
            float g = fr.range(0.78f, 0.95f);
            Color gc = groundColor(x, z, h, world::terrainNormal(x, z));
            glColor3f(gc.r * g, gc.g * g, gc.b * g);
            glBegin(GL_TRIANGLES);
            for (int k = 0; k < 3; ++k) {
                float a = fr.range(0, TWO_PI);
                float lx = std::cos(a) * 0.25f, lz = std::sin(a) * 0.25f;
                glNormal3f(-lz, 0.3f, lx);
                glVertex3f(x - lz * 0.2f, h, z + lx * 0.2f);
                glVertex3f(x + lz * 0.2f, h, z - lx * 0.2f);
                glVertex3f(x + lx, h + fr.range(0.35f, 0.6f), z + lz);
            }
            glEnd();
        }
    }

    // ---- distant mountain ring (fog tints them with the horizon colour)
    Rng mr(31337);
    for (int i = 0; i < 30; ++i) {
        float a = TWO_PI * i / 30 + mr.range(-0.08f, 0.08f);
        if(std::fabs(std::cos(a))<.5f && std::sin(a)<-.8f)continue; // clear sunset valley
        float d = mr.range(240.0f, 300.0f);
        float h = mr.range(55.0f, 115.0f)*(world::activeMap()==MapId::Highland?1.3f:world::activeMap()==MapId::Marsh?.35f:1);
        float r = mr.range(55.0f, 85.0f);
        glColor3f(0.24f + mr.range(0, 0.06f), 0.33f + mr.range(0, 0.06f), 0.30f);
        glPushMatrix();
        glTranslatef(std::cos(a) * d, -5.0f, std::sin(a) * d);
        glRotatef(mr.range(0, 360), 0, 1, 0);
        for(int tier=0;tier<8;++tier) {
            float width=r*(1-tier*.115f);
            if(world::activeMap()==MapId::Highland && tier>=5)prim::color(Color(.85f,.89f,.94f));
            prim::boxFromTo(Vec3(-width,h*tier/8,-width*.7f),Vec3(width,h*(tier+1)/8,width*.7f));
        }
        glPopMatrix();
    }
    glShadeModel(GL_SMOOTH);
    glEndList();
}

void Terrain::buildShadows() {
    shadowList_ = glGenLists(1);
    glNewList(shadowList_, GL_COMPILE);
    for (const Placed& p : trees_)
        prim::blobShadow(p.pos.x + 0.6f, terrainHeight(p.pos.x, p.pos.z) + 0.08f, p.pos.z + 0.6f,
                         2.4f * p.scale, 2.2f * p.scale, 0.38f);
    for (const Placed& p : bushes_)
        prim::blobShadow(p.pos.x + 0.2f, terrainHeight(p.pos.x, p.pos.z) + 0.07f, p.pos.z + 0.2f,
                         1.2f * p.scale, 1.1f * p.scale, 0.30f);
    for (const Placed& p : rocks_) {
        float h = terrainHeight(p.pos.x, p.pos.z);
        if (h > 0.05f) prim::blobShadow(p.pos.x + 0.3f * p.scale, h + 0.06f, p.pos.z + 0.3f * p.scale,
                                        1.5f * p.scale, 1.3f * p.scale, 0.32f);
    }
    glEndList();
}

void Terrain::init() {
    if(groundList_)glDeleteLists(groundList_,1);
    if(sceneryList_)glDeleteLists(sceneryList_,1);
    if(shadowList_)glDeleteLists(shadowList_,1);
    if(rockShapes_)glDeleteLists(rockShapes_,ROCK_SHAPES);
    trees_.clear();rocks_.clear();bushes_.clear();
    buildGround();
    buildRockShapes();
    buildScenery();
    buildShadows();
}

void Terrain::drawGround() const { glCallList(groundList_); }

void Terrain::drawScenery(float time,float lampGlow) const {
    glCallList(sceneryList_);
    if(world::activeMap()==MapId::Marsh)return;
    // A small cabin and terraced waterfall are decorative, outside the castable lake.
    glPushAttrib(GL_ENABLE_BIT | GL_CURRENT_BIT | GL_LIGHTING_BIT);
    tex::unbind();
    float bx=-world::LAKE_RX-5, bz=-9, base=terrainHeight(bx,bz);
    glPushMatrix();glTranslatef(bx,base,bz);
    prim::color(Color(.42f,.23f,.10f));prim::boxFromTo(Vec3(-5,0,-4),Vec3(5,6,4));
    for(int row=0;row<9;++row) {
        prim::color(row%2?Color(.49f,.28f,.12f):Color(.57f,.33f,.15f));
        prim::boxFromTo(Vec3(-5.15f,row*.65f,4),Vec3(5.15f,row*.65f+.51f,4.15f));
    }
    for(int layer=0;layer<5;++layer) {
        float w=6-layer*1.05f;
        prim::color(Color(.31f+layer*.025f,.17f+layer*.012f,.08f));
        prim::boxFromTo(Vec3(-w,6+layer*.65f,-5),Vec3(w,6.65f+layer*.65f,5));
    }
    prim::color(Color(.22f,.13f,.07f));prim::boxFromTo(Vec3(-1,0,4.02f),Vec3(1,4.3f,4.15f));
    glDisable(GL_LIGHTING);
    for(int k=-1;k<=1;k+=2) {prim::color(lerpc(Color(.39f,.30f,.19f),Color(1,.67f,.22f),lampGlow));prim::boxFromTo(Vec3(k*3.f-1,2,4.08f),Vec3(k*3.f+1,4,4.2f));}
    glEnable(GL_LIGHTING);
    prim::color(Color(.24f,.14f,.07f));
    for(int k=-1;k<=1;k+=2){prim::boxFromTo(Vec3(k*3.f-.08f,2,4.21f),Vec3(k*3.f+.08f,4,4.25f));prim::boxFromTo(Vec3(k*3.f-1,2.9f,4.21f),Vec3(k*3.f+1,3.05f,4.25f));}
    // porch and square posts
    prim::color(Color(.67f,.39f,.16f));prim::boxFromTo(Vec3(-6,-.2f,4),Vec3(6,.3f,8));
    for(int k=-1;k<=1;k+=2)prim::boxFromTo(Vec3(k*5.5f-.15f,0,7),Vec3(k*5.5f+.15f,5,7.3f));
    glPopMatrix();
    if(world::activeMap()==MapId::Highland){glPopAttrib();return;}
    const float wx=28,wz=-37, floor=terrainHeight(wx,wz);
    for(int block=0;block<10;++block) {
        float side=block<5?-1.f:1.f,shift=float(block%5);
        prim::color(Color(.40f+shift*.02f,.44f+shift*.02f,.46f+shift*.02f));
        prim::boxFromTo(Vec3(wx+side*(10+shift*1.2f)-2,floor,wz-3-shift),Vec3(wx+side*(10+shift*1.2f)+2,floor+4+shift*1.7f,wz+3-shift));
    }
    for(int tier=0;tier<6;++tier) {
        float width=9-tier*.75f;
        prim::color(Color(.40f+tier*.017f,.43f+tier*.018f,.43f+tier*.024f));
        prim::boxFromTo(Vec3(wx-width,floor+tier*2.5f,wz-4),Vec3(wx+width,floor+(tier+1)*2.5f,wz+3-tier*.4f));
    }
    glDisable(GL_LIGHTING);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for(int strip=0;strip<7;++strip) {
        float x=wx-3.f+strip*.8f;
        prim::color(Color(.32f,.68f,.89f),.72f);
        prim::boxFromTo(Vec3(x,floor+.2f,wz+3.15f),Vec3(x+.65f,floor+15.1f,wz+3.3f));
        float y=floor+15-std::fmod(time*(3.2f+strip*.13f)+strip*2.1f,15.f);
        prim::color(Color(.86f,.94f,.91f),.75f);
        prim::boxFromTo(Vec3(x+.1f,y,wz+3.34f),Vec3(x+.35f,y+1.5f,wz+3.38f));
    }
    for(int bubble=0;bubble<12;++bubble) {
        float x=wx-4.3f+bubble*.74f, rise=.2f+std::sin(time*2.1f+bubble)*.12f;
        prim::color(Color(.84f,.94f,.93f),.70f);
        prim::boxFromTo(Vec3(x,floor+rise,wz+3),Vec3(x+.8f,floor+rise+.45f,wz+4.1f));
    }
    glPopAttrib();
}

void Terrain::drawShadows() const {
    glPushAttrib(GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT | GL_POLYGON_BIT);
    glDisable(GL_LIGHTING);
    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(-2.0f, -2.0f);
    glCallList(shadowList_);
    glPopAttrib();
}

} // namespace lf
