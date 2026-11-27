#pragma once
namespace lf::ui {
enum Action : int {
    Close=99, Menu=100, MenuEnd=200,
    Inventory=400, Shop=401, Settings=402, Pause=403, CloseHelp=407, Help=408,
    SettingDecrease=500, SettingEnd=512, SoundSlider=520,
    Keep=600, Release=601,
    Category=700, CategoryEnd=706, OpenInventory=710, OpenShop=711,
    Item=810, ItemEnd=818, Buy=820, Equip=821,
    QuantityDown=830, QuantityUp=831, AddOne=832, AddFive=833, AddTen=834,
    AcceptOrder=840, EndDay=841, PreviousPage=850, NextPage=851,
    Location=860, LocationEnd=863, BuyLocation=870, BuyLocationEnd=873,
    Mode=900, ModeEnd=903, DifficultyDown=903, DifficultyUp=904,
    Start=905, TripInventory=906, TripShop=907, AcceptTrip=908, SkipTrip=909,
    SecretFish=920, CreatorProfile=921
};
} // namespace lf::ui
