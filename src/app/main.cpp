// =============================================================================
//  Lake Fishing 3D - application entry point.
//
//  FreeGLUT owns the window and the event loop. We register callbacks:
//    display   -> render one frame (no game logic here)
//    timer     -> fixed ~60 Hz tick: measures elapsed time, updates the game
//                 (animation is time-based, independent of the frame rate)
//                 and requests a redraw
//    keyboard / special / mouse / motion / wheel -> input state + camera
//    reshape   -> new window size
// =============================================================================
#include "render/GLHeaders.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "ui/HUD.h"
#include "ui/Actions.h"
#include "ui/UIFont.h"
#include "ui/Artwork.h"
#include "game/Game.h"
#include "game/Input.h"
#include "game/MouseInput.h"
#include "app/DemoDirector.h"
#include "app/Diagnostics.h"
#include "core/FrameTime.h"
#include "core/Canvas.h"
#include "audio/SoundEffects.h"
#include "app/Platform.h"
#include "app/SaveStore.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>

using namespace lf;

namespace {

Game* g_game = nullptr;
Renderer* g_renderer = nullptr;
Camera g_camera;
HUD g_hud;
SoundEffects g_sfx;
InputState g_input;
InputSources g_sources;
std::unique_ptr<DemoDirector> g_demo;
std::unique_ptr<SaveStore> g_progress;

int g_width = 1280, g_height = 720;
int g_lastTimeMs = 0;
float g_fps = 60, g_fpsAccum = 0;
int g_fpsFrames = 0;
bool g_dragging = false,g_sfxDragging=false;
bool g_appliedFullscreen=false;
int g_cameraButton = -1;
MouseFishingInput g_mouse;

void updateMouseKeys(float dt);
void clearGameplayInput();
int g_lastMx = 0, g_lastMy = 0, g_downMx = 0, g_downMy = 0;
std::string g_pendingShot;

// ------------------------------------------------------------- callbacks ----
void display() {
    if (g_demo) {
        std::string shot = g_demo->takeShotRequest();
        if (!shot.empty()) g_pendingShot = shot;
    }
    g_renderer->render(*g_game, g_camera, g_width, g_height);
    g_hud.draw(*g_game, *g_renderer, g_camera, g_width, g_height, g_fps);

    if (!g_pendingShot.empty()) {
        std::string dir = g_demo ? g_demo->shotDir() : std::string("screenshots");
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        std::string path = dir + "/" + g_pendingShot + ".bmp";
        bool ok = diagnostics::saveScreenshotBMP(path, g_width, g_height);
        std::printf("%s %s\n", ok ? "Saved screenshot" : "FAILED to save screenshot", path.c_str());
        g_pendingShot.clear();
    }
    ++g_fpsFrames;
    glutSwapBuffers();
}

void reshape(int w, int h) {
    g_width = w > 1 ? w : 1;
    g_height = h > 1 ? h : 1;
    glViewport(0, 0, g_width, g_height);
}

void updateCameraInput(float dt) {
    if (!g_game->uiBlocked()) {
        if(int(g_camera.mode())!=g_game->preferences.camera) g_camera.setMode(CameraMode(g_game->preferences.camera));
        if (g_input.wasPressed('m')) g_camera.setMode(CameraMode::TopDown);
        if (g_input.wasPressed('v')) g_camera.cycleMode();
        for (int view = 0; view < 4; ++view)
            if (g_input.wasPressed((unsigned char)('1' + view))) g_camera.setMode(CameraMode(view));
        if (g_input.wasPressed('c')) g_camera.reset();
        if (g_input.isHeld('z')) g_camera.zoom(dt * 4.0f);
        if (g_input.isHeld('x')) g_camera.zoom(-dt * 4.0f);
        g_game->preferences.camera=int(g_camera.mode());

    }
}

void timer(int) {
    int now = glutGet(GLUT_ELAPSED_TIME);
    float dt = (now - g_lastTimeMs) / 1000.0f;
    g_lastTimeMs = now;
    if (dt < 0.0f) dt = 0.0f;
    g_fpsAccum += dt;
    if (g_fpsAccum >= 0.5f) { g_fps = g_fpsFrames / g_fpsAccum; g_fpsAccum = 0; g_fpsFrames = 0; }
    advanceFrame(dt, [&](float dt) {
        if (g_game->quitRequested || (g_demo && g_demo->finished())) return;
        if (g_demo) {
            g_demo->update(dt, *g_game, g_input, g_camera);
            if (g_demo->finished()) return;
        }

        if (!g_demo) {
            updateMouseKeys(dt);
            g_sources.mergeInto(g_input);
        }
        // Cursor: crosshair while fishing, arrow in menus.
        static int cursor = -1;
        int want = g_game->uiBlocked() ? GLUT_CURSOR_LEFT_ARROW : GLUT_CURSOR_CROSSHAIR;
        if (want != cursor) { glutSetCursor(want); cursor = want; }

        updateCameraInput(dt);
        if (g_input.wasPressed(KEY_F12)) {
            char name[64];
            std::snprintf(name, sizeof(name), "shot_%ld", long(std::time(nullptr)));
            g_pendingShot = name;
        }
        if (g_input.wasPressed(KEY_F11)) g_game->preferences.fullscreen=!g_game->preferences.fullscreen;
        if(g_appliedFullscreen!=g_game->preferences.fullscreen) { glutFullScreenToggle(); g_appliedFullscreen=g_game->preferences.fullscreen; }

        bool wasBlocked=g_game->uiBlocked();
        g_game->update(dt, g_input);
        g_sfx.update(g_game->takeSounds(),g_game->preferences.sfx);
        if(wasBlocked!=g_game->uiBlocked())clearGameplayInput();
        g_camera.update(dt, *g_game);
        g_input.endFrame();

    });
    if(g_progress){g_progress->poll(ProgressData::capture(*g_game),dt);g_game->saveStatus=g_progress->status();}
    if (g_game->quitRequested || (g_demo && g_demo->finished())) { glutLeaveMainLoop(); return; }
    // Scripted captures also render when the test window is hidden/minimised.
    if (g_demo) display(); else glutPostRedisplay();
    glutTimerFunc(settings::timerMs, timer, 0);
}

void keyboard(unsigned char key, int, int) {
    if (key == '\r' || key == '\n') key = KEY_ENTER;
    if ((key == '+' || key == '=') && !g_game->uiBlocked()) { g_camera.zoom(1); return; }
    if ((key == '-' || key == '_') && !g_game->uiBlocked()) { g_camera.zoom(-1); return; }
    if (!g_demo) g_sources.keyboard.keyDown(key);
}

void keyboardUp(unsigned char key, int, int) {
    if(key==0) { g_game->suspendControls(); clearGameplayInput(); g_cameraButton=-1; g_hud.pressed=-1; return; }
    if (key == 13 || key == 10) key = KEY_ENTER;
    if (!g_demo) g_sources.keyboard.keyUp(key);
}

unsigned char mapSpecial(int key) {
    switch (key) {
    case GLUT_KEY_UP: return KEY_UP;
    case GLUT_KEY_DOWN: return KEY_DOWN;
    case GLUT_KEY_LEFT: return KEY_LEFT;
    case GLUT_KEY_RIGHT: return KEY_RIGHT;
    case GLUT_KEY_F1: return KEY_F1;
    case GLUT_KEY_F5: return KEY_F5;
    case GLUT_KEY_F11: return KEY_F11;
    case GLUT_KEY_F12: return KEY_F12;
    default: return 0;
    }
}

void special(int key, int, int) {
    if (g_demo) return;
    unsigned char k = mapSpecial(key);
    if (k == KEY_F1) { g_sources.keyboard.keyDown('h'); return; }
    if (k) g_sources.keyboard.keyDown(k);
}

void specialUp(int key, int, int) {
    if (g_demo) return;
    unsigned char k = mapSpecial(key);
    if (k == KEY_F1) { g_sources.keyboard.keyUp('h'); return; }
    if (k) g_sources.keyboard.keyUp(k);
}

// ---------------------------------------------------------- mouse play ----
// Left mouse button is the main fishing button. Depending on the state it acts
// as SPACE (hold to charge, release to cast), E (click to hook) or R (hold to
// reel). These "virtual keys" are merged with the real keyboard each tick.
void updateMouseKeys(float dt) {
    g_mouse.update(dt, g_game->activeState(),
        g_demo || g_game->uiBlocked() || g_cameraButton >= 0, g_sources.mouse);
}

void clearGameplayInput() {
    g_input.releaseAll(); g_input.endFrame(); g_sources=InputSources();
    g_mouse=MouseFishingInput(); g_dragging=false;g_sfxDragging=false;
}
void activateSelectedEquipment(bool purchase) {
    const auto items=g_game->visibleItems();
    if(items.empty())return;
    const int selected=std::min(g_game->overlayIndex,int(items.size())-1);
    const int itemId=items[selected];
    if(purchase)g_game->buy(g_game->gearCategory,itemId,g_game->purchaseQuantity);
    else g_game->equip(g_game->gearCategory,itemId);
}

void activateUI(int action) {
    g_game->emitSound(SoundCue::Click);
    switch(action) {
    case ui::Close:case ui::CloseHelp:g_game->closeOverlay();break;
    case ui::Inventory:case ui::OpenInventory:case ui::TripInventory:
        g_game->openOverlay(Overlay::Inventory);break;
    case ui::Shop:case ui::OpenShop:case ui::TripShop:
        g_game->openOverlay(Overlay::Shop);break;
    case ui::Settings:g_game->openOverlay(Overlay::Settings);break;
    case ui::Pause:g_game->pause();break;
    case ui::Help:g_game->openHelp();break;
    case ui::Keep:case ui::Release:g_game->chooseCatch(action==ui::Keep);break;
    case ui::Buy:case ui::Equip:activateSelectedEquipment(action==ui::Buy);break;
    case ui::QuantityDown:g_game->adjustPurchaseQuantity(-1);break;
    case ui::QuantityUp:case ui::AddOne:g_game->adjustPurchaseQuantity(1);break;
    case ui::AddFive:g_game->adjustPurchaseQuantity(5);break;
    case ui::AddTen:g_game->adjustPurchaseQuantity(10);break;
    case ui::AcceptOrder:g_game->acceptOrder();break;
    case ui::EndDay:g_game->finishDay(DayEnd::Early);break;
    case ui::DifficultyDown:
        g_game->setDifficulty(Difficulty((int(g_game->difficulty)+2)%3));break;
    case ui::DifficultyUp:
        g_game->setDifficulty(Difficulty((int(g_game->difficulty)+1)%3));break;
    case ui::Start:g_game->requestStart();break;
    case ui::AcceptTrip:case ui::SkipTrip:g_game->confirmStart(action==ui::AcceptTrip);break;
    case ui::SecretFish:g_game->clickCreditsFish();break;
    case ui::CreatorProfile:platform::openCreatorProfile();break;
    case ui::PreviousPage:case ui::NextPage: {
        const auto items=g_game->visibleItems();
        if(!items.empty())g_game->overlayIndex=std::min(int(items.size())-1,action==ui::PreviousPage?0:4);
        break;
    }
    default:
        if(action>=ui::Menu && action<ui::MenuEnd) {
            g_game->menuIndex=action-ui::Menu;
            g_game->activateMenuItem(g_game->menuIndex);
        } else if(action>=ui::SettingDecrease && action<ui::SettingEnd) {
            const int row=(action-ui::SettingDecrease)/2;
            const int direction=(action-ui::SettingDecrease)%2?1:-1;
            g_game->overlayIndex=row;
            g_game->adjustSetting(row,direction);
        } else if(action>=ui::Category && action<ui::CategoryEnd) {
            g_game->selectCategory(action-ui::Category);
        } else if(action>=ui::Item && action<ui::ItemEnd) {
            const auto items=g_game->visibleItems();
            const auto item=std::find(items.begin(),items.end(),action-ui::Item);
            if(item!=items.end())g_game->overlayIndex=int(item-items.begin());
        } else if(action>=ui::Location && action<ui::LocationEnd) {
            const auto map=MapId(action-ui::Location);
            if(!g_game->selectMap(map)) {
                g_game->openOverlay(Overlay::Shop);
                g_game->selectCategory(5);
                g_game->overlayIndex=int(map);
            }
        } else if(action>=ui::BuyLocation && action<ui::BuyLocationEnd) {
            g_game->buyMap(MapId(action-ui::BuyLocation));
        } else if(action>=ui::Mode && action<ui::ModeEnd) {
            g_game->selectedMode=RoundMode(action-ui::Mode);
        }
        break;
    }
    // A UI click must never become the next cast or hook press.
    clearGameplayInput();
}

void dragSfx(int x) {
    Canvas canvas(g_width,g_height);float px,py;
    canvas.point(float(x),0,px,py);
    g_game->preferences.sfx=clampf((px-soundSlider.x)/soundSlider.w,0,1);
}
bool insideCanvas(int x,int y) {
    return Canvas(g_width,g_height).contains(float(x),float(y));
}
void mouse(int button, int state, int x, int y) {
    bool ui=g_game->uiBlocked();
    if ((button==3 || button==4) && state==GLUT_DOWN) {if(!ui && insideCanvas(x,y))g_camera.zoom(button==3?1.f:-1.f);return;}
    if(button==GLUT_LEFT_BUTTON) {
        if(g_demo)return;
        if(state==GLUT_DOWN) {
            g_downMx=x;g_downMy=y;
            g_cameraButton=g_hud.actionAt(*g_game,x,y,g_width,g_height);
            g_hud.pressed=g_cameraButton;
            if(g_cameraButton==ui::SoundSlider){g_sfxDragging=true;dragSfx(x);}
            g_mouse.button(true,g_game->activeState(),ui || !insideCanvas(x,y) || g_cameraButton>=0,g_sources.mouse);
        } else {
            g_mouse.button(false,g_game->activeState(),ui,g_sources.mouse);
            int action=g_hud.actionAt(*g_game,x,y,g_width,g_height);
            if(g_sfxDragging){dragSfx(x);g_sfxDragging=false;}
            bool click=std::abs(x-g_downMx)<6 && std::abs(y-g_downMy)<6;
            if(click && action>=0 && action==g_cameraButton)activateUI(action);
            g_cameraButton=-1;g_hud.pressed=-1;
        }
        return;
    }
    if(!ui && (button==GLUT_RIGHT_BUTTON || button==GLUT_MIDDLE_BUTTON)) {
        if(state==GLUT_DOWN && !insideCanvas(x,y))return;
        g_dragging=state==GLUT_DOWN;g_lastMx=x;g_lastMy=y;
    }
}

void trackMouse(int x, int y) {
    if(!insideCanvas(x,y))return;
    Canvas canvas(g_width,g_height);float px,py;
    canvas.point(float(x),float(y),px,py);
    g_input.mouseX=clampf(2*px/Canvas::width-1,-1,1);
    g_input.mouseY=clampf(2*py/Canvas::height-1,-1,1);
    if (!g_demo && !g_game->uiBlocked()) g_input.mouseAim = true;
}

void motion(int x, int y) {
    if(g_sfxDragging){dragSfx(x);return;}
    if (g_dragging && !g_game->uiBlocked()) g_camera.orbit(float(x - g_lastMx)*g_game->preferences.sensitivity, float(y - g_lastMy)*g_game->preferences.sensitivity);
    else trackMouse(x, y);
    g_lastMx = x;
    g_lastMy = y;
}

void passiveMotion(int x, int y) {
    g_hud.hovered=g_hud.inspectAt(*g_game,x,y,g_width,g_height);
    if (g_hud.actionAt(*g_game, x, y, g_width, g_height) < 0) trackMouse(x, y);
    int item = g_hud.menuItemAt(*g_game, x, y, g_width, g_height);
    if (item >= 0) g_game->menuIndex = item;
}

void mouseWheel(int, int dir, int x, int y) { if(!g_game->uiBlocked() && insideCanvas(x,y))g_camera.zoom(dir > 0 ? 1.0f : -1.0f); }


void printUsage() {
    std::printf(
        "Lake Fishing 3D\n"
        "  --benchmark         measure a fixed scene render cost\n"
        "  --check-input       verify application callbacks and scaled menu hits\n"
        "  --demo              the computer plays the game (endless)\n"
        "  --shots <folder>    scripted demo that saves screenshots + log, then exits\n"
        "  --size <w> <h>      window size (default 1280 720)\n"
        "  --fullscreen        start in fullscreen\n");
}

} // namespace

int main(int argc, char** argv) {
    bool demo = false, fullscreen = false, inputCheck = false, benchmark = false;
    std::string shotDir;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--benchmark") benchmark=true;
        else if (a == "--check-input") inputCheck = true;
        else if (a == "--demo") demo = true;
        else if (a == "--shots" && i + 1 < argc) { shotDir = argv[++i]; demo = true; }
        else if (a == "--size" && i + 2 < argc) { g_width = std::atoi(argv[i + 1]); g_height = std::atoi(argv[i + 2]); i += 2; }
        else if (a == "--fullscreen") fullscreen = true;
        else if (a == "--help" || a == "-h") { printUsage(); return 0; }
    }

    glutInit(&argc, argv);
    glutSetOption(GLUT_MULTISAMPLE, 4);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitWindowSize(g_width, g_height);
    glutInitWindowPosition(60, 40);
    glutCreateWindow("FISH LACK 3D");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glutIgnoreKeyRepeat(1);   // we track held keys ourselves

    std::printf("OpenGL vendor:   %s\nOpenGL renderer: %s\nOpenGL version:  %s\n",
                (const char*)glGetString(GL_VENDOR), (const char*)glGetString(GL_RENDERER),
                (const char*)glGetString(GL_VERSION));

    Game game;
    const auto executableDirectory=std::filesystem::absolute(argv[0]).parent_path();
    if(!demo && !inputCheck && !benchmark){
        g_progress=std::make_unique<SaveStore>(executableDirectory/"progress.save");
        ProgressData profile;g_progress->load(profile);profile.apply(game);
        game.setMenuMap(profile.advanceMenuMap());
        g_progress->save(ProgressData::capture(game),true);game.saveStatus=g_progress->status();
    }
    if(fullscreen)game.preferences.fullscreen=true;
    if(game.preferences.fullscreen)glutFullScreen();
    Renderer renderer;
    g_game = &game;
    g_renderer = &renderer;
    int t0 = glutGet(GLUT_ELAPSED_TIME);
    renderer.init();
    auto assets = executableDirectory / "assets";
    uifont::init((assets / "fonts").string());
    uiart::init((assets / "art").string());
    std::printf("Scene built in %d ms\n", glutGet(GLUT_ELAPSED_TIME) - t0);

    g_camera.setMode(CameraMode(game.preferences.camera));
    g_appliedFullscreen=game.preferences.fullscreen;
    g_camera.update(0,game);
    diagnostics::Context diagnosticsContext{g_game,g_renderer,g_camera,g_hud,g_input,g_sources,
        g_width,g_height,bool(g_progress),mouse,keyboard,keyboardUp,updateMouseKeys,updateCameraInput,clearGameplayInput};
    if(benchmark) {int result=0;for(int map=0;map<3;++map){g_game->session.maps[map]=true;g_game->selectedMap=MapId(map);result|=diagnostics::benchmarkRender(diagnosticsContext);}glutDestroyWindow(glutGetWindow());return result;}
    if (inputCheck) {
        int result = diagnostics::checkInputCallbacks(diagnosticsContext);
        glutDestroyWindow(glutGetWindow());
        return result;
    }

    g_sfx.init((assets / "sfx").string());
    if (demo) {
        if (!shotDir.empty()) {
            std::error_code ec;
            std::filesystem::create_directories(shotDir, ec);
        }
        g_demo = std::make_unique<DemoDirector>(shotDir.empty() ? std::string("screenshots") : shotDir, !shotDir.empty());
    }

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutKeyboardUpFunc(keyboardUp);
    glutSpecialFunc(special);
    glutSpecialUpFunc(specialUp);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutPassiveMotionFunc(passiveMotion);
    glutMouseWheelFunc(mouseWheel);

    g_lastTimeMs = glutGet(GLUT_ELAPSED_TIME);
    glutTimerFunc(settings::timerMs, timer, 0);
    glutMainLoop();

    if(g_progress){if(game.ledger.active)game.finishDay(DayEnd::Early);g_progress->save(ProgressData::capture(game),true);}
    g_demo.reset();
    std::printf("Bye!\n");
    return 0;
}
