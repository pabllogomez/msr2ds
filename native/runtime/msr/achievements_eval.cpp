// Parsing of the achievements list and the evaluation of its conditions against a snapshot of the game (see
// achievements_eval.h). Standard library only.
#include "achievements_eval.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace MsrAchievements {

namespace {

std::string Trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::vector<std::string> Split(const std::string& s, char sep) {
    std::vector<std::string> out;
    size_t start = 0;
    for (;;) {
        const size_t at = s.find(sep, start);
        if (at == std::string::npos) {
            out.push_back(s.substr(start));
            return out;
        }
        out.push_back(s.substr(start, at - start));
        start = at + 1;
    }
}

std::vector<std::string> Tokens(const std::string& s) {
    std::istringstream in(s);
    std::vector<std::string> out;
    for (std::string t; in >> t;) out.push_back(t);
    return out;
}

bool ValidId(const std::string& id) {
    if (id.empty()) return false;
    for (const char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    }
    return true;
}

// A whole-token decimal number (no trailing junk, finite).
bool ParseNumber(const std::string& s, float& out) {
    if (s.empty() || std::isspace(static_cast<unsigned char>(s[0]))) return false;
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end != s.c_str() + s.size() || v != v || v > 1e30 || v < -1e30) return false;
    out = static_cast<float>(v);
    return true;
}

// One condition's text; returns "" when fine, else why not.
std::string ParseCondition(const std::string& text, Condition& c) {
    const std::vector<std::string> t = Tokens(text);
    if (t.empty()) return "empty condition";
    if (t[0] == "item") {
        if (t.size() != 4 || t[2] != ">=") return "expected 'item NAME >= N' in '" + text + "'";
        if (!ParseNumber(t[3], c.value)) return "bad number '" + t[3] + "'";
        c.kind = Condition::Kind::Item;
        c.name = t[1];
        return "";
    }
    if (t[0] == "scenario") {
        if (t.size() != 2) return "expected 'scenario NAME' in '" + text + "'";
        c.kind = Condition::Kind::Scenario;
        c.name = t[1];
        return "";
    }
    if (t[0] == "metroids_left") {
        if (t.size() != 3 || t[1] != "<=") return "expected 'metroids_left <= N' in '" + text + "'";
        if (!ParseNumber(t[2], c.value)) return "bad number '" + t[2] + "'";
        c.kind = Condition::Kind::MetroidsLeft;
        c.name.clear();
        return "";
    }
    return "unknown condition '" + text + "'";
}

bool Holds(const Condition& c, const Snapshot& now) {
    switch (c.kind) {
    case Condition::Kind::Item: {
        const auto it = now.items.find(c.name);
        return it != now.items.end() && it->second >= 0.f && it->second >= c.value;
    }
    case Condition::Kind::Scenario:
        return !c.name.empty() && now.scenario == c.name;
    case Condition::Kind::MetroidsLeft:
        return now.metroids_left >= 0 && static_cast<float>(now.metroids_left) <= c.value;
    }
    return false;
}

}  // namespace

std::vector<Achievement> Parse(const std::string& text, std::vector<std::string>* errors) {
    std::vector<Achievement> list;
    std::set<std::string> seen;
    std::istringstream in(text);
    int number = 0;
    for (std::string raw; std::getline(in, raw);) {
        ++number;
        const std::string line = Trim(raw);   // also drops the CR of CRLF
        if (line.empty() || line[0] == '#') continue;
        const auto fail = [&](const std::string& why) {
            if (errors) errors->push_back("line " + std::to_string(number) + ": " + why);
        };
        const std::vector<std::string> f = Split(line, '|');
        if (f.size() != 5) {
            fail("expected 5 fields separated by '|', found " + std::to_string(f.size()));
            continue;
        }
        Achievement a;
        a.id = Trim(f[0]);
        a.category = Trim(f[1]);
        a.name = Trim(f[2]);
        a.description = Trim(f[3]);
        if (!ValidId(a.id)) {
            fail("bad id '" + a.id + "' (use a-z, 0-9, _)");
            continue;
        }
        if (seen.count(a.id)) {
            fail("duplicate id '" + a.id + "'");
            continue;
        }
        std::string problem;
        for (const std::string& part : Split(f[4], '&')) {
            if (Trim(part).empty()) {
                if (Trim(f[4]).empty()) break;   // no conditions at all: allowed, it never holds
                problem = "empty condition";
                break;
            }
            Condition c;
            problem = ParseCondition(Trim(part), c);
            if (!problem.empty()) break;
            a.all.push_back(std::move(c));
        }
        if (!problem.empty()) {
            fail(problem);
            continue;
        }
        seen.insert(a.id);
        list.push_back(std::move(a));
    }
    return list;
}

std::vector<std::string> Evaluate(const std::vector<Achievement>& list, const Snapshot& now,
                                  const std::set<std::string>& unlocked) {
    std::vector<std::string> out;
    for (const Achievement& a : list) {
        if (a.all.empty() || unlocked.count(a.id)) continue;
        if (std::all_of(a.all.begin(), a.all.end(), [&](const Condition& c) { return Holds(c, now); })) {
            out.push_back(a.id);
        }
    }
    return out;
}

std::vector<std::string> ItemNames(const std::vector<Achievement>& list) {
    std::set<std::string> names;
    for (const Achievement& a : list) {
        for (const Condition& c : a.all) {
            if (c.kind == Condition::Kind::Item) names.insert(c.name);
        }
    }
    return {names.begin(), names.end()};
}

}  // namespace MsrAchievements
