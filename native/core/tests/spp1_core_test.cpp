// Shared Preservation Parity Batch 1 (docs/SHARED_PRESERVATION_PARITY_SPP1.md) -- the core
// rules, driven directly. Candidate C: the arena's (J)immy, (P)ush and (Y)ell (COMBAT 0x097a /
// 0x0994 / 0x09c0; spp1-findings/C-arena-jpy.md). One party actor at (5,5) on floor 0x44; the
// strings, tiles, key counts, RNG draws and the turn are read back exactly.
#include "openu5/combat.h"

#include <cstdio>
#include <string>
#include <utility>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool ok, const std::string &what) {
    ++checks;
    if (!ok) ++failures;
    std::printf("%s %s\n", ok ? "GREEN" : "RED", what.c_str());
}

struct Arena {
    GameState g{};
    TurnState t{};
    CombatState s{};
    CombatContext c{g, t, s};
    std::vector<std::string> text;
    std::vector<std::pair<int, int>> draws;
    std::vector<CombatEvent> moves;
    explicit Arena(int dex = 20, int keys = 2) {
        g.party.party_size = g.party.character_count = 1;
        auto &m = g.party.characters[0];
        m.status = 'G';
        m.current_hp = m.max_hp = 100;
        m.dexterity = uint8_t(dex);
        g.keys = keys;
        g.karma = 50;
        s.initialized = true;
        s.count = 1;
        s.current = 0;
        auto &a = s.actors[0];
        a.id = 1;
        a.member = 0;
        a.status = CombatStatus::Active;
        a.position = {5, 5};
        a.hp = a.max_hp = 100;
        // A hostile asleep in the far corner keeps the fight going after every turn.
        s.count = 2;
        auto &foe = s.actors[1];
        foe.id = 9;
        foe.status = CombatStatus::Active;
        foe.position = {0, 10};
        foe.hp = foe.max_hp = 50;
        foe.sleeping = true;
        for (auto &tile : s.map.tiles) tile = 0x44;
        s.rng.seed(7);
        c.events = {this, [](void *p, const CombatEvent &e) {
                        auto &self = *static_cast<Arena *>(p);
                        if (e.kind == CombatEventKind::Moved) self.moves.push_back(e);
                        else if (e.text) self.text.emplace_back(e.text);
                    }};
        c.trace = {this, [](void *p, const char *, int32_t lo, int32_t hi, int32_t) {
                       static_cast<Arena *>(p)->draws.emplace_back(lo, hi);
                   }};
    }
    void set(int x, int y, int v) { s.map.tiles[y * 11 + x] = int16_t(v); }
    int at(int x, int y) const { return s.map.tiles[y * 11 + x]; }
    /** One action; true when the actor's turn ended (a new activation was scheduled). */
    bool act(CombatAction action, int x) {
        text.clear();
        draws.clear();
        moves.clear();
        const auto before = s.action_count;
        combat_action(c, action, x);
        return s.action_count != before;
    }
    std::string said() const {
        std::string out;
        for (const auto &l : text) out += (out.empty() ? "" : " | ") + l;
        return out.empty() ? "(nothing)" : out;
    }
    bool one_draw(int lo, int hi) const { return draws.size() == 1 && draws[0] == std::make_pair(lo, hi); }
};
constexpr int E = 0, W = 1, S = 2, N = 3; // the arena's getdir ordinal (commands.cpp)
// The three actions APPENDED after SetActive (combat.h); named by ordinal so the same test also
// builds against a core that predates them (which rejects the ordinals: the RED-first run).
constexpr CombatAction kJimmy = CombatAction(int(CombatAction::SetActive) + 1);
constexpr CombatAction kPush = CombatAction(int(CombatAction::SetActive) + 2);
constexpr CombatAction kYell = CombatAction(int(CombatAction::SetActive) + 3);

void test_jimmy() {
    std::printf("C/J  the arena's (J)immy: SJOG 0x0d4a through COMBAT 0x097a\n");
    {
        Arena a(20, 0);
        const bool turn = a.act(kJimmy, E);
        check(a.said() == "No Keys!" && a.g.keys == 0 && a.draws.empty() && turn,
              "J1 no key: \"No Keys!\" (SJOG 0x0d66, before the getdir), no draw, the turn ends: " + a.said());
    }
    {
        Arena a(30, 2);
        a.set(6, 5, 0xb9);
        const bool turn = a.act(kJimmy, E);
        check(a.said() == "Unlocked!" && a.at(6, 5) == 0xb8 && a.g.keys == 2 && a.one_draw(0, 29) && turn,
              "J2 a locked door, DEX 30: rand(0,29) once, \"Unlocked!\", 0xB9 -> 0xB8, the key kept: " + a.said());
    }
    {
        Arena a(0, 2);
        a.set(6, 5, 0xbb);
        a.act(kJimmy, E);
        check(a.said() == "Key broke!" && a.at(6, 5) == 0xbb && a.g.keys == 1 && a.one_draw(0, 29),
              "J3 a windowed locked door, DEX 0: one draw, \"Key broke!\", one key lost, the door stays: " + a.said());
    }
    {
        Arena a(30, 2);
        a.set(6, 5, 0x97);
        a.act(kJimmy, E);
        check(a.said() == "Key broke!" && a.g.keys == 1 && a.draws.empty() && a.at(6, 5) == 0x97,
              "J4 a magic lock always breaks the key, with no draw (0x0e1c)");
    }
    {
        Arena a(30, 2);
        a.set(6, 5, 0x85);
        a.act(kJimmy, E);
        check(a.said() == "Unlocked" && a.at(6, 5) == 0x44 && a.one_draw(0, 29) && a.g.karma == 50 && a.g.keys == 2,
              "J5 manacles, DEX 30: \"Unlocked\" with no '!', tile 0x44, no karma (0x0ee4): " + a.said());
    }
    {
        Arena a(30, 2);
        a.s.loot[5 * 11 + 6] = 1;
        a.s.chest_contents[5 * 11 + 6] = 4;
        a.s.chest_state[5 * 11 + 6] = CombatChestState::Unopened;
        a.act(kJimmy, E);
        check(a.said() == "Key broke!" && a.g.keys == 1 && a.draws.empty() && a.s.loot[5 * 11 + 6] == 1,
              "J6 an untrapped chest: the key breaks with no draw (0x0bcb), the chest untouched");
    }
    {
        Arena a(30, 2);
        a.s.loot[5 * 11 + 6] = 129;
        a.s.chest_contents[5 * 11 + 6] = 0;
        a.s.chest_state[5 * 11 + 6] = CombatChestState::Unopened;
        a.act(kJimmy, E);
        check(a.said() == "Success!" && a.s.loot[5 * 11 + 6] == 1 && a.g.keys == 2 && a.one_draw(1, 30),
              "J7 a trapped chest, DEX 30 (threshold 0): rand(1,30), \"Success!\", the trap disarmed, still shut: " + a.said());
    }
    {
        Arena a(50, 2);
        a.s.loot[5 * 11 + 6] = 129;
        a.s.chest_contents[5 * 11 + 6] = 0;
        a.s.chest_state[5 * 11 + 6] = CombatChestState::Unopened;
        a.act(kJimmy, E);
        check(a.said() == "Key broke!" && a.s.loot[5 * 11 + 6] == 129 && a.g.keys == 1 && a.one_draw(1, 30),
              "J8 DEX 50 wraps the byte threshold to 0xF6 (0x0bd2-0x0bee): the key breaks whatever the roll: " + a.said());
    }
    {
        Arena a(30, 2);
        const bool turn = a.act(kJimmy, N);
        check(a.said() == "No lock!" && a.g.keys == 2 && a.draws.empty() && turn, "J9 bare floor: \"No lock!\", the key kept, the turn ends");
    }
    {
        Arena a(30, 2);
        a.set(6, 5, 0xb9);
        const bool turn = a.act(kJimmy, -1);
        check(a.text.empty() && a.draws.empty() && a.at(6, 5) == 0xb9 && turn, "J10 Space at the getdir: nothing at all, the turn ends");
    }
    {
        Arena a(30, 2);
        a.s.actors[0].position = {10, 5};
        a.act(kJimmy, E);
        check(a.said() == "No lock!" && a.g.keys == 2, "J11 off the grid: \"No lock!\" (native reads no memory beyond it; recorded)");
    }
}

void test_push() {
    std::printf("C/P  the arena's (P)ush: CMDS 0x161a through COMBAT 0x0994\n");
    {
        Arena a;
        a.set(6, 5, 0xb4);
        a.set(7, 5, 0x45);
        const bool turn = a.act(kPush, E);
        check(a.said() == "Pushed!" && a.at(7, 5) == 0xb5 && a.at(6, 5) == 0x45 && a.s.actors[0].position.x == 6 &&
                  a.moves.size() == 1 && a.draws.empty() && turn,
              "P1 a 0xB4 pushed east over its fill 0x45: \"Pushed!\", it turns to face east (0xB5), the actor steps in: " + a.said());
    }
    {
        Arena a;
        a.set(6, 5, 0x5b);
        a.set(7, 5, 0x80);
        a.act(kPush, E);
        check(a.said() == "Pulled!" && a.at(5, 5) == 0x5b && a.at(6, 5) == 0x44 && a.s.actors[0].position.x == 6,
              "P2 nothing free beyond, the actor on the fill 0x44: \"Pulled!\" and the two swap: " + a.said());
    }
    {
        Arena a;
        a.set(5, 5, 0x45);
        a.set(6, 5, 0xb4);
        a.set(7, 5, 0x80);
        a.act(kPush, E);
        check(a.said() == "Pulled!" && a.at(5, 5) == 0xb7 && a.at(6, 5) == 0x45, "P3 a pulled 0xB4 faces back (class + (idx ^ 2) = 0xB7)");
    }
    {
        Arena a;
        a.set(6, 5, 0x5b);
        a.s.actors[1].position = {6, 5}; // the hostile stands on the target
        a.act(kPush, E);
        check(a.said() == "Won't budge!" && a.at(6, 5) == 0x5b && a.s.actors[0].position.x == 5, "P4 a combatant on the target: \"Won't budge!\"");
    }
    {
        Arena a;
        a.act(kPush, E);
        check(a.said() == "Won't budge!", "P5 bare floor is not pushable: \"Won't budge!\"");
    }
    {
        Arena a;
        a.set(5, 5, 0x45);
        a.set(6, 5, 0x5b);
        a.set(7, 5, 0x45);
        a.act(kPush, E);
        check(a.said() == "Won't budge" && a.at(6, 5) == 0x5b, "P6 the fill (0x44) neither beyond nor here: \"Won't budge\" -- the other string, no '!'");
    }
    {
        Arena a;
        a.set(6, 5, 0xb4);
        a.act(kPush, E);
        check(a.said() == "Won't budge" && a.at(6, 5) == 0xb4, "P7 a 0xB4's fill is 0x45, so bare 0x44 floor neither slides nor pulls it");
    }
    {
        Arena a;
        a.set(6, 5, 0x5b);
        a.s.loot[5 * 11 + 7] = 30;
        a.act(kPush, E);
        check(a.said() == "Pulled!" && a.at(5, 5) == 0x5b, "P8 a corpse on the cell beyond blocks the push (kernel 0x368E): a pull instead");
    }
    {
        Arena a;
        a.s.room = true;
        a.set(6, 5, 0x5b);
        a.set(7, 5, 0x44);
        a.s.map.trigger_count = 1;
        auto &t = a.s.map.triggers[0];
        t.at = {6, 5};
        t.first = {2, 2};
        t.second = {3, 3};
        t.tile = 0x4d;
        const bool turn = a.act(kPush, E);
        check(a.text.empty() && a.at(2, 2) == 0x4d && a.at(3, 3) == 0x4d && t.at.x == 255 && a.at(6, 5) == 0x5b &&
                  a.s.actors[0].position.x == 5 && turn,
              "P9 a dungeon room's live plate on the target fires, silently, and nothing moves (COMBAT 0x111a)");
    }
    {
        Arena a;
        a.set(6, 5, 0x5b);
        a.set(7, 5, 0x44);
        a.s.map.trigger_count = 1;
        auto &t = a.s.map.triggers[0];
        t.at = {6, 5};
        t.first = {2, 2};
        t.second = {3, 3};
        t.tile = 0x4d;
        a.act(kPush, E);
        check(a.said() == "Pushed!" && a.at(2, 2) == 0x44 && t.at.x == 6 && a.at(7, 5) == 0x5b,
              "P10 outside a room the plate is not checked ([0x58a1] = 0): an ordinary push");
    }
    {
        Arena a;
        a.set(6, 5, 0x5b);
        a.set(7, 5, 0x44);
        const bool turn = a.act(kPush, -1);
        check(a.text.empty() && a.at(6, 5) == 0x5b && turn, "P11 Space at the getdir: nothing, the turn ends");
    }
    {
        Arena a;
        a.s.actors[0].position = {5, 6};
        a.set(5, 5, 0x90);
        a.set(5, 4, 0x44);
        a.act(kPush, N);
        check(a.said() == "Pushed!" && a.at(5, 4) == 0x90, "P12 pushed north a 0x90 faces north (idx 0)");
    }
}

void test_yell() {
    std::printf("C/Y  the arena's (Y)ell: CMDS 0x1418 through COMBAT 0x09c0\n");
    {
        Arena a;
        const bool turn = a.act(kYell, 1);
        check(a.said() == "No effect!" && a.draws.empty() && turn, "Y1 any word: \"No effect!\" (DS 0x453a; no sails at g_location 0xFF), the turn ends");
    }
    {
        Arena a;
        const bool turn = a.act(kYell, 0);
        check(a.text.empty() && a.draws.empty() && turn, "Y2 an empty answer: the core adds nothing (\"Nothing\" is the ':' row's), the turn ends");
    }
}
} // namespace

int main() {
    test_jimmy();
    test_push();
    test_yell();
    std::printf("SPP1 core: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
