#include "app/Diagnostics.h"
#include "render/GLHeaders.h"
#include "render/Renderer.h"
#include "render/Camera.h"
#include "ui/HUD.h"
#include "game/Game.h"
#include "game/AutoPlayer.h"
#include "core/Settings.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <vector>
namespace lf::diagnostics {
// ---------------------------------------------------------- screenshots ----
bool saveScreenshotBMP(const std::string& path, int w, int h) {
    std::vector<unsigned char> px(size_t(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    int rowSize = (w * 3 + 3) & ~3;
    unsigned int dataSize = rowSize * h, fileSize = 54 + dataSize;
    unsigned char hdr[54] = {'B', 'M'};
    auto put32 = [&](int off, unsigned int v) { for (int i = 0; i < 4; ++i) hdr[off + i] = (v >> (8 * i)) & 0xFF; };
    put32(2, fileSize); put32(10, 54); put32(14, 40); put32(18, w); put32(22, h);
    hdr[26] = 1; hdr[28] = 24; put32(34, dataSize);
    std::fwrite(hdr,1,54,f);
    std::vector<unsigned char> row(rowSize, 0);
    for (int y = 0; y < h; ++y) { // BMP rows are bottom-up like glReadPixels
        for (int x = 0; x < w; ++x) {
            const unsigned char* p = &px[(size_t(y) * w + x) * 3];
            row[x * 3 + 0] = p[2]; row[x * 3 + 1] = p[1]; row[x * 3 + 2] = p[0];
        }
        std::fwrite(row.data(),1,rowSize,f);
    }
    std::fclose(f);
    return true;
}

// Exercise the actual application callbacks, without injecting OS input.
// Useful when a desktop automation helper cannot reliably focus a game window.
int checkInputCallbacks(Context& c) {
    int failures=0,checks=0;std::error_code error;std::filesystem::create_directories("qa_shots_maps",error);
    auto prefix="qa_shots_maps/"+std::to_string(c.width)+"x"+std::to_string(c.height);
    auto check=[&](bool ok,const char* msg){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",msg);}};
    auto snapshot=[&](const char* name){for(int i=0;i<120;++i)c.camera.update(.016f,*c.game);c.renderer->render(*c.game,c.camera,c.width,c.height);c.hud.draw(*c.game,*c.renderer,c.camera,c.width,c.height,60);return saveScreenshotBMP(prefix+name+".bmp",c.width,c.height);};
    auto click=[&](int action){int x=0,y=0;if(!c.hud.actionCenter(*c.game,action,c.width,c.height,x,y))return false;c.mouse(GLUT_LEFT_BUTTON,GLUT_DOWN,x,y);c.mouse(GLUT_LEFT_BUTTON,GLUT_UP,x,y);return true;};
    auto absent=[&](int action){int x=0,y=0;return !c.hud.actionCenter(*c.game,action,c.width,c.height,x,y);};
    auto tick=[&]{c.updateMouseKeys(.016f);c.sources.mergeInto(c.input);c.updateCameraInput(.016f);c.game->update(.016f,c.input);c.camera.update(.016f,*c.game);c.input.endFrame();};
    check(!c.hasProgress,"QA does not load or write the player save");
    for(int map=0;map<3;++map){c.game->setMenuMap(MapId(map));check(!c.game->session.maps[1] && !c.game->session.maps[2],"backdrop leaves locked maps locked");check(snapshot(("_menu_map"+std::to_string(map)).c_str()),"rotating menu map screenshot");check(glGetError()==GL_NO_ERROR,"menu map renders cleanly");}
    c.game->setMenuMap(MapId::Forest);
    c.game->openOverlay(Overlay::Shop);
    for(int category=0;category<GEAR_CATEGORIES;++category){c.game->selectCategory(category);
        for(int id=0;id<gearCount(GearCategory(category));++id){c.game->overlayIndex=id;
            c.renderer->render(*c.game,c.camera,c.width,c.height);c.hud.draw(*c.game,*c.renderer,c.camera,c.width,c.height,60);
            float scale=std::min(c.width/1600.f,c.height/900.f),ox=(c.width-1600*scale)/2,oy=(c.height-900*scale)/2;
            int px=int(ox+1092*scale),top=int(oy+392*scale),w=std::max(1,int(56*scale)),h=std::max(1,int(48*scale));std::vector<unsigned char> pixels(w*h*3);
            glReadBuffer(GL_BACK);glReadPixels(px,c.height-top-h,w,h,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());int ink=0;
            for(size_t n=0;n<pixels.size();n+=3){int sample=int(n/3);if(sample%w>w*7/10 && sample/w<h/3)continue;ink+=(pixels[n]*.2126f+pixels[n+1]*.7152f+pixels[n+2]*.0722f)<160;}
            if(ink<=std::max(3,w*h/200)){std::printf("Icon coverage: category=%d id=%d ink=%d\n",category,id,ink);snapshot(("_icon_failure_"+std::to_string(category)+"_"+std::to_string(id)).c_str());}
            check(ink>std::max(3,w*h/200),"equipment detail has visible central geometry, not only a badge");
            if(id==0 || id==4)check(snapshot(("_catalog_"+std::to_string(category)+"_page"+std::to_string(id/4+1)).c_str()),"catalog page screenshot");
        }
    }
    c.game->closeOverlay();c.game->saveStatus="Saved"; // Display-only fixture; no SaveStore is attached.
    check(snapshot("_menu"),"main menu screenshot");check(click(104) && c.game->overlay==Overlay::Credits,"Credits opens");check(snapshot("_credits"),"game title and official GitHub mark screenshot");check(!absent(921),"GitHub profile is clickable");for(int i=0;i<7;++i)click(920);check(c.game->overlay==Overlay::Secret && c.game->session.coins==1000000,"seven real clicks reveal million secret");check(snapshot("_secret"),"Arabic secret screenshot");click(99);c.game->session.coins=100;click(99);
    check(click(100) && c.game->overlay==Overlay::TripSetup,"trip preparation");check(snapshot("_trip_locked"),"locked location cards screenshot");
    check(click(861) && c.game->overlay==Overlay::Shop && c.game->locationsTab,"locked location opens Shop Locations");click(871);check(!c.game->session.maps[1] && c.game->session.coins==100,"unaffordable location cannot charge");
    c.game->session.coins=2000;check(click(871) && c.game->session.maps[1] && c.game->session.coins==1650,"buy Highland at exact price");click(871);check(c.game->session.coins==1650,"map buy disabled once owned");check(click(872) && c.game->session.maps[2] && c.game->session.coins==1050,"buy Marsh");check(snapshot("_locations"),"Locations screenshot");
    click(701);click(811);click(834);click(834);check(c.game->purchaseQuantity==21,"repeated +10 adds to selected quantity");check(click(820) && c.game->session.baitCount[1]==21,"batch purchase uses accumulated quantity");check(snapshot("_shop"),"shop screenshot without notification");check(click(851),"next shop page");check(snapshot("_shop_page2"),"new bait icons screenshot");check(!absent(817),"eighth item has a working click target");click(99);
    check(c.game->overlay==Overlay::TripSetup,"location Shop returns to trip");check(click(861) && c.game->selectedMap==MapId::Highland && world::activeMap()==MapId::Forest,"selection does not switch active world before start");check(snapshot("_trip"),"unlocked trip screenshot");click(860);
    click(906);check(absent(711) && absent(710),"Inventory contains neither Shop nor redundant self button");check(absent(705),"Day Catch tab removed");check(snapshot("_inventory"),"inventory screenshot");click(99);check(click(905) && c.game->overlay==Overlay::DailyOrder,"start offers order");check(snapshot("_order"),"pre-day order screenshot");check(click(908) && c.game->ledger.active,"accept and start Forest day");
    check(absent(401),"locked Shop icon removed from gameplay");c.keyboard('u',0,0);tick();c.keyboardUp('u',0,0);tick();check(c.game->overlay==Overlay::None,"shop shortcut still blocked during day");
    c.keyboard(KEY_SPACE,0,0);tick();for(int i=0;i<35;++i)tick();check(snapshot("_charge"),"power side card screenshot");c.keyboardUp(KEY_SPACE,0,0);tick();for(int i=0;i<25;++i)tick();check(c.game->ledger.baitUsed==1,"bait charged once at release");
    click(400);float frozen=c.game->timeLeft;tick();check(c.game->timeLeft==frozen && absent(711),"inventory freezes time and has no Shop");click(99);click(402);check(absent(408) && absent(407) && absent(510),"settings omits help, back and extra rows");check(snapshot("_settings"),"five-setting screenshot");click(99);check(click(403) && c.game->state==GameState::Paused,"door pauses");check(snapshot("_paused"),"compact pause screenshot");check(c.game->menuItemLabel(1)=="How to Play" && click(101) && c.game->showHelp,"pause help opens");check(snapshot("_help"),"updated camera help screenshot");click(99);click(100);
    AutoPlayer bot(83);InputState input;bool fightCaptured=false;
    for(int i=0;i<18000 && !c.game->catchPending && !c.game->summary.valid;++i){bot.drive(*c.game,input,1.f/60);c.game->update(1.f/60,input);input.endFrame();if(!fightCaptured && c.game->state==GameState::Reeling){check(snapshot("_fight_quality"),"quality inside tension card screenshot");fightCaptured=true;}}
    check(c.game->catchPending,"real fish reaches choice");c.game->lastCatchQuality=60;c.game->lastCatchSale=fishSaleValue(c.game->lastCatchType)*60/100;
    check(snapshot("_catch_quality"),"catch quality grade and actual price screenshot");check(click(600),"keep fish");c.game->ledger.keep(0,40);c.game->ledger.keep(0,90);c.game->caughtByType[0]+=2;c.game->caught+=2;c.game->finishDay(DayEnd::Early);c.game->stateTimer=1.3f;
    check(c.game->summary.qualityAverage[0]>=40 && c.game->summary.saleTotals[0]>0,"receipt aggregates individual quality prices");check(c.game->menuItemCount()==2 && absent(102) && absent(103),"results has only Next Day and Main Menu");check(snapshot("_results"),"two-button quality receipt screenshot");click(100);check(c.game->overlay==Overlay::TripSetup,"Next Day returns to trip");click(99);click(101);check(c.game->state==GameState::MainMenu,"results Main Menu action");
    for(int map=0;map<3;++map){
        c.game->selectMap(MapId(map));c.game->startNewGame();c.clearGameplayInput();c.camera.reset();
        for(int view=0;view<4;++view){c.keyboard('1'+view,0,0);tick();c.keyboardUp('1'+view,0,0);tick();check(snapshot(("_map"+std::to_string(map)+"_view"+std::to_string(view)).c_str()),"map/camera screenshot");check(glGetError()==GL_NO_ERROR,"map scene has no OpenGL errors");}
        c.camera.setMode(CameraMode::FirstPerson);c.game->preferences.camera=0;
        for(float yaw:{-.5f,0.f,.5f})for(float power:{0.f,1.f}){
            c.game->fishing.reset();FishingControls aim;aim.left=yaw>0;aim.right=yaw<0;c.game->fishing.updateAim(std::fabs(yaw)/.9f,aim);c.game->fishing.beginCharge();c.game->fishing.updateCharge(power/.85f);snapshot("_aim_check");float x=0,y=0;bool projected=c.renderer->project(c.game->fishing.predictedLanding(),x,y);
            float scale=std::min(c.width/1600.f,c.height/900.f),oy=(c.height-900*scale)/2;
            check(projected && x/scale>24 && x/scale<1576 && (y-oy)/scale>112 && (y-oy)/scale<740,"map target visible at aim and power extremes");
        }
        unsigned builds=c.renderer->worldRebuilds();snapshot("_stable_map");check(c.renderer->worldRebuilds()==builds,"same map reuses cached geometry");
        c.game->timeLeft=12;check(snapshot(("_map"+std::to_string(map)+"_sunset").c_str()),"map sunset screenshot");c.game->finishDay(DayEnd::Early);
    }
    for(int cycle=0;cycle<6;++cycle){c.game->selectMap(MapId(cycle%3));c.game->startNewGame();snapshot("_switch_stress");check(glGetError()==GL_NO_ERROR,"repeated map switch rebuilds cleanly");c.game->finishDay(DayEnd::Early);}
    Game fixture;Game* prior=c.game;c.game=&fixture;fixture.startNewGame();fixture.session.baitCount[0]=0;fixture.checkSupplies();check(snapshot("_no_bait"),"out of bait popup screenshot");click(99);check(snapshot("_empty_receipt"),"zero catch receipt screenshot");fixture.startNewGame();fixture.lastCatchType=FISH_CARP;fixture.lastCatchQuality=88;fixture.lastCatchSale=44;fixture.lastCatchSuspicious=true;fixture.lastCatchToxic=true;fixture.catchPending=true;fixture.state=GameState::FishCaught;fixture.caughtByType[2]=1;fixture.caught=1;check(snapshot("_suspicious"),"warning before Keep Release screenshot");fixture.chooseCatch(true);fixture.finishDay(DayEnd::Early);fixture.stateTimer=1.3f;check(snapshot("_toxic_receipt"),"negative income receipt screenshot");fixture.startNewGame();fixture.ledger.orderAccepted=true;fixture.ledger.order={FISH_CARP,2,18};
    for(int type=0;type<FISH_TYPE_COUNT;++type){fixture.caughtByType[type]=3;fixture.caught+=3;fixture.ledger.keep(type,80);fixture.ledger.keep(type,90,1,true);fixture.ledger.released[type]=1;}
    fixture.finishDay(DayEnd::Early);fixture.stateTimer=1.3f;check(snapshot("_six_species"),"six-species receipt and incomplete order screenshot");
    check(!fixture.summary.order.complete() && fixture.summary.order.keptHealthy==1,"incomplete order snapshot includes healthy kept count");
    fixture.ledger.orderAccepted=false;fixture.ledger.kept.clear();check(fixture.summary.order.accepted && fixture.summary.order.keptHealthy==1,"receipt ignores later ledger changes");
    fixture.goToMainMenu();check(snapshot("_exit_contrast"),"Exit icon contrast screenshot");fixture.session.coins=1000000;fixture.openOverlay(Overlay::Inventory);check(snapshot("_million_inventory"),"million-wallet alignment screenshot");c.game=prior;
    Game fresh;Game* tested=c.game;c.game=&fresh;fresh.startNewGame();c.camera.setMode(CameraMode::FirstPerson);check(snapshot("_fresh_gameplay"),"clean 100-coin starter gameplay");c.game=tested;
    std::FILE* log=std::fopen((prefix+".txt").c_str(),"w");if(log){std::fprintf(log,"%d checks, %d failures\n",checks,failures);std::fclose(log);}std::printf("Application callbacks: %d checks, %d failures\n",checks,failures);return failures?1:0;
}

// Deterministic rendering benchmark: no simulation, file capture or buffer swap in the timed loop.
int benchmarkRender(Context& c) {
    c.game->rng.seed(20261126);c.game->startNewGame();
    c.camera.setMode(CameraMode::FirstPerson);c.camera.reset();
    for(int i=0;i<180;++i)c.camera.update(settings::maxStep,*c.game);
    auto render=[&] {c.renderer->render(*c.game,c.camera,c.width,c.height);c.hud.draw(*c.game,*c.renderer,c.camera,c.width,c.height,0);glFinish();};
    for(int i=0;i<20;++i)render();
    std::vector<double> batches;
    for(int batch=0;batch<5;++batch) {
        auto start=std::chrono::steady_clock::now();
        for(int frame=0;frame<40;++frame)render();
        batches.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/40);
    }
    std::sort(batches.begin(),batches.end());
    std::error_code ec;std::filesystem::create_directories("qa_benchmark",ec);
    auto path="qa_benchmark/map"+std::to_string(int(c.game->roundMap))+"_"+std::to_string(c.width)+"x"+std::to_string(c.height)+".txt";
    if(FILE* file=std::fopen(path.c_str(),"w")) {
        std::fprintf(file,"Map: %s\nRenderer: %s\nResolution: %dx%d\nMedian render ms: %.3f\nBatches ms: ",world::mapSpec(c.game->roundMap).name,(const char*)glGetString(GL_RENDERER),c.width,c.height,batches[2]);
        for(double ms:batches)std::fprintf(file,"%.3f ",ms);
        std::fprintf(file,"\nScene: fixed initial aiming, 20 warmup + 5 x 40 frames; glFinish, no swap\n");std::fclose(file);
    }
    return 0;
}

} // namespace lf::diagnostics
