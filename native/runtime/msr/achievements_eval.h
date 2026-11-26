// The achievements' rules without the game: the list's text (achievements.txt) parsed into achievements, and a
// snapshot of what the game shows (items, map, metroids left) checked against them. achievements.cpp feeds it the
// snapshot on the main thread; nothing here touches the game, so native/tests/achievements_eval_test.cpp runs alone.
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace MsrAchievements {

struct Condition {
    enum class Kind { Item, Scenario, MetroidsLeft };
    Kind kind = Kind::Item;
    std::string name;    // Item: the inventory item's name (ITEM_...); Scenario: a map folder (s010_area1)
    float value = 0.f;   // Item: at least this; MetroidsLeft: at most this
};

struct Achievement {
    std::string id, category, name, description;
    std::vector<Condition> all;   // every one must hold
};

/// What the game shows at one moment (achievements.cpp reads it on the main thread).
struct Snapshot {
    std::map<std::string, float> items;   // by name; absent or < 0: could not be read (never satisfies a condition)
    std::string scenario;                 // the current map folder, "" when not known
    int metroids_left = -1;               // -1: not known
};

/// The list's text (achievements.txt); lines that cannot be read go to errors ("line N: why") and are skipped.
std::vector<Achievement> Parse(const std::string& text, std::vector<std::string>* errors);

/// The ids of the achievements that hold now and are not in unlocked yet, in the list's order.
std::vector<std::string> Evaluate(const std::vector<Achievement>& list, const Snapshot& now,
                                  const std::set<std::string>& unlocked);

/// Every item name the list's conditions name, once each, sorted (what achievements.cpp reads from the game).
std::vector<std::string> ItemNames(const std::vector<Achievement>& list);

}  // namespace MsrAchievements
