#pragma once
#include "game/Types.h"
#include "core/World.h"
#include <array>
#include <vector>
#include <algorithm>
#include <iterator>
namespace lf {
enum class Overlay { None, Inventory, Shop, Settings, TripSetup, Credits, DailyOrder, NoBait, Secret };
enum class GearCategory { Rod, Bait, Reel, Line, Float };
constexpr int GEAR_CATEGORIES=5;
inline const char* categoryName(GearCategory c) {
    const char* names[]={"Rods","Baits","Reels","Lines","Floats"};return names[int(c)];
}
constexpr int ITEMS_PER_CATEGORY=8;
struct LoadoutStats {
    float tension=1,search=1,reel=1,maxLine=46,bite=1,searchRadius=1,qualityLoss=1;
    int preferredA=-1,preferredB=-1;
};
struct Equipment {const char* name;int price;LoadoutStats effects;const char* detail;};
inline Equipment rodItem(const char* name,int price,float tension,const char* detail,float quality=1) {
    LoadoutStats effect;effect.tension=tension;effect.qualityLoss=quality;return {name,price,effect,detail};
}
inline Equipment baitItem(const char* name,int price,float search,const char* detail,float radius=1,int a=-1,int b=-1) {
    LoadoutStats effect;effect.search=search;effect.searchRadius=radius;effect.preferredA=a;effect.preferredB=b;return {name,price,effect,detail};
}
inline Equipment reelItem(const char* name,int price,float speed,const char* detail,float tension=1) {
    LoadoutStats effect;effect.reel=speed;effect.tension=tension;return {name,price,effect,detail};
}
inline Equipment lineItem(const char* name,int price,float metres,const char* detail,float tension=1,float radius=1) {
    LoadoutStats effect;effect.maxLine=metres;effect.tension=tension;effect.searchRadius=radius;return {name,price,effect,detail};
}
inline Equipment floatItem(const char* name,int price,float bite,const char* detail,float quality=1,float radius=1) {
    LoadoutStats effect;effect.bite=bite;effect.qualityLoss=quality;effect.searchRadius=radius;return {name,price,effect,detail};
}
inline const Equipment RODS[]={
    rodItem("Wooden",0,1,"Standard tension rise."),rodItem("Fiberglass",200,.95f,"5% less tension rise."),
    rodItem("Iron",400,.9f,"10% less tension rise."),rodItem("Golden",1200,.8f,"20% less tension rise."),
    rodItem("River",300,.97f,"3% less tension. Protects quality by 20%.",.8f),
    rodItem("Carbon",650,.86f,"14% less tension rise."),
    rodItem("Careful",800,.95f,"5% less tension. Protects quality by 40%.",.6f),
    rodItem("Guardian",1400,.82f,"18% less tension. Protects quality by 25%.",.75f)};
inline const Equipment BAITS[]={
    baitItem("Worm",2,1,"Standard search. All species."),baitItem("Insect",4,.9f,"10% quicker search. Bluefish / Perch.",1,FISH_BLUE,FISH_PERCH),
    baitItem("Dough",5,.85f,"15% quicker search. Carp / Catfish.",1,FISH_CARP,FISH_CATFISH),baitItem("Minnow",8,.8f,"20% quicker search. Trout / Pike.",1,FISH_TROUT,FISH_PIKE),
    baitItem("Bread",3,.95f,"5% quicker, 15% wider search. Carp / Perch.",1.15f,FISH_CARP,FISH_PERCH),
    baitItem("Shrimp",6,.9f,"10% quicker, 10% wider search. Catfish / Bluefish.",1.1f,FISH_CATFISH,FISH_BLUE),
    baitItem("Spinner",9,.75f,"25% quicker, 15% smaller search. Trout / Pike.",.85f,FISH_TROUT,FISH_PIKE),
    baitItem("Glow Grub",7,1.05f,"5% slower, 35% wider search. Perch / Trout.",1.35f,FISH_PERCH,FISH_TROUT)};
inline const Equipment REELS[]={
    reelItem("Basic",0,1,"Standard reel speed."),reelItem("Spinning",250,1.12f,"12% faster reeling."),reelItem("Sport",600,1.25f,"25% faster reeling."),reelItem("Precision",900,1.4f,"40% faster reeling."),
    reelItem("Compact",180,1.06f,"6% faster reeling. 3% less tension.",.97f),reelItem("Drag",500,1.08f,"8% faster reeling. 10% less tension.",.9f),
    reelItem("Power",800,1.3f,"30% faster reeling. 5% more tension.",1.05f),reelItem("Tournament",1100,1.55f,"55% faster reeling. 12% more tension.",1.12f)};
inline const Equipment LINES[]={lineItem("Standard",0,46,"46 metres available."),lineItem("Long",300,56,"56 metres available."),lineItem("Reinforced",450,46,"46 m / 5% less tension.",.95f,1),lineItem("Braided",850,56,"56 m / 10% less tension.",.9f,1),
    lineItem("Light",180,38,"38 m. 10% less tension.",.9f,1),lineItem("Fluorocarbon",500,46,"46 m. 15% wider fish search.",1,1.15f),
    lineItem("Elastic",650,44,"44 m. 12% less tension.",.88f,1),lineItem("Deep Spool",1000,64,"64 m. 5% more tension.",1.05f,1)};
inline const Equipment FLOATS[]={floatItem("Classic",0,1,"Standard hook window."),floatItem("Sensitive",180,1.15f,"15% longer hook window."),floatItem("Glow",280,1.15f,"15% longer window. Sunset glow."),floatItem("Pro",650,1.3f,"30% longer hook window."),
    floatItem("Feather",130,1.08f,"8% longer hook window."),floatItem("Steady",350,1.1f,"10% longer window. Protects quality by 15%.",.85f,1),
    floatItem("Pulse",450,1.25f,"25% longer window. 10% smaller search.",1,.9f),floatItem("Scout",500,1.05f,"5% longer window. 20% wider search.",1,1.2f)};
inline int gearCount(GearCategory c) {
    switch(c){
    case GearCategory::Rod:return int(std::size(RODS));
    case GearCategory::Bait:return int(std::size(BAITS));
    case GearCategory::Reel:return int(std::size(REELS));
    case GearCategory::Line:return int(std::size(LINES));
    case GearCategory::Float:return int(std::size(FLOATS));
    }
    return 0;
}
static_assert(std::size(RODS)==ITEMS_PER_CATEGORY && std::size(BAITS)==ITEMS_PER_CATEGORY &&
    std::size(REELS)==ITEMS_PER_CATEGORY && std::size(LINES)==ITEMS_PER_CATEGORY && std::size(FLOATS)==ITEMS_PER_CATEGORY);
inline const Equipment& gearSpec(GearCategory c,int id) {
    const Equipment* lists[]={RODS,BAITS,REELS,LINES,FLOATS};
    return lists[int(c)][id>=0 && id<gearCount(c)?id:0];
}
struct Session {
    int coins=100,daysStarted=0;
    int baitCount[ITEMS_PER_CATEGORY]={15};
    bool secretClaimed=false;
    bool nextOrderAccepted=false;
    bool maps[3]={true,false,false};
    int rod=0,bait=0,reel=0,line=0,bobber=0;
    bool rods[ITEMS_PER_CATEGORY]={true};
    bool reels[ITEMS_PER_CATEGORY]={true},lines[ITEMS_PER_CATEGORY]={true},floats[ITEMS_PER_CATEGORY]={true};
    bool* ownership(GearCategory category) {
        switch(category) {
        case GearCategory::Rod:return rods;
        case GearCategory::Bait:return nullptr; // Consumables use baitCount instead.
        case GearCategory::Reel:return reels;
        case GearCategory::Line:return lines;
        case GearCategory::Float:return floats;
        }
        return rods;
    }
    bool owns(GearCategory category,int itemId) const {
        if(itemId<0 || itemId>=gearCount(category))return false;
        if(category==GearCategory::Bait)return baitCount[itemId]>0;
        const bool* ownedItems[]={rods,nullptr,reels,lines,floats};
        return ownedItems[int(category)][itemId];
    }
    int& equipped(GearCategory category) {
        switch(category) {
        case GearCategory::Rod:return rod;
        case GearCategory::Bait:return bait;
        case GearCategory::Reel:return reel;
        case GearCategory::Line:return line;
        case GearCategory::Float:return bobber;
        }
        return rod;
    }
    int equipped(GearCategory category) const {
        const int equippedItems[]={rod,bait,reel,line,bobber};
        return equippedItems[int(category)];
    }
    bool purchase(GearCategory category,int itemId,int quantity=1) {
        constexpr int maxPurchaseQuantity=99;
        if(itemId<0 || itemId>=gearCount(category) || quantity<1 || quantity>maxPurchaseQuantity)return false;
        const bool consumable=category==GearCategory::Bait;
        if(!consumable && owns(category,itemId))return false;
        const int cost=gearSpec(category,itemId).price*(consumable?quantity:1);
        if(coins<cost)return false;

        coins-=cost;
        if(consumable)baitCount[itemId]+=quantity;
        else ownership(category)[itemId]=true;
        return true;
    }
    LoadoutStats stats() const {
        const auto& rodEffects=gearSpec(GearCategory::Rod,rod).effects;
        const auto& baitEffects=gearSpec(GearCategory::Bait,bait).effects;
        const auto& reelEffects=gearSpec(GearCategory::Reel,reel).effects;
        const auto& lineEffects=gearSpec(GearCategory::Line,line).effects;
        const auto& floatEffects=gearSpec(GearCategory::Float,bobber).effects;
        LoadoutStats combined;
        combined.tension=rodEffects.tension*lineEffects.tension*reelEffects.tension;
        combined.search=baitEffects.search;
        combined.reel=reelEffects.reel;
        combined.maxLine=lineEffects.maxLine;
        combined.bite=floatEffects.bite;
        combined.searchRadius=baitEffects.searchRadius*lineEffects.searchRadius*floatEffects.searchRadius;
        combined.qualityLoss=rodEffects.qualityLoss*floatEffects.qualityLoss;
        combined.preferredA=baitEffects.preferredA;
        combined.preferredB=baitEffects.preferredB;
        return combined;
    }
};
struct DailyOrder {int type,quantity,reward;};
inline DailyOrder dailyOrder(int day) {static const DailyOrder orders[]={{FISH_BLUE,2,12},{FISH_PERCH,2,10},{FISH_TROUT,1,12},{FISH_CARP,1,18}};return orders[(day-1)%4];}
struct CatchRecord {int type,quality,saleValue;bool toxic=false;};
inline const char* qualityName(int q) {return q>=90?"Excellent":q>=75?"Good":q>=55?"Fair":"Damaged";}
struct DayLedger {
    bool active=false;int number=0,spent=0,baitUsed=0;
    bool orderAccepted=false;
    DailyOrder order{FISH_BLUE,2,12};
    std::vector<CatchRecord> kept;
    std::array<int,FISH_TYPE_COUNT> released{};
    void keep(int type,int quality=100,int quantity=1,bool toxic=false) {
        if(type<0 || type>=FISH_TYPE_COUNT)return;
        quality=quality<40?40:quality>100?100:quality;
        for(int i=0;i<quantity;++i)kept.push_back({type,quality,toxic?0:fishSaleValue(type)*quality/100,toxic});
    }
    int keptCount(int type) const {
        int count=0;
        for(const auto& fish:kept)count+=fish.type==type;
        return count;
    }
    int toxicCount(int type) const {
        int count=0;
        for(const auto& fish:kept)count+=fish.type==type && fish.toxic;
        return count;
    }
    int safeCount(int type) const {return keptCount(type)-toxicCount(type);}
    int cleaningCost() const {
        constexpr int toxicFishFee=10;
        int cost=0;
        for(const auto& fish:kept)cost+=fish.toxic?toxicFishFee:0;
        return cost;
    }
    int saleTotal(int type) const {
        int income=0;
        for(const auto& fish:kept)if(fish.type==type)income+=fish.saleValue;
        return income;
    }
    int averageQuality(int type) const {
        int totalQuality=0,count=0;
        for(const auto& fish:kept) {
            if(fish.type==type && !fish.toxic) {
                totalQuality+=fish.quality;
                ++count;
            }
        }
        return count?totalQuality/count:0;
    }
    bool orderComplete() const {return orderAccepted && safeCount(order.type)>=order.quantity;}
    int pendingValue() const {
        int income=0;
        for(const auto& fish:kept)income+=fish.saleValue;
        return income;
    }
};
struct OrderResult {
    bool accepted=false;int type=FISH_BLUE,required=0,keptHealthy=0;
    bool complete() const {return accepted && keptHealthy>=required;}
};
struct DaySummary {
    OrderResult order;
    bool valid=false;int number=0,gross=0,spent=0,net=0,closingCoins=0,orderBonus=0,totalIncome=0,cleaningFee=0;
    RoundMode mode=RoundMode::Classic;DayEnd reason=DayEnd::Early;
    MapId map=MapId::Forest;
    std::array<int,FISH_TYPE_COUNT> sold{},saleTotals{},qualityAverage{},caught{},released{},toxic{};
};
struct Preferences {float sensitivity=.75f,brightness=1,sfx=.55f;int camera=0;bool fullscreen=false;};
}
