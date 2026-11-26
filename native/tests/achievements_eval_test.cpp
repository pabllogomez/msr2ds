// Standalone test of the achievements list parser and evaluator (runtime/msr/achievements_eval.*); no game needed.
#include "achievements_eval.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace MsrAchievements;

static int g_checks = 0;
static int g_failed = 0;

#define CHECK(cond)                                                                       \
    do {                                                                                  \
        ++g_checks;                                                                       \
        if (!(cond)) {                                                                    \
            ++g_failed;                                                                   \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        }                                                                                 \
    } while (0)

using Ids = std::vector<std::string>;

static std::vector<Achievement> One(const std::string& line) {
    return Parse(line, nullptr);
}

int main() {
    // Parse: a valid list with comments, blank lines and CRLF
    {
        const std::string text =
            "# header\r\n"
            "\r\n"
            "  a_one | Items | First | Get the thing | item ITEM_A >= 1 & scenario s010_area1\r\n"
            "b2|Boss|Second|Has spaces, commas; ok|metroids_left <= 3\n"
            "   # indented comment\n"
            "c|Cat|Third|d|item ITEM_B >= 0.5 & item ITEM_A >= 175 & metroids_left <= 0";
        std::vector<std::string> errors;
        const auto list = Parse(text, &errors);
        CHECK(errors.empty());
        CHECK(list.size() == 3);
        CHECK(list[0].id == "a_one" && list[0].category == "Items" && list[0].name == "First");
        CHECK(list[0].description == "Get the thing");
        CHECK(list[0].all.size() == 2);
        CHECK(list[0].all[0].kind == Condition::Kind::Item && list[0].all[0].name == "ITEM_A");
        CHECK(list[0].all[0].value == 1.f);
        CHECK(list[0].all[1].kind == Condition::Kind::Scenario && list[0].all[1].name == "s010_area1");
        CHECK(list[1].id == "b2" && list[1].description == "Has spaces, commas; ok");
        CHECK(list[1].all.size() == 1 && list[1].all[0].kind == Condition::Kind::MetroidsLeft);
        CHECK(list[1].all[0].value == 3.f);
        CHECK(list[2].all.size() == 3 && list[2].all[0].value == 0.5f && list[2].all[1].value == 175.f);
        CHECK(list[2].all[2].value == 0.f);
    }

    // Parse: errors with line numbers, bad lines skipped, good ones kept
    {
        const std::string text =
            "good1|c|n|d|item ITEM_A >= 1\n"          // 1
            "Bad-Id|c|n|d|item ITEM_A >= 1\n"         // 2 bad id
            "only|three|fields\n"                      // 3 wrong field count
            "# comment\n"                              // 4
            "x|c|n|d|jump high\n"                      // 5 unknown condition
            "y|c|n|d|item ITEM_A >= abc\n"             // 6 bad number
            "good1|c|n|d|item ITEM_B >= 1\n"           // 7 duplicate id
            "z|c|n|d|item ITEM_A >= 1x\n"              // 8 bad number (trailing junk)
            "w|c|n|d|item ITEM_A > 1\n"                // 9 wrong operator
            "good2|c|n|d|scenario s020_area2\n";       // 10
        std::vector<std::string> errors;
        const auto list = Parse(text, &errors);
        CHECK(list.size() == 2);
        CHECK(list[0].id == "good1" && list[0].all[0].name == "ITEM_A");
        CHECK(list[1].id == "good2");
        CHECK(errors.size() == 7);
        if (errors.size() == 7) {
            CHECK(errors[0].rfind("line 2: ", 0) == 0);
            CHECK(errors[1].rfind("line 3: ", 0) == 0);
            CHECK(errors[2].rfind("line 5: ", 0) == 0);
            CHECK(errors[3].rfind("line 6: ", 0) == 0);
            CHECK(errors[4].rfind("line 7: ", 0) == 0);
            CHECK(errors[5].rfind("line 8: ", 0) == 0);
            CHECK(errors[6].rfind("line 9: ", 0) == 0);
        }
        CHECK(Parse("bad id|c|n|d|scenario s\n", nullptr).empty());   // null errors is fine
    }

    // Evaluate: item thresholds
    {
        const auto list = One("a|c|n|d|item ITEM_A >= 5");
        Snapshot s;
        s.items["ITEM_A"] = 4.f;
        CHECK(Evaluate(list, s, {}).empty());
        s.items["ITEM_A"] = 5.f;
        CHECK(Evaluate(list, s, {}) == Ids{"a"});
        s.items["ITEM_A"] = 6.f;
        CHECK(Evaluate(list, s, {}) == Ids{"a"});
        s.items.clear();   // absent
        CHECK(Evaluate(list, s, {}).empty());
        s.items["ITEM_A"] = -1.f;   // unreadable
        CHECK(Evaluate(list, s, {}).empty());
        const auto zero = One("z|c|n|d|item ITEM_A >= 0");
        CHECK(Evaluate(zero, s, {}).empty());   // -1 never satisfies, even >= 0
        s.items["ITEM_A"] = 0.f;
        CHECK(Evaluate(zero, s, {}) == Ids{"z"});
        s.items["ITEM_A"] = 5.f;
        CHECK(Evaluate(list, s, {"a"}).empty());   // already unlocked
    }

    // Evaluate: AND of conditions
    {
        const auto list = One("a|c|n|d|item ITEM_A >= 1 & scenario s010_area1 & metroids_left <= 2");
        Snapshot s;
        s.items["ITEM_A"] = 1.f;
        s.scenario = "s010_area1";
        s.metroids_left = 2;
        CHECK(Evaluate(list, s, {}) == Ids{"a"});
        s.metroids_left = 3;
        CHECK(Evaluate(list, s, {}).empty());
        s.metroids_left = 2;
        s.scenario = "s020_area2";
        CHECK(Evaluate(list, s, {}).empty());
        s.scenario = "s010_area1";
        s.items["ITEM_A"] = 0.f;
        CHECK(Evaluate(list, s, {}).empty());
    }

    // Evaluate: scenario and metroids_left
    {
        const auto sc = One("s|c|n|d|scenario s010_area1");
        Snapshot s;
        CHECK(Evaluate(sc, s, {}).empty());   // empty scenario
        s.scenario = "s010_area1";
        CHECK(Evaluate(sc, s, {}) == Ids{"s"});
        s.scenario = "s010_area2";
        CHECK(Evaluate(sc, s, {}).empty());

        const auto m = One("m|c|n|d|metroids_left <= 0");
        Snapshot t;
        CHECK(t.metroids_left == -1);
        CHECK(Evaluate(m, t, {}).empty());   // unknown
        t.metroids_left = 1;
        CHECK(Evaluate(m, t, {}).empty());   // above
        t.metroids_left = 0;
        CHECK(Evaluate(m, t, {}) == Ids{"m"});   // at
        const auto m3 = One("m|c|n|d|metroids_left <= 3");
        t.metroids_left = 2;
        CHECK(Evaluate(m3, t, {}) == Ids{"m"});   // below
    }

    // Evaluate: order preserved, only unlocked removed
    {
        const auto list = Parse("c|k|n|d|item I >= 1\na|k|n|d|item I >= 1\nb|k|n|d|item I >= 1\n", nullptr);
        Snapshot s;
        s.items["I"] = 1.f;
        CHECK((Evaluate(list, s, {}) == Ids{"c", "a", "b"}));
        CHECK((Evaluate(list, s, {"a"}) == Ids{"c", "b"}));
    }

    // Zero conditions never hold
    {
        const auto list = One("n|c|n|d|");
        CHECK(list.size() == 1 && list[0].all.empty());
        CHECK(Evaluate(list, Snapshot{}, {}).empty());
        Achievement a;
        a.id = "manual";
        CHECK(Evaluate({a}, Snapshot{}, {}).empty());
    }

    // ItemNames: dedup, sorted, items only
    {
        const auto list = Parse(
            "a|c|n|d|item ITEM_B >= 1 & scenario s1\n"
            "b|c|n|d|item ITEM_A >= 1 & item ITEM_B >= 2\n"
            "c|c|n|d|metroids_left <= 1\n",
            nullptr);
        CHECK((ItemNames(list) == Ids{"ITEM_A", "ITEM_B"}));
        CHECK(ItemNames({}).empty());
    }

    if (g_failed) {
        std::fprintf(stderr, "achievements_eval_test: %d of %d checks failed\n", g_failed, g_checks);
        return 1;
    }
    std::printf("achievements_eval_test: %d checks passed\n", g_checks);
    return 0;
}
