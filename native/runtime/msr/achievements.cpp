// msr-native's achievements (achievements.h): Metroid II's set on RetroAchievements fitted to the remake, local only.
// - The list: achievements.txt, built into the program (achievements_list.inc, made by CMake), or the player's own
//   achievements.txt beside settings.ini. Its conditions are achievements_eval.cpp's, which knows nothing of the game.
// - What the game shows: Samus's inventory items by name (msr_item, cheats.cpp), read on the main thread every 2 s where
//   the Discord presence reads the Metroids left (main.c), so never while the game's thread is writing a frame's worth.
//   msr_item needs each item's key global: the known ones (cheats.h) lie together near 0x7086B8, and the others are found
//   there by their names, once, at the first moment in play. UNVERIFIED: that every item's key is in that range (see
//   docs/conquistas-estado-do-jogo.md); a name the scan does not find is logged and its achievements stay locked.
// - Nothing unlocks while a cheat keeps energy, missiles or Aeion full (the Cheats tab).
// - The unlocked ones: user/msr/achievements.ini [unlocked] id=when, written at once (MsrIni: a temporary file renamed
//   over the old one), so closing the game loses none.
// - The notice: the overlay draws the newest unlock's name and description in the top right corner for a few seconds,
//   over the game and below the menu, when the player's "Achievement notices" setting has it.
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <deque>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <imgui.h>
#include "achievements.h"
#include "achievements_eval.h"
#include "ini.h"
#include "msr.h"
#include "msr/cheats.h"
#include "msr/overlay.h"
#include "cpu.h"
extern "C" {
#include "kernel.h"
}

namespace {
namespace A = MsrAchievements;
namespace C = MSR::Cheats;
using u32 = std::uint32_t;

constexpr char BuiltIn[] =
#include "achievements_list.inc"
    ;

// The Metroid counts' key globals (discord.cpp, compositor.cpp: verified)
constexpr u32 KeyMetroids = 0x7086BC, KeyMetroidsTotal = 0x7086C0;
// Where the item name globals are looked for: the known ones are 0x7086B8..0x70873C (cheats.h), the range around them
// is a guess (UNVERIFIED)
constexpr u32 KeysFrom = 0x708000, KeysTo = 0x70A000;
// The cheats that make an unlock worthless
constexpr u32 Cheats = C::InfiniteEnergy | C::InfiniteMissiles | C::InfiniteAeion;
constexpr double NoticeSeconds = 5.0;

// Main thread only (start and tick)
std::vector<A::Achievement> list;
std::set<std::string> unlocked;
std::map<std::string, u32> keys;   // item name -> its key global, once found
std::string progress;              // achievements.ini; "" when there is no settings file
bool started = false, scanned = false, cheat_noted = false;

std::mutex lock;   // the scenario (the file system's thread) and the notices (the overlay's)
std::string scenario;
struct Notice {
    std::string category, name, description;
};
std::deque<Notice> notices;
double notice_shown = -1.0;

double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

// A guest range is readable (cheats.cpp's)
bool Valid(u32 address) { return address >= 0x00100000 && k_region_state(address) != 0; }

// The item name a key global points at (as msr_item reads it: a name object whose first word is its text), "" when it
// is not one: unreadable, longer than 64 or not printable
std::string NameAt(u32 key) {
    const u32 object = Valid(key) ? rd32(key) : 0;
    const u32 text = Valid(object + 0x14) ? rd32(object) : 0;
    std::string name;
    for (u32 i = 0; i < 64; ++i) {
        if (!Valid(text + i)) return {};
        const u32 c = rd8(text + i);
        if (c == 0) return name;
        if (c < 0x20 || c > 0x7E) return {};
        name.push_back(static_cast<char>(c));
    }
    return {};
}

// The key globals of the list's items, by their names; every ITEM_ name in the range logged once, for checking the list
void FindKeys() {
    const std::vector<std::string> wanted = A::ItemNames(list);
    std::string seen;
    for (u32 at = KeysFrom; at < KeysTo; at += 4) {
        const std::string name = NameAt(at);
        if (name.rfind("ITEM_", 0) != 0) continue;
        seen += (seen.empty() ? "" : " ") + name;
        if (!keys.count(name) && std::binary_search(wanted.begin(), wanted.end(), name)) keys[name] = at;
    }
    if (keys.empty()) return;   // (not set up yet: again at the next tick)
    scanned = true;
    std::fprintf(stderr, "achievements: the game's item names: %s\n", seen.c_str());
    for (const std::string& name : wanted)
        if (!keys.count(name)) std::fprintf(stderr, "achievements: no item %s in this game: its achievements stay locked\n", name.c_str());
}

std::string Folder(const char* file) {
    const std::string path = file ? file : "";
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
}

std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return {};
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

void Unlock(const A::Achievement& a, bool notice) {
    unlocked.insert(a.id);
    std::fprintf(stderr, "achievements: unlocked %s (%s)\n", a.id.c_str(), a.name.c_str());
    if (!progress.empty()) {
        char when[32];
        const std::time_t t = std::time(nullptr);
        std::strftime(when, sizeof when, "%Y-%m-%d %H:%M:%S", std::localtime(&t));
        MsrIni::File file(progress);
        file.SetString("unlocked", a.id, when);
        if (!file.Save()) std::fprintf(stderr, "achievements: could not write %s\n", progress.c_str());
    }
    if (notice) {
        std::scoped_lock hold{lock};
        notices.push_back({a.category, a.name, a.description});
    }
}

// "packs/maps/<scenario>/<scenario>.pkg" -> "<scenario>" (discord.cpp's ScenarioOf); "" for anything else
std::string ScenarioOf(const char* path) {
    static constexpr char Maps[] = "packs/maps/";
    if (!path || std::strncmp(path, Maps, sizeof Maps - 1)) return {};
    const char* folder = path + sizeof Maps - 1;
    const char* slash = std::strchr(folder, '/');
    if (!slash) return {};
    const std::string name(folder, slash);
    return std::strcmp(slash + 1, (name + ".pkg").c_str()) == 0 ? name : std::string();
}
// The notice's badge: a hexagon in the category's color with its mark, drawn (no image: nothing from the game or from
// anywhere else). Items a diamond, Remake a four-pointed star, Collectibles a plus, Metroids a crosshair, any other
// category (a player's own list) a dot.
void DrawBadge(ImDrawList* draw, ImVec2 c, float r, const std::string& category, float alpha) {
    struct Style {
        const char* category;
        ImU32 rgb;
    };
    static constexpr Style Styles[] = {{"Items", IM_COL32(60, 170, 255, 0)},
                                       {"Remake", IM_COL32(170, 110, 255, 0)},
                                       {"Collectibles", IM_COL32(60, 210, 140, 0)},
                                       {"Metroids", IM_COL32(255, 170, 50, 0)}};
    ImU32 rgb = IM_COL32(150, 165, 180, 0);
    int mark = -1;
    for (int i = 0; i < 4; ++i)
        if (category == Styles[i].category) rgb = Styles[i].rgb, mark = i;
    const auto with = [alpha](ImU32 color, float a) { return color | (static_cast<ImU32>(255 * a * alpha) << IM_COL32_A_SHIFT); };
    ImVec2 hex[6];
    for (int i = 0; i < 6; ++i) {
        const float angle = 3.14159265f / 3.f * static_cast<float>(i) + 3.14159265f / 6.f;
        hex[i] = {c.x + r * std::cos(angle), c.y + r * std::sin(angle)};
    }
    draw->AddConvexPolyFilled(hex, 6, with(rgb, .35f));
    draw->AddPolyline(hex, 6, with(rgb, 1.f), ImDrawFlags_Closed, std::max(1.5f, r * .1f));
    const ImU32 ink = with(IM_COL32(255, 255, 255, 0), 1.f);
    const float m = r * .45f, line = std::max(1.5f, r * .12f);
    switch (mark) {
    case 0: draw->AddQuadFilled({c.x, c.y - m}, {c.x + m * .7f, c.y}, {c.x, c.y + m}, {c.x - m * .7f, c.y}, ink); break;
    case 1: {
        ImVec2 star[8];
        for (int i = 0; i < 8; ++i) {
            const float angle = 3.14159265f / 4.f * static_cast<float>(i) - 3.14159265f / 2.f, k = i % 2 ? m * .35f : m;
            star[i] = {c.x + k * std::cos(angle), c.y + k * std::sin(angle)};
        }
        for (int i = 0; i < 8; i += 2) draw->AddTriangleFilled(c, star[i], star[(i + 1) % 8], ink), draw->AddTriangleFilled(c, star[(i + 7) % 8], star[i], ink);
        break;
    }
    case 2:
        draw->AddRectFilled({c.x - m, c.y - line}, {c.x + m, c.y + line}, ink);
        draw->AddRectFilled({c.x - line, c.y - m}, {c.x + line, c.y + m}, ink);
        break;
    case 3:
        draw->AddCircle(c, m * .75f, ink, 0, line);
        draw->AddLine({c.x - m, c.y}, {c.x + m, c.y}, ink, line * .7f);
        draw->AddLine({c.x, c.y - m}, {c.x, c.y + m}, ink, line * .7f);
        break;
    default: draw->AddCircleFilled(c, m * .4f, ink); break;
    }
}
} // namespace

extern "C" void msr_achievements_start(void) {
    const std::string folder = Folder(msr_settings_file());
    std::string text = folder.empty() ? std::string() : ReadFile(folder + "achievements.txt");
    const bool own = !text.empty();
    if (!own) text = BuiltIn;
    std::vector<std::string> errors;
    list = A::Parse(text, &errors);
    for (const std::string& e : errors)
        std::fprintf(stderr, "achievements: %s, %s\n", own ? "achievements.txt" : "the built-in list", e.c_str());
    if (!folder.empty()) {
        progress = folder + "achievements.ini";
        const MsrIni::File file(progress);
        for (const A::Achievement& a : list)
            if (file.Raw("unlocked", a.id)) unlocked.insert(a.id);
    }
    started = true;
    std::fprintf(stderr, "achievements: %zu in the %s list, %zu unlocked (%s)\n", list.size(), own ? "player's" : "built-in",
                 unlocked.size(), MSR::GetOverlay().GetPlayerSettings().achievements ? "on" : "off");
}

extern "C" void msr_achievements_scenario(const char* path) {
    std::string s = ScenarioOf(path);
    if (s.empty()) return;
    std::scoped_lock hold{lock};
    scenario = std::move(s);
}

extern "C" void msr_achievements_tick(void) {
    if (!started) return;
    const MSR::PlayerSettings settings = MSR::GetOverlay().GetPlayerSettings();
    if (!settings.achievements || unlocked.size() == list.size()) return;
    if (C::enabled.load() & Cheats) {
        if (!cheat_noted) std::fprintf(stderr, "achievements: a cheat is on: nothing unlocks while it is\n");
        cheat_noted = true;
        return;
    }
    cheat_noted = false;
    const float total = msr_item(KeyMetroidsTotal, "ITEM_METROID_TOTAL_COUNT");
    if (total < 0.f) return;   // (out of play, or not this game: msr_item cannot tell)
    if (!scanned) FindKeys();

    A::Snapshot now;
    for (const auto& [name, key] : keys) now.items[name] = msr_item(key, name.c_str());
    const float killed = msr_item(KeyMetroids, "ITEM_METROID_COUNT");
    if (killed >= 0.f && total > 0.f) now.metroids_left = std::max(0, static_cast<int>(total - killed));
    {
        std::scoped_lock hold{lock};
        now.scenario = scenario;
    }
    for (const std::string& id : A::Evaluate(list, now, unlocked)) {
        const auto a = std::find_if(list.begin(), list.end(), [&id](const A::Achievement& x) { return x.id == id; });
        Unlock(*a, settings.achievement_notices);
    }
}

extern "C" void msr_achievements_draw(float scale, float width, float height) {
    std::scoped_lock hold{lock};
    if (notices.empty()) return;
    const double t = Now();
    if (notice_shown < 0.0) notice_shown = t;
    const float age = static_cast<float>(t - notice_shown);
    if (age >= NoticeSeconds) {   // the next one at the next frame
        notices.pop_front();
        notice_shown = -1.0;
        return;
    }
    const float alpha = std::clamp(std::min(age / .25f, (static_cast<float>(NoticeSeconds) - age) / .5f), 0.f, 1.f);
    const Notice& n = notices.front();
    ImFont* font = ImGui::GetFont();
    const float pad = 12.f * scale, wrap = std::min(420.f * scale, width * .4f);
    const float head = 16.f * scale, title = 22.f * scale, body = 18.f * scale;
    const char* heading = "Achievement unlocked";
    const ImVec2 h = font->CalcTextSizeA(head, FLT_MAX, 0.f, heading);
    const ImVec2 a = font->CalcTextSizeA(title, FLT_MAX, wrap, n.name.c_str());
    const ImVec2 b = font->CalcTextSizeA(body, FLT_MAX, wrap, n.description.c_str());
    const float w = std::max({h.x, a.x, b.x}), gap = 4.f * scale, tall = h.y + a.y + b.y + 2 * gap;
    const float badge = std::min(tall, 64.f * scale);   // (the badge's width, left of the text)
    const ImVec2 at{width - 18.f * scale - pad - w, 18.f * scale + pad};
    auto* draw = ImGui::GetForegroundDrawList();
    draw->AddRectFilled({at.x - pad - badge - pad, at.y - pad}, {at.x + w + pad, at.y + tall + pad},
                        IM_COL32(4, 14, 22, static_cast<int>(200 * alpha)), 6.f * scale);
    DrawBadge(draw, {at.x - pad - badge / 2.f, at.y + tall / 2.f}, badge * .48f, n.category, alpha);
    draw->AddText(font, head, at, IM_COL32(90, 200, 255, static_cast<int>(255 * alpha)), heading);
    draw->AddText(font, title, {at.x, at.y + h.y + gap}, IM_COL32(255, 255, 255, static_cast<int>(255 * alpha)),
                  n.name.c_str(), nullptr, wrap);
    draw->AddText(font, body, {at.x, at.y + h.y + a.y + 2 * gap}, IM_COL32(200, 220, 235, static_cast<int>(255 * alpha)),
                  n.description.c_str(), nullptr, wrap);
    (void)height;
}
