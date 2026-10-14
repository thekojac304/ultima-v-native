// Shared Preservation Parity Batch 1 (docs/SHARED_PRESERVATION_PARITY_SPP1.md) -- the core
// rules, driven directly. Candidate C: the arena's (J)immy, (P)ush and (Y)ell (COMBAT 0x097a /
// 0x0994 / 0x09c0; spp1-findings/C-arena-jpy.md). One party actor at (5,5) on floor 0x44; the
// strings, tiles, key counts, RNG draws and the turn are read back exactly.
#include "openu5/combat.h"
#include "openu5/dungeon.h"
#include "openu5/shop_orchestration.h"

#include <algorithm>
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
// --- Candidate E: the dungeon (L)ook, DNGLOOK.OVL 0x0000 (spp1-findings/E-dungeon-look.md) ------
// Appended after Search / Error (dungeon.h); by ordinal, so the test also builds against a core
// that predates them (the RED-first run).
constexpr DungeonAction kLook = DungeonAction(int(DungeonAction::Search) + 1);
constexpr DungeonAction kLookDrink = DungeonAction(int(DungeonAction::Search) + 2);
constexpr DungeonEventKind kDrinkPrompt = DungeonEventKind(int(DungeonEventKind::Error) + 1);
constexpr int kPass = 4, kNoDir = -1;
struct Corridor {
    GameState g{};
    TurnState t{};
    DungeonState d{};
    std::vector<std::string> text;
    int drink_prompts = 0;
    int draws_before = 0;
    explicit Corridor(uint8_t dungeon = 0x22) {
        g.party.party_size = g.party.character_count = 3;
        for (int i = 0; i < 3; ++i) {
            auto &m = g.party.characters[i];
            m.status = 'G';
            m.current_hp = 40;
            m.max_hp = 90;
        }
        g.party.active_character = 255;
        g.torch_turns = 50;
        g.rng.seed(11);
        d.active = true;
        d.pos = {dungeon, 0, 3, 3, DungeonFacing::North};
        for (auto &c : d.cells) c = 0x00;
    }
    void put(int x, int y, uint8_t raw) { d.cells[y * 8 + x] = raw; }
    void act(DungeonAction a, int dir, int member) {
        text.clear();
        drink_prompts = 0;
        DungeonSink sink{this, [](void *p, const DungeonEvent &e) {
                             auto &self = *static_cast<Corridor *>(p);
                             if (e.kind == DungeonEventKind::Message && e.text) self.text.emplace_back(e.text);
                             if (e.kind == kDrinkPrompt) ++self.drink_prompts;
                         }};
        dungeon_action(g, t, d, a, sink, dir, member);
    }
    std::string said() const {
        std::string out;
        for (const auto &l : text) out += (out.empty() ? "" : " | ") + l;
        return out.empty() ? "(nothing)" : out;
    }
};
// The 4-bit-class table of DNGLOOK 0x0102-0x01e6 (fields and special walls aside).
const char *class_text(int raw) {
    switch (raw & 0xf0) {
    case 0x00: return "a passage.";
    case 0x10: return "an up ladder.";
    case 0x20: return "a down ladder.";
    case 0x30: return "a ladder.";
    case 0x40: return "a wooden chest.";
    case 0x50: return "a fountain.";
    case 0x60: return "a pit.";
    case 0x70: return "an open chest.";
    case 0x90: return "nothing of note.";
    case 0xb0: case 0xd0: return "a wall.";
    default: return "a heavy door.";
    }
}

void test_dungeon_look() {
    std::printf("E  the dungeon (L)ook: DNGLOOK.OVL 0x0000\n");
    int wrong = 0, prompts_wrong = 0;
    std::string first_wrong;
    for (int raw = 0; raw < 256; ++raw) {
        if ((raw & 0xf0) == 0x80 || (raw & 0xf0) == 0xc0) continue;
        Corridor c;
        c.put(3, 2, uint8_t(raw)); // ahead of a party facing north
        c.act(kLook, 0, 1);
        const std::string want = raw == 0x61 ? "a passage." : class_text(raw);
        if (c.text.size() != 2 || c.text[0] != "You see:" || c.text[1] != want) {
            ++wrong;
            if (first_wrong.empty()) first_wrong = std::to_string(raw) + ": " + c.said();
        }
        if ((c.drink_prompts == 1) != ((raw & 0xf0) == 0x50)) ++prompts_wrong;
    }
    check(wrong == 0, "E-C1 every raw byte outside the fields and special walls: \"You see:\" + its class text" +
                          (first_wrong.empty() ? "" : " (first wrong " + first_wrong + ")"));
    check(prompts_wrong == 0, "E-C1 only 0x50-0x5F raise \"Will you drink?\"");
    {
        const std::pair<int, const char *> exact[] = {{0x61, "a passage."}, {0x69, "a pit."}, {0x80, "A sleep field."},
                                                      {0x81, "A poison gas field."}, {0x82, "A wall of fire."},
                                                      {0x83, "An electric field."}, {0x84, "An energy field."},
                                                      {0x88, "An energy field."}, {0xd0, "a wall."}, {0xf3, "a heavy door."}};
        bool ok = true;
        std::string bad;
        for (const auto &e : exact) {
            Corridor c;
            c.put(3, 2, uint8_t(e.first));
            c.act(kLook, 0, 0);
            if (c.text.size() != 2 || c.text[1] != e.second) { ok = false; bad += " " + std::to_string(e.first) + "=" + c.said(); }
        }
        check(ok, "E-C2 the exact bytes: 0x61 a passage (the hidden pit fall), 0x69 a pit, fields 0x80-0x83 by name, 0x84/0x88 "
                  "the energy field, 0xD0 a wall (a secret door), 0xF3 a heavy door" + bad);
    }
    {
        Corridor v1(0x22), v2(0x26), v3(0x21);
        for (auto *c : {&v1, &v2, &v3}) c->put(3, 2, 0xc0);
        v1.act(kLook, 0, 0);
        v2.act(kLook, 0, 0);
        const auto seed = v3.g.rng.get_seed();
        v3.act(kLook, 0, 0);
        OriginalRng once;
        once.seed(seed);
        const int roll = once.next(1, 255).value;
        check(v1.text.size() == 2 && v1.text[1] == "a dripping stalactite." && v2.text.size() == 2 &&
                  v2.text[1] == "a caved in passage.",
              "E-C3 a special wall: variant 1 the stalactite, variant 2 the caved-in passage, with no draw");
        check(v3.text.size() == 2 && v3.g.rng.get_seed() == once.get_seed() &&
                  v3.text[1] == (roll == 255 ? "an unfortunate software pirate." : "a less fortunate adventurer."),
              "E-C3 variant 3 (Deceit) draws rand(1,255) once: 255 the pirate, else the adventurer (" + v3.said() + ")");
    }
    {
        Corridor c;
        c.d.pos.x = 0;
        c.d.pos.y = 0;
        c.put(0, 7, 0x10); // ahead, wrapped
        c.put(0, 0, 0x20); // here
        c.put(7, 0, 0x30); // left (west, wrapped)
        c.put(1, 0, 0x40); // right
        std::string got;
        for (int dir = 0; dir < 4; ++dir) {
            c.act(kLook, dir, 0);
            got += c.text.size() == 2 ? c.text[1] : c.said();
            got += dir < 3 ? " / " : "";
        }
        check(got == "an up ladder. / a down ladder. / a ladder. / a wooden chest.",
              "E-C4 Dir- 0 Ahead / 1 Here / 2 Left / 3 Right, wrapped &7 like SJOG 0x002a + DNGLOOK 0x0046: " + got);
    }
    {
        Corridor c;
        c.g.torch_turns = 0;
        c.t.light_spell_minutes = 0;
        c.put(3, 3, 0x51);
        c.act(kLook, 1, 0);
        check(c.said() == "You see:\ndarkness." && c.drink_prompts == 0,
              "E-C5 no torch, no light: \"You see:\\ndarkness.\" and no question, even on a fountain (0x0013)");
        c.act(kLook, kNoDir, 0);
        check(c.said() == "You see:\ndarkness.", "E-C5 the darkness line comes whatever the direction (no Dir- is asked)");
    }
    {
        Corridor c;
        c.put(3, 3, 0x51);
        c.act(kLook, 1, -1);
        check(c.text.empty() && c.drink_prompts == 0, "E-C6 member -1 (the picker's \"None!\"): the Look prints nothing more");
        c.act(kLook, kPass, 0);
        check(c.text.empty(), "E-C6 the Dir- Pass: nothing more");
    }
    {
        Corridor c;
        c.put(3, 3, 0x52);
        c.act(kLookDrink, 1, 2);
        check(c.said() == "Poisoned!" && c.g.party.characters[2].status == 'P' && c.g.party.characters[0].status == 'G' &&
                  c.g.party.characters[1].status == 'G',
              "E-C7 0x52 poisons the member picked at Player: (DNGLOOK 0x0257 indexes [bp-0xc]), nobody else");
        c.put(3, 3, 0x50);
        c.act(kLookDrink, 1, 2);
        check(c.said() == "Cured!" && c.g.party.characters[2].status == 'G', "E-C7 0x50 cures the same member");
        c.put(3, 3, 0x51);
        c.act(kLookDrink, 1, 1);
        check(c.said() == "Healed!" && c.g.party.characters[1].current_hp == 90 && c.g.party.characters[0].current_hp == 40,
              "E-C7 0x51 heals the picked member to full");
        c.put(3, 3, 0x59);
        const int hp = c.g.party.characters[2].current_hp;
        c.act(kLookDrink, 1, 2);
        check(c.said() == "Bad taste." && c.g.party.characters[2].current_hp <= hp && c.g.party.characters[2].current_hp >= hp - 7,
              "E-C7 0x59 (a good fountain's bit-0x08 copy) is \"Bad taste.\": the WHOLE byte is compared (0x020d)");
        c.act(kLookDrink, 1, -1);
        check(c.text.empty(), "E-C7 \"No.\" (member -1): no effect");
    }
}

// --- Candidate B (P2): a paid tavern rumour is shown ---------------------------------------------
//   SHOPPES2 0x0612-0x0659: after "Fair 'nuff?" Y and the payment, the gossip form drawn
//   rand(0,3) -- records 85..88, '&' the subject, '*' the place -- then "\nsays " + keeper +
//   ".\n\n" (DS 0x9F82 / 0x9F8A). Native drew the form and printed nothing at all.
const char *kForms[4] = {"Form0: & in *.", "Form1: & in *.", "Form2: & in *.", "Form3: & in *."};
std::vector<std::string> paid_rumour(uint32_t seed, GameState &g) {
    g = GameState{};
    TurnState t{};
    TravelState travel{};
    CommandState commands{};
    WorldData world{};
    CommandContext c{g, t, travel, commands, world};
    g.position.map.location = 2;
    g.party.character_count = g.party.party_size = 1;
    g.party.characters[0].status = 'G';
    g.party.characters[0].intelligence = 15;
    g.gold = 500;
    g.rng.seed(seed);
    ShopRecord record{};
    record.keeper = "Greyson";
    record.name = "The Blue Boar";
    record.type = ShopType::Barkeeper;
    ShopSession s{};
    s.owner = &g;
    s.record = &record;
    s.type = ShopType::Barkeeper;
    s.location = 2;
    s.phase = ShopPhase::RumorDeal;
    s.item = 0; // "Malik", gossip place 0 ("Moonglow"), 50 gold
    s.price = 50;
    ShopData data{};
    ShopServices v{s, data};
    v.record_present = [](void *, int32_t) { return true; };
    v.record = [](void *, int32_t i) -> const char * { return i >= 85 && i <= 88 ? kForms[i - 85] : nullptr; };
    c.shop_services = &v;
    std::vector<std::string> lines;
    c.events = {&lines, [](void *p, const GameEvent &e) {
                    auto &out = *static_cast<std::vector<std::string> *>(p);
                    if (e.kind == GameEventKind::Message && e.text) out.emplace_back(e.text);
                }};
    ShopInput yes{};
    yes.action = ShopAction::Confirm;
    execute_shop(c, yes);
    return lines;
}

void test_rumour() {
    std::printf("B  a paid tavern rumour is shown (P2)\n");
    GameState g{};
    const auto lines = paid_rumour(5, g);
    bool gossip = false, says = false;
    std::string all;
    for (const auto &l : lines) {
        gossip |= l.find(": Malik in Moonglow.") != std::string::npos && l.rfind("Form", 0) == 0;
        says |= l.find("says Greyson.") != std::string::npos;
        all += (all.empty() ? "" : " | ") + l;
    }
    check(g.gold == 450 && gossip && says, "P2 the paid rumour prints its drawn form with the subject and place, then "
                                           "\"says Greyson.\": " + (all.empty() ? std::string("(nothing)") : all));
    // SHOPPES2 0x0636-0x0648: the form is record 85 + rand(0,3), so it follows the draw.
    std::string forms;
    for (uint32_t seed = 1; seed <= 24; ++seed) {
        GameState h{};
        for (const auto &l : paid_rumour(seed, h))
            if (l.rfind("Form", 0) == 0 && l.size() > 4 && forms.find(l[4]) == std::string::npos) forms += l[4];
    }
    std::sort(forms.begin(), forms.end());
    check(forms == "0123", "P2 the gossip form is drawn per rumour (forms seen over 24 seeds: " + forms + ")");
}
} // namespace

int main() {
    test_rumour();
    test_jimmy();
    test_push();
    test_yell();
    test_dungeon_look();
    std::printf("SPP1 core: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
