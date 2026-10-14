// Shared Preservation Parity Batch 1 (docs/SHARED_PRESERVATION_PARITY_SPP1.md) -- the
// corrections that live in the shared runtime, driven through the REAL AlphaRuntime on the REAL
// tdeck_board.cpp (the A4-UI2 harness). Desktop keys arrive the way native/targets/windows sends
// them (AlphaRuntime::handle_action); T-Deck keys as raw matrix keys.
//
//   G  the context loops' control keys (MAINOUT 0x0b14 / table 0x0bbe, TOWN table 0x1552,
//      DUNGEON 0x06c4; re/notes/teclas-control-karma-derivacion.md, re-derived for SPP1):
//      G1 Ctrl-K (code 0x0B) on the surface prints the karma as a bare decimal (kernel 0x1a3e,
//         pad ' ', width 1) on the command row and costs nothing (MAINOUT 0x0af8: [bp-8]=0)
//      G2 Ctrl-V (code 0x16) prints "1.16" (DS 0x2b4f) and costs nothing
//      G3 the same two keys in a town cost nothing (TOWN 0x14ff -> 0x154b: [bp-0xa]=0)
//      G4 in a dungeon the handler keeps its default return of 1 (DUNGEON 0x06cb), so the
//         dungeon's turn block (0x0c76: the wanderer, housekeeping K:0x2ae8) runs once
//      G5 control: a T-Deck raw key never produces either code (the matrix has no Ctrl)
//
//   spp1_runtime <openu5-alpha1-resources.bin>
#include "a4_ui2_harness.h"
#include "openu5/combat.h"
#include "openu5/debug_developer.h"
#include "openu5/outdoor.h"
#include "openu5/platform_profile.h"
#include "openu5/quest_state.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shops.h"
#include "openu5/world.h"

using namespace a4_ui2;

namespace {
const std::vector<Member> kParty = {{"Avatar", 'G', 200}, {"Iolo", 'G', 200}, {"Shamino", 'G', 200}};
constexpr int kDeceit = 33;

UiAction ch(char16_t c) { UiAction a; a.kind = UiActionKind::Character; a.character = c; return a; }

/** One desktop input: handle_action, as native/targets/windows routes a key, then 100 ms of frames. */
bool act(Run &h, const UiAction &a) {
    openu5_host_virtual_clock_us() += 100000;
    const bool routed = h.rt->handle_action(a, tdeck::DeviceShortcut::None, Run::now(), "spp1_runtime");
    h.run(100);
    return routed;
}

struct Snap {
    int64_t turns;
    uint32_t seed;
    int day, hour, minute;
    uint16_t food;
    uint8_t dx, dy, df;
    int px, py, loc;
};
Snap snap(Run &h) {
    auto &g = h.rt->game();
    Snap s{};
    s.turns = g.turns_since_start;
    s.seed = g.rng.get_seed();
    s.day = g.time.day;
    s.hour = g.time.hour;
    s.minute = g.time.minute;
    s.food = g.food;
    const auto &d = h.rt->dungeon_state();
    s.dx = d.pos.x;
    s.dy = d.pos.y;
    s.df = d.pos.floor;
    s.px = g.position.xy.x;
    s.py = g.position.xy.y;
    s.loc = g.position.map.location;
    return s;
}
int minutes(const Snap &s) { return (s.day * 24 + s.hour) * 60 + s.minute; }
std::string diff(const Snap &a, const Snap &b) {
    std::string r;
    if (a.turns != b.turns) r += " turns " + n(a.turns) + "->" + n(b.turns);
    if (a.seed != b.seed) r += " seed changed";
    if (minutes(a) != minutes(b)) r += " clock +" + n(minutes(b) - minutes(a)) + "min";
    if (a.food != b.food) r += " food " + n(a.food) + "->" + n(b.food);
    if (a.dx != b.dx || a.dy != b.dy || a.df != b.df) r += " dungeon-cell";
    if (a.px != b.px || a.py != b.py || a.loc != b.loc) r += " position";
    return r.empty() ? " (none)" : r;
}
bool unchanged(const Snap &a, const Snap &b) { return std::memcmp(&a, &b, sizeof a) == 0; }

/** The transcript holds a block whose text is exactly `line`. */
size_t lines_equal(Run &h, const std::string &line) {
    size_t k = 0;
    for (size_t i = 0; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) k += line == b->text;
    return k;
}
const UiTextBlock *last_block(Run &h) {
    const size_t n = h.rt->ui()->transcript_size();
    return n ? h.rt->ui()->transcript_at(n - 1) : nullptr;
}

void surface(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {80, 80};
    h.run(50);
}
void enter_deceit(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[kDeceit - 1], pack->location_y[kDeceit - 1]};
    set_quest_flag(g.quest, QuestFlag::Word33);
    h.key('e');
}
bool in_dungeon(Run &h) { return h.rt->dungeon_state().active && h.rt->command_context_for_test().dungeon; }
/** The party at Britain's gate (location 2 in the location table), then (E)nter. */
void enter_town(Run &h, int location) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[location - 1], pack->location_y[location - 1]};
    h.key('e');
    h.run(200);
}

void test_g() {
    std::printf("G  Ctrl-K / Ctrl-V: the context loops' control keys\n");
    {
        Run h(kParty);
        h.rt->game().karma = 73;
        surface(h);
        const Snap a = snap(h);
        act(h, ch(0x0b));
        const Snap b = snap(h);
        const auto *last = last_block(h);
        check(lines_equal(h, "73") == 1 && last && std::string(last->text) == "73" && last->channel == UiTextChannel::CommandEcho,
              "G1 Ctrl-K on the surface: \"73\" alone on the command row (the karma, bare decimal)");
        check(unchanged(a, b), "G1 Ctrl-K on the surface costs no turn (MAINOUT 0x0af8):" + diff(a, b));
        act(h, ch(0x16));
        const Snap c = snap(h);
        last = last_block(h);
        check(last && std::string(last->text) == "1.16" && last->channel == UiTextChannel::CommandEcho,
              "G2 Ctrl-V prints \"1.16\" (DS 0x2b4f) on the command row");
        check(unchanged(b, c), "G2 Ctrl-V costs no turn:" + diff(b, c));
        check(lines_equal(h, "What?") == 0, "G1/G2 neither key is \"What?\"");
    }
    {
        Run h(kParty);
        h.rt->game().karma = 41;
        enter_town(h, 2);
        check(h.rt->game().position.map.location == 2, "G3 control: (E)nter at the gate puts the party in town 2");
        const Snap a = snap(h);
        act(h, ch(0x0b));
        act(h, ch(0x16));
        const Snap b = snap(h);
        check(lines_equal(h, "41") == 1 && lines_equal(h, "1.16") == 1, "G3 in a town: \"41\" and \"1.16\"");
        check(unchanged(a, b), "G3 in a town neither key costs a turn (TOWN 0x154b):" + diff(a, b));
    }
    {
        Run h(kParty);
        h.rt->game().karma = 88;
        enter_deceit(h);
        check(in_dungeon(h), "G4 control: (E)nter at Deceit's mouth opens the dungeon session");
        const Snap a = snap(h);
        act(h, ch(0x0b));
        const Snap b = snap(h);
        check(lines_equal(h, "88") == 1, "G4 in a dungeon Ctrl-K prints \"88\"");
        check(b.turns == a.turns + 1 && minutes(b) == minutes(a) + 1 && b.dx == a.dx && b.dy == a.dy,
              "G4 in a dungeon Ctrl-K costs exactly one dungeon turn (DUNGEON 0x06cb returns 1 -> 0x0c76):" + diff(a, b));
        act(h, ch(0x16));
        const Snap c = snap(h);
        check(lines_equal(h, "1.16") == 1 && c.turns == b.turns + 1, "G4 Ctrl-V likewise costs one dungeon turn:" + diff(b, c));
    }
    {
        Run h(kParty);
        surface(h);
        const size_t before = h.rt->ui()->transcript_size();
        const Snap a = snap(h);
        // Every printable key and the control row of the T-Deck matrix: none is 0x0B or 0x16.
        h.key('k', true);
        h.key('v', true);
        const Snap b = snap(h);
        bool any = false;
        for (size_t i = before; i < h.rt->ui()->transcript_size(); ++i)
            if (const auto *bl = h.rt->ui()->transcript_at(i)) any |= std::string(bl->text) == "0" || std::string(bl->text) == "1.16";
        check(!any && unchanged(a, b), "G5 control: the T-Deck's Alt+K / Alt+V do not reach the control keys:" + diff(a, b));
    }
}
// --- Candidate F: cancelled healing spells (CAST.OVL 0x0dba) ---------------------------------
//   The one Cast routine gates in this order -- location (0x0e1a), None mixed (0x0eb8), the charge
//   spent (0x0ec8 dec), MP (0x0ecc), the MP spent (0x0ef8 sub), level (0x0f01) -- and only then
//   jumps to the spell's arm (0x0f1a), which asks "On who: " (CAST2 0x009e). ESC there returns -1
//   from the arm: no "Success!"/"Failed!" (tail 0x11a6), and the charge and MP stay spent. In the
//   arena COMBAT 0x0923 has already ended the actor's turn. (U)se spends the potion at CAST 0x136a
//   and the In Mani Corp scroll at 0x11ec, both before "On who: ".
//      F1 a world Mani cancelled at "On who": the charge and the caster's MP are spent, silently
//      F2 the member chosen at "Player:" pays (CAST 0x0ef8 subtracts from [bp-4], the 0x0dd5 pick)
//      F3 a cast the gates refuse never opens "On who:": M.P. too low! / Failed! (charge spent),
//         the level gate's Failed! (charge and MP spent)
//      F4 (U)se In Mani Corp scroll / a potion, "On who:" cancelled: the item is spent (D-51 retired)
//      F5 the arena: the cancelled Mani spends and ends the actor's turn exactly once (both modes)
//      F6 the 1988 model's rows: "On who: None!", no "Failed!"
bool g_dos = false;
struct DosProfile {
    DosProfile() {
        g_dos = true;
        PlatformProfile p{};
        p.back_key = "Esc";
        p.move_keys = "Arrows";
        p.menu_key = "F10";
        p.typed_spell_names = [](void *) { return g_dos; };
        p.quit_save_key = [](void *) { return g_dos; };
        p.dos_interface = [](void *) { return g_dos; };
        set_platform_profile(p);
    }
    ~DosProfile() {
        g_dos = false;
        set_platform_profile(PlatformProfile{});
    }
    DosProfile(const DosProfile &) = delete;
    DosProfile &operator=(const DosProfile &) = delete;
};
UiAction kind(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction select(int32_t i) { UiAction a; a.kind = UiActionKind::SelectIndex; a.index = i; return a; }
UiMode mode(Run &h) { return h.rt->ui()->mode(); }
UiRequestId request(Run &h) { return h.rt->ui()->request(); }
bool transcript_has(Run &h, const char *needle) { return h.transcript().find(needle) != std::string::npos; }
constexpr int kMani = 4, kVasMani = 27;

/** Three able members, none active; only `spell` mixed (2), every member MP 20 at `level`. */
void caster_party(Run &h, int spell, int level = 3) {
    auto &g = h.rt->game();
    for (auto &q : g.spell_quantities) q = 0;
    g.spell_quantities[spell] = 2;
    for (int i = 0; i < 3; ++i) {
        g.party.characters[i].current_mp = 20;
        g.party.characters[i].level = uint8_t(level);
    }
    g.party.characters[2].current_hp = 150;
}

// --- the arena: A3-HF3's troll fight (a4_ui2_death_music_runtime_test.cpp) ------------------
CombatState &cs(Run &h) { return h.rt->combat_state_for_test(); }
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
bool in_combat(Run &h) { return ctx(h).combat && cs(h).initialized; }
int troll_tile(CommandContext &c) {
    for (size_t i = 0; i < c.outdoor->resources->enemy_count; ++i)
        if (const auto *d = c.outdoor->resources->enemies[i]; d && d->index == 41) return d->tile;
    return -1;
}
bool inland_grass(int &gx, int &gy) {
    const auto m = get_active_map(pack->world, MapId{LocationId(0), 0});
    if (m.error != Error::None) return false;
    for (int y = 60; y < 200; ++y)
        for (int x = 60; x < 200; ++x) {
            bool ok = true;
            for (int dy = -1; dy <= 1 && ok; ++dy)
                for (int dx = -2; dx <= 2 && ok; ++dx) ok = m.value.tile_at(x + dx, y + dy) == 5;
            if (ok) { gx = x; gy = y; return true; }
        }
    return false;
}
CombatActor *player_turn(Run &h) {
    auto &s = cs(h);
    if (!in_combat(h) || s.current < 0 || s.current >= s.count) return nullptr;
    auto &a = s.actors[s.current];
    if (a.enemy || a.member == 255 || a.status != CombatStatus::Active) return nullptr;
    return mode(h) == UiMode::Combat ? &a : nullptr;
}
/** Waits (frames) until a party member has the turn; the enemy is put to sleep far away. */
CombatActor *await_player(Run &h) {
    for (int t = 0; t < 20000 && in_combat(h) && !player_turn(h); t += 5) h.run(5);
    return player_turn(h);
}
bool enter_arena(Run &h) {
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx;
    r.y = gy;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    h.render(true);
    auto &c = ctx(h);
    c.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(c);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    c.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    auto &s = cs(h);
    for (int i = 0; i < s.count; ++i)
        if (s.actors[i].enemy) {
            s.actors[i].position = {10, 0};
            s.actors[i].sleeping = true;
            s.actors[i].hp = s.actors[i].max_hp = 250;
        }
    return in_combat(h) && await_player(h);
}

struct CastRun {
    int qty = -1, mp_caster = -1, mp_other = -1;
    UiMode after = UiMode::Exploration;
    bool success = false, failed = false, none_row = false, asked_target = false;
};
/** (C)ast on the surface: Player: `caster`, the only spell, then "On who:" `target` (-1 = Cancel). */
CastRun world_cast(Run &h, int spell, int caster, int target) {
    act(h, ch('c'));
    if (mode(h) == UiMode::PartySelection) act(h, select(caster));
    if (mode(h) == UiMode::SpellSelection) act(h, select(0));
    CastRun r;
    r.asked_target = mode(h) == UiMode::PartySelection && request(h) == UiRequestId::Target;
    if (r.asked_target) act(h, target >= 0 ? select(target) : kind(UiActionKind::Cancel));
    auto &g = h.rt->game();
    r.qty = g.spell_quantities[spell];
    r.mp_caster = g.party.characters[caster].current_mp;
    r.mp_other = g.party.characters[caster ? 0 : 1].current_mp;
    r.after = mode(h);
    r.success = transcript_has(h, "Success!");
    r.failed = transcript_has(h, "Failed!");
    r.none_row = transcript_has(h, "On who: None!");
    return r;
}

void test_f() {
    std::printf("F  cancelled healing spells: CAST.OVL spends before \"On who:\"\n");
    {
        Run h(kParty);
        surface(h);
        caster_party(h, kMani);
        const auto r = world_cast(h, kMani, 1, -1);
        check(r.asked_target && r.after == UiMode::Exploration, "F1 control: \"On who:\" opens, and the cancel returns to the world");
        check(r.qty == 1, "F1 the cancelled Mani's charge is spent (CAST 0x0ec8): qty " + n(r.qty));
        check(r.mp_caster == 19 && r.mp_other == 20, "F1/F2 the Player: pick (Iolo) pays one MP, nobody else: Iolo " + n(r.mp_caster) + ", Avatar " + n(r.mp_other));
        check(!r.success && !r.failed, "F1 the cancel prints neither Success! nor Failed! (the arm returns -1)");
    }
    {
        Run h(kParty);
        surface(h);
        caster_party(h, kMani);
        const auto r = world_cast(h, kMani, 1, 2);
        check(r.mp_caster == 19 && r.mp_other == 20 && r.success,
              "F2 a completed Mani is paid by the Player: pick, not the active/first member: Iolo " + n(r.mp_caster) + ", Avatar " + n(r.mp_other));
    }
    {
        Run h(kParty);
        surface(h);
        caster_party(h, kMani);
        h.rt->game().party.characters[1].current_mp = 0;
        const auto r = world_cast(h, kMani, 1, -1);
        check(!r.asked_target && r.after == UiMode::Exploration && transcript_has(h, "M.P. too low!") && r.failed && r.qty == 1 && r.mp_caster == 0,
              "F3 MP 0: no \"On who:\" is asked; M.P. too low! / Failed!, the charge spent, MP kept (0x0ecc)");
    }
    {
        Run h(kParty);
        surface(h);
        caster_party(h, kVasMani, 1);
        const auto r = world_cast(h, kVasMani, 1, -1);
        check(!r.asked_target && r.after == UiMode::Exploration && r.failed && r.qty == 1 && r.mp_caster == 15,
              "F3 level 1 casting circle 5: no \"On who:\"; Failed!, the charge and 5 MP spent (0x0f01): qty " + n(r.qty) + " mp " + n(r.mp_caster));
    }
    for (int item : {6, 8}) {
        Run h(kParty);
        surface(h);
        auto &g = h.rt->game();
        // The fixture's new game carries potions and scrolls: only the one under test stays.
        for (auto &q : g.scroll_quantities) q = 0;
        for (auto &q : g.potion_quantities) q = 0;
        if (item == 6) g.scroll_quantities[6] = 1;
        else g.potion_quantities[0] = 1;
        act(h, ch('u'));
        act(h, select(0));
        const bool asked = mode(h) == UiMode::PartySelection && request(h) == UiRequestId::UseTarget;
        act(h, kind(UiActionKind::Cancel));
        const int left = item == 6 ? g.scroll_quantities[6] : g.potion_quantities[0];
        check(asked && left == 0, std::string("F4 (U)se ") + (item == 6 ? "In Mani Corp scroll" : "a potion") +
                                      ", \"On who:\" cancelled: the item is spent (CAST " + (item == 6 ? "0x11ec" : "0x136a") + ")");
    }
    for (int dos = 0; dos < 2; ++dos) {
        std::unique_ptr<DosProfile> profile;
        if (dos) profile = std::make_unique<DosProfile>();
        const char *tag = dos ? " [Original]" : " [T-Deck]";
        // Baseline: the same fight with a Pass; the next party member to act is the one after it.
        int baseline_next = -1;
        {
            Run b(kParty);
            if (!enter_arena(b)) { check(false, std::string("F5 control: the troll arena opens") + tag); continue; }
            act(b, ch(' '));
            if (auto *n2 = await_player(b)) baseline_next = n2->member;
        }
        Run h(kParty);
        enter_arena(h);
        auto *actor = player_turn(h);
        const int caster = actor ? actor->member : -1;
        caster_party(h, kMani);
        act(h, ch('c'));
        if (mode(h) == UiMode::TextEntry) { // Original: the typed name
            for (char16_t c : {u'm'}) act(h, ch(c));
            act(h, kind(UiActionKind::Confirm));
        } else if (mode(h) == UiMode::SpellSelection) act(h, select(0));
        const bool asked = mode(h) == UiMode::PartySelection && request(h) == UiRequestId::Target;
        act(h, kind(UiActionKind::Cancel));
        auto &g = h.rt->game();
        auto *next = await_player(h);
        check(asked && caster >= 0, std::string("F5 control: \"On who:\" opens for the actor's Mani") + tag);
        check(g.spell_quantities[kMani] == 1 && g.party.characters[size_t(caster)].current_mp == 19,
              std::string("F5 the arena's cancelled Mani is spent: qty ") + n(g.spell_quantities[kMani]) + " mp " +
                  n(g.party.characters[size_t(caster)].current_mp) + tag);
        check(next && next->member == baseline_next,
              std::string("F5 the cancel ends the actor's turn exactly once (next to act ") + n(next ? next->member : -1) +
                  ", after a Pass " + n(baseline_next) + ")" + tag);
        if (dos) check(transcript_has(h, "On who: None!") && !transcript_has(h, "Failed!"), "F6 Original: \"On who: None!\" and no Failed!");
    }
}
// --- Candidate A: the kernel dispatcher's turn (spp1-findings/A-turn-costs.md) ---------------
//   K:0x3178 returns 1 -- one world turn for the context loop -- on EVERY path of (R)eady 0x339a,
//   (Z)-stats 0x3472, (N)ew order 0x334e, (U)se 0x340c, (C)ast 0x3242 (CAST 0x0dba always
//   returns [bp-6] = 1) and (M)ix, and of the getdir commands L/G/O/J/S/P/F/T/X, a Space `Pass`
//   included (getdir 0x35EC has no ESC; its only "no direction" is Space). Overworld A returns 0
//   (MAINOUT 0x06ec); town A returns 1 except "On foot!". MAINOUT 0x0c39 then runs advance_clock(2)
//   and the turn block, TOWN 0x15d0 advance_clock(1); DUNGEON 0x0f84 the turn block 0x0c76.
//      A1  R: Cancel at Player:, and two items equipped then the list closed -- one turn each
//      A2  Z: Cancel at Player:, and the pages browsed and closed -- one turn each
//      A3  N: Cancel at Swap, and a completed swap -- one turn each
//      A4  U: Cancel at the list, and a potion drunk -- one turn each
//      A5  C: Cancel at Player:, and a completed Mani -- one turn each; M: Cancel -- one turn
//      A6  the getdir: Look / Get cancelled (the T-Deck's Cancel is the binary's Pass), a
//          completed Look -- one turn each; Search, whose core command already charges its
//          turn, still costs exactly one
//      A7  the town: one turn of one minute (TOWN 0x15d0) for R and for a Look-Pass
//      A8  the dungeon: R / U / Z cost one dungeon turn (DUNGEON 0x0f84 -> 0x0c76)
//      A9  Original: Space at Look- prints "Look-Pass" and costs one turn
//      A10 controls that stay free: overworld Attack cancelled (MAINOUT 0x06ec), D ("D-What?"),
//          Ctrl-K, a valid digit; and Ignite / Pass, charged by the core, cost one, not two
UiAction dir(Direction d) { UiAction a; a.kind = UiActionKind::Direction; a.direction = d; return a; }
bool beside(Run &h, int location, int slot, int hour, Direction &toward);
struct Cost { int64_t turns; int minutes; };
template <class F> Cost cost(Run &h, F &&f) {
    const Snap a = snap(h);
    f();
    const Snap b = snap(h);
    return {b.turns - a.turns, minutes(b) - minutes(a)};
}
std::string show(const Cost &c) { return " (turns +" + n(c.turns) + ", clock +" + n(c.minutes) + " min)"; }
bool one_world(const Cost &c) { return c.turns == 1 && c.minutes == 2; }
void equip_two(Run &h) {
    auto &g = h.rt->game();
    for (auto &q : g.equipment_quantities) q = 0;
    g.equipment_quantities[0] = 1;  // Leather Helm
    g.equipment_quantities[16] = 1; // Dagger
}

void test_a() {
    std::printf("A  the dispatcher's turn: R / Z / N / U / C / M and the getdir, every path\n");
    {
        Run h(kParty);
        surface(h);
        equip_two(h);
        auto c1 = cost(h, [&] { act(h, ch('r')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c1), "A1 R cancelled at Player: costs one world turn" + show(c1));
        auto c2 = cost(h, [&] {
            act(h, ch('r'));
            act(h, select(0));
            act(h, select(0));
            act(h, select(1));
            act(h, kind(UiActionKind::Cancel));
        });
        check(one_world(c2), "A1 R with two items readied, then closed, costs ONE world turn" + show(c2));
        auto c3 = cost(h, [&] { act(h, ch('z')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c3), "A2 Z cancelled at Player: costs one world turn" + show(c3));
        bool viewer_open = false, viewer_closed = false;
        auto c4 = cost(h, [&] {
            act(h, ch('z'));
            act(h, select(1));
            viewer_open = h.rt->transient_probe_for_test().zstats; // the page viewer is the runtime's, not a UiMode
            act(h, dir(Direction::East));                           // turn a page
            for (int i = 0; i < 3 && h.rt->transient_probe_for_test().zstats; ++i) act(h, kind(UiActionKind::Cancel));
            viewer_closed = !h.rt->transient_probe_for_test().zstats;
        });
        check(viewer_open && viewer_closed && one_world(c4), "A2 Z browsed and closed costs one world turn" + show(c4));
        auto c5 = cost(h, [&] { act(h, ch('n')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c5), "A3 N cancelled at Swap costs one world turn" + show(c5));
        auto c6 = cost(h, [&] { act(h, ch('n')); act(h, select(1)); act(h, select(2)); });
        check(one_world(c6) && std::string(h.rt->game().party.characters[1].name) == "Shamino",
              "A3 a completed swap (Iolo <-> Shamino) costs one world turn" + show(c6));
        auto &g = h.rt->game();
        for (auto &q : g.scroll_quantities) q = 0;
        for (auto &q : g.potion_quantities) q = 0;
        g.potion_quantities[0] = 2;
        auto c7 = cost(h, [&] { act(h, ch('u')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c7), "A4 U cancelled at the list costs one world turn" + show(c7));
        auto c8 = cost(h, [&] { act(h, ch('u')); act(h, select(0)); act(h, select(0)); });
        check(one_world(c8) && g.potion_quantities[0] == 1, "A4 U a potion drunk costs one world turn" + show(c8));
        caster_party(h, kMani);
        auto c9 = cost(h, [&] { act(h, ch('c')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c9) && g.spell_quantities[kMani] == 2, "A5 C cancelled at Player: costs one world turn, nothing spent" + show(c9));
        auto c10 = cost(h, [&] { world_cast(h, kMani, 1, 2); });
        check(one_world(c10) && g.spell_quantities[kMani] == 1, "A5 C a completed Mani costs one world turn" + show(c10));
        auto c11 = cost(h, [&] { act(h, ch('m')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c11), "A5 M cancelled at the spell costs one world turn" + show(c11));
        auto c12 = cost(h, [&] { act(h, ch('l')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c12), "A6 Look cancelled at the direction (the binary's Pass) costs one world turn" + show(c12));
        auto c13 = cost(h, [&] { act(h, ch('g')); act(h, kind(UiActionKind::Cancel)); });
        check(one_world(c13), "A6 Get cancelled at the direction costs one world turn" + show(c13));
        auto c14 = cost(h, [&] { act(h, ch('l')); act(h, dir(Direction::North)); });
        check(one_world(c14), "A6 a completed Look costs one world turn (K:0x3310 -> 0x333c returns 1)" + show(c14));
        auto c15 = cost(h, [&] {
            act(h, ch('s'));
            act(h, dir(Direction::North));
            if (mode(h) == UiMode::PartySelection) act(h, select(0));
        });
        check(one_world(c15), "A6 Search, which charges its own turn in the core, still costs exactly one" + show(c15));
        // A10 controls
        auto z1 = cost(h, [&] { act(h, ch('a')); act(h, kind(UiActionKind::Cancel)); });
        check(z1.turns == 0 && z1.minutes == 0, "A10 overworld Attack cancelled stays free (MAINOUT 0x06ec returns 0)" + show(z1));
        auto z2 = cost(h, [&] { act(h, ch('d')); });
        check(z2.turns == 0 && z2.minutes == 0, "A10 D (\"D-What?\") stays free (K:0x324e)" + show(z2));
        auto z3 = cost(h, [&] { act(h, ch(0x0b)); });
        check(z3.turns == 0, "A10 Ctrl-K stays free on the surface" + show(z3));
        auto z4 = cost(h, [&] { act(h, ch('1')); });
        check(z4.turns == 0, "A10 a valid digit stays free" + show(z4));
        g.torches = 3;
        auto z5 = cost(h, [&] { act(h, ch('i')); });
        check(one_world(z5), "A10 Ignite (charged by the core) costs one turn, not two" + show(z5));
        auto z6 = cost(h, [&] { act(h, kind(UiActionKind::Confirm)); });
        check(one_world(z6), "A10 Pass costs one turn, not two" + show(z6));
    }
    {
        Run h(kParty);
        enter_town(h, 2);
        equip_two(h);
        auto c1 = cost(h, [&] { act(h, ch('r')); act(h, kind(UiActionKind::Cancel)); });
        check(c1.turns == 1 && c1.minutes == 1, "A7 town: R cancelled costs one turn of one minute (TOWN 0x15d0)" + show(c1));
        auto c2 = cost(h, [&] { act(h, ch('l')); act(h, kind(UiActionKind::Cancel)); });
        check(c2.turns == 1 && c2.minutes == 1, "A7 town: Look-Pass costs one turn of one minute" + show(c2));
    }
    {
        Run h(kParty);
        enter_deceit(h);
        auto c1 = cost(h, [&] { act(h, ch('r')); act(h, kind(UiActionKind::Cancel)); });
        check(in_dungeon(h) && c1.turns == 1 && c1.minutes == 1, "A8 dungeon: R cancelled costs one dungeon turn" + show(c1));
        auto c2 = cost(h, [&] { act(h, ch('u')); act(h, kind(UiActionKind::Cancel)); });
        check(c2.turns == 1 && c2.minutes == 1, "A8 dungeon: U cancelled costs one dungeon turn" + show(c2));
        auto c3 = cost(h, [&] { act(h, ch('z')); act(h, kind(UiActionKind::Cancel)); });
        check(c3.turns == 1 && c3.minutes == 1, "A8 dungeon: Z cancelled costs one dungeon turn" + show(c3));
    }
    {
        DosProfile dos;
        Run h(kParty);
        surface(h);
        auto c1 = cost(h, [&] { act(h, ch('l')); act(h, ch(' ')); });
        check(transcript_has(h, "Look-Pass") && one_world(c1), "A9 Original: Space at Look- prints \"Look-Pass\" and costs one world turn" + show(c1));
        auto c2 = cost(h, [&] { act(h, kind(UiActionKind::Confirm)); act(h, kind(UiActionKind::Cancel)); });
        check(c2.turns == 0, "A10 Original: RETURN / ESC at the prompt (\"What?\") stay free" + show(c2));
    }
    // A12 a command that ends on a TIMER, not a key (found by the code review). The device holds
    // "Mixing..." for 10 ticks (A4-UI4 D-71, the dialogue pacer), so the bare prompt comes back with no
    // input to settle on. The Mix's turn (K:0x3340 -> 1) is still charged exactly once, before the next
    // input runs: here a Pass, its own turn, so two turns in all (it was one -- the Pass swallowed it).
    {
        Run h(kParty, true); // paced, as the device runs
        surface(h);
        auto &g = h.rt->game();
        for (auto &r : g.reagent_quantities) r = 0;
        g.reagent_quantities[0] = 5; // Sulfur Ash
        g.spell_quantities[0] = 2;   // In Lor
        const Snap before = snap(h);
        h.key('m');
        h.key('\r'); // In Lor, row 0
        h.key('\r'); // mark Sulfur Ash, row 0
        h.key('m');  // mix
        h.key('1');
        h.key('\r');
        h.run(2000); // the "Mixing..." hold ends on the clock
        const bool mixed = g.spell_quantities[0] == 3 && g.reagent_quantities[0] == 4;
        h.key('\r'); // Pass
        h.run(200);
        const Snap after = snap(h);
        const Cost c{after.turns - before.turns, minutes(after) - minutes(before)};
        check(mixed && c.turns == 2 && c.minutes == 4,
              "A12 paced: a Mix whose prompt returns on the pacer's clock, then a Pass, costs two world turns" + show(c));
    }
    // A13 the map reveal swallows every key (R-12, route_input_body): a swallowed letter is no command and
    // must charge nothing (it charged a full world turn per key).
    {
        Run h(kParty);
        surface(h);
        GameEvent reveal{};
        reveal.kind = GameEventKind::MapReveal;
        reveal.note = 40; // 2.2 s
        auto &ctx = h.rt->command_context_for_test();
        ctx.events.emit(ctx.events.context, reveal);
        const bool active = h.rt->transient_probe_for_test().map_reveal;
        auto c = cost(h, [&] { h.key('z'); h.key('l'); });
        check(active && c.turns == 0 && c.minutes == 0, "A13 Z and L pressed during the map reveal are swallowed and cost nothing" + show(c));
    }
    // A11 a town conversation is TALK's own time. TALK 0x041c returns nonzero after a conversation and the
    // dispatcher then returns 2, whose handling by the town loop is UNRESOLVED (spp1-findings/A-turn-costs.md):
    // SPP1 leaves the conversation's cost as it was (none). The probe found the device's conversation runs in
    // TextEntry (request Dialogue), never UiMode::Dialogue, so the latch has to read the session itself.
    // A11b CONTROL, from the binary: Talk at an empty town cell is TALK 0x048a (no NPC) -> 0x0491 prints
    // DS 0x91d6 "Nobody's here!" -> 0x0429 returns 0 -> the dispatcher's 1 -> one town turn. Native still
    // prints the overworld's "Funny, no response!" there (fidelity matrix §7 item 5, ledger D-111), so the
    // text is NOT pinned here: only that no conversation opened and the turn was spent.
    for (int dos = 0; dos < 2; ++dos) {
        std::unique_ptr<DosProfile> profile;
        if (dos) profile = std::make_unique<DosProfile>();
        const std::string tag = dos ? " [Original]" : " [T-Deck]";
        const auto &npcs = pack->npc_locations[0];
        bool talked = false;
        for (size_t i = 0; i < npcs.count && !talked; ++i) {
            if (!npcs.slots[i].dialog || npcs.slots[i].dialog >= 0x81) continue;
            Run h(kParty);
            Direction toward{};
            if (!beside(h, 1, npcs.slots[i].slot, 9, toward)) continue;
            bool open = false;
            auto c = cost(h, [&] {
                act(h, ch('t'));
                act(h, dir(toward));
                const auto *ds = h.rt->command_context_for_test().dialogue_services;
                open = ds && ds->session.active;
                for (int k = 0; k < 12 && mode(h) != UiMode::Exploration; ++k) act(h, kind(UiActionKind::Confirm));
            });
            if (!open || !transcript_has(h, "You see")) continue;
            talked = true;
            check(c.turns == 0 && mode(h) == UiMode::Exploration,
                  "A11 a town conversation, ended with an empty answer, keeps its pre-SPP1 cost (none)" + tag + show(c));
        }
        if (!talked) check(false, "A11 no conversation could be opened in location 1" + tag);
        Run h(kParty);
        enter_town(h, 2);
        auto &g = h.rt->game();
        auto &cx = h.rt->command_context_for_test();
        int nx = -1, ny = -1;
        // SPP2: away from every actor, so the town turn's NPC tail brings no approach. SPP2 opens an
        // approach the turn brings at once (spp2_runtime N2), and the first cell this scan found, (2,2),
        // is beside Tika of the Wayfarer Tavern, who comes up to the party on that turn.
        const auto clear = [&](int x, int y) {
            for (size_t i = 0; i < cx.actors->count; ++i) {
                const auto &a = cx.actors->actors[i];
                if (a.location == g.position.map.location && std::abs(a.x - x) + std::abs(a.y - y) <= 4) return false;
            }
            return true;
        };
        for (int y = 2; y < 30 && nx < 0; ++y)
            for (int x = 2; x < 30 && nx < 0; ++x) {
                bool ok = is_passable(cx.terrain->effective(cx.world, g.position.map, x, y), TransportMode::Foot).value &&
                          is_passable(cx.terrain->effective(cx.world, g.position.map, x + 1, y), TransportMode::Foot).value &&
                          !cx.shop_services->occupied(cx.shop_services->context, x, y) &&
                          !cx.shop_services->occupied(cx.shop_services->context, x + 1, y) && clear(x, y);
                if (ok) { nx = x; ny = y; }
            }
        g.position.xy = {uint8_t(nx), uint8_t(ny)};
        auto c = cost(h, [&] { act(h, ch('t')); act(h, dir(Direction::East)); });
        const auto *ds = h.rt->command_context_for_test().dialogue_services;
        check(nx >= 0 && transcript_has(h, "Talk-") && !(ds && ds->session.active) && mode(h) == UiMode::Exploration &&
                  c.turns == 1 && c.minutes == 1,
              "A11b CONTROL: Talk at an empty town cell opens nothing and costs one town turn" + tag + show(c));
    }
}
// --- Candidate C: the arena's J / P / Y through the real runtime ----------------------------
//      C1 J / P / Y no longer answer "What?" in the arena (both modes)
//      C2 Jimmy opens the getdir with its arena name "Jimmy-"; without a key "No Keys!" comes
//         first and no direction is asked (SJOG 0x0d66), and the turn ends
//      C3 Original: Space at Jimmy- prints "Jimmy-Pass" and the turn ends (getdir 0x35EC)
//      C4 Push-East onto bare floor: "Won't budge!" and the turn ends
//      C5 Yell: Original "Yell what?" / ":" -- a word is "No effect!"; an empty answer
//         "Nothing"; both end the turn. The T-Deck's Yell bar the same
//      C6 the T-Deck's Cancel at Jimmy- stays free (the arena's handheld contract)
// --- Candidate E: the dungeon (L)ook through the real runtime --------------------------------
//      E1 L: "Look...", the "Player: " picker (three eligible, none active), Dir-, "You see:"
//         -- one dungeon turn (K:0x3310 returns 1; DUNGEON 0x0c76) and one minute
//      E2 a poison fountain Here, Y: the PICKED member is poisoned, nobody else; one turn in all
//      E3 the same fountain, N: "No.", no effect, still one turn
//      E4 darkness: no Dir- is asked, "You see:\ndarkness.", one turn
//      E5 Cancel at "Player:": "None!" and one turn; Cancel at Dir-: the Pass, one turn
//      E6 the T-Deck's (D)rink (ledger E-3) is the Look aimed Here: the picker, the question,
//         the picked member healed, one turn
//      E7 Original: D is "D-What?" (K:0x324e) -- no command; Space at Dir- is the Pass
// Appended after QuitSave (ui_session.h); by ordinal so the test builds against an older core.
constexpr UiRequestId kLookMember = UiRequestId(int(UiRequestId::QuitSave) + 1);
void dungeon_party(Run &h) {
    auto &g = h.rt->game();
    for (int i = 0; i < 3; ++i) g.party.characters[i].status = 'G';
    g.party.active_character = 255;
    g.torch_turns = 100;
}
uint8_t &here_cell(Run &h) {
    auto &d = h.rt->dungeon_state_for_test();
    return d.cells[d.pos.floor * 64 + d.pos.y * 8 + d.pos.x];
}
void test_e() {
    std::printf("E  the dungeon (L)ook through the real runtime\n");
    {
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        bool picker = false, dir_prompt = false;
        auto c1 = cost(h, [&] {
            act(h, ch('l'));
            picker = mode(h) == UiMode::PartySelection && request(h) == kLookMember;
            act(h, select(1));
            dir_prompt = mode(h) == UiMode::TargetSelection;
            act(h, dir(Direction::North));
        });
        check(in_dungeon(h) && picker && dir_prompt && transcript_has(h, "Look...") && transcript_has(h, "You see:") &&
                  c1.turns == 1 && c1.minutes == 1,
              "E1 L: Look..., Player:, Dir-, You see: -- one dungeon turn" + show(c1));
    }
    for (int yes = 1; yes >= 0; --yes) {
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        here_cell(h) = 0x52;
        bool asked = false;
        auto c2 = cost(h, [&] {
            act(h, ch('l'));
            act(h, select(2));
            act(h, dir(Direction::South)); // Here
            asked = mode(h) == UiMode::YesNo && transcript_has(h, "a fountain.");
            act(h, ch(yes ? 'y' : 'n'));
        });
        auto &g = h.rt->game();
        if (yes)
            check(asked && g.party.characters[2].status == 'P' && g.party.characters[0].status == 'G' &&
                      g.party.characters[1].status == 'G' && transcript_has(h, "Yes.  Gulp!") && c2.turns == 1 && c2.minutes == 1,
                  "E2 a poison fountain Here, Y: only the picked member (Shamino) is poisoned, one turn in all" + show(c2));
        else
            check(asked && g.party.characters[2].status == 'G' && transcript_has(h, "No.") && c2.turns == 1 && c2.minutes == 1,
                  "E3 the same fountain, N: \"No.\", no effect, still one turn" + show(c2));
    }
    {
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        h.rt->game().torch_turns = 0;
        h.rt->command_context_for_test().turn.light_spell_minutes = 0;
        bool dir_prompt = true;
        auto c = cost(h, [&] {
            act(h, ch('l'));
            act(h, select(0));
            dir_prompt = mode(h) == UiMode::TargetSelection;
        });
        check(!dir_prompt && transcript_has(h, "darkness.") && c.turns == 1, "E4 darkness: no Dir-, \"You see: darkness.\", one turn" + show(c));
    }
    {
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        auto c = cost(h, [&] { act(h, ch('l')); act(h, kind(UiActionKind::Cancel)); });
        check(transcript_has(h, "None!") && c.turns == 1 && c.minutes == 1, "E5 Cancel at Player: \"None!\" and the Look's turn" + show(c));
        auto c2 = cost(h, [&] { act(h, ch('l')); act(h, select(0)); act(h, kind(UiActionKind::Cancel)); });
        check(mode(h) == UiMode::Dungeon && c2.turns == 1 && c2.minutes == 1, "E5 Cancel at Dir-: the Pass, one turn" + show(c2));
    }
    {
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        here_cell(h) = 0x51;
        h.rt->game().party.characters[1].current_hp = 50;
        bool asked = false;
        auto c = cost(h, [&] {
            act(h, ch('d'));
            act(h, select(1));
            asked = mode(h) == UiMode::YesNo;
            act(h, ch('y'));
        });
        auto &g = h.rt->game();
        check(asked && g.party.characters[1].current_hp == g.party.characters[1].max_hp && g.party.characters[0].current_hp == 200 &&
                  transcript_has(h, "Healed!") && c.turns == 1,
              "E6 the T-Deck's D is the Look aimed Here: the picked member (Iolo) healed, one turn" + show(c));
    }
    {
        DosProfile dos;
        Run h(kParty);
        enter_deceit(h);
        dungeon_party(h);
        auto c = cost(h, [&] { act(h, ch('d')); });
        check(transcript_has(h, "D-What?") && c.turns == 0, "E7 Original: D is \"D-What?\" and no command (K:0x324e)" + show(c));
        auto c2 = cost(h, [&] { act(h, ch('l')); act(h, kind(UiActionKind::Confirm)); act(h, ch(' ')); });
        check(transcript_has(h, "Dir-Pass") && c2.turns == 1, "E7 Original: Space at Dir- prints \"Dir-Pass\" and the Look costs its turn" + show(c2));
    }
}

int next_member_after(Run &h) { auto *n2 = await_player(h); return n2 ? n2->member : -1; }

// --- Candidate D: spell target prompts (spp1-findings/DF-spell-targeting-heal-cancel.md) -----
//      D1 the arena's An Xen Corp asks nothing (CAST 0x043e): no reticle, the spell spent, the turn ends
//      D2 Original: an arena line spell (In Zu) reads "Direction-" (CAST 0x1f8a -> CAST2 0x0306);
//         Space is the Pass -- spent, and the turn ends exactly once
//      D3 Original: Charm prints "Creature: " before its crosshair (CAST 0x09a7, DS 0x45bb)
//      D4 a town An Ylem: Direction-, then "POOF!" turns the object east into 0x44 (CAST 0x0230);
//         bare floor is "Failed!"
void test_d() {
    std::printf("D  spell target prompts\n");
    {
        int baseline_next = -1;
        {
            Run b(kParty);
            if (enter_arena(b)) { act(b, ch(' ')); baseline_next = next_member_after(b); }
        }
        Run h(kParty);
        enter_arena(h);
        caster_party(h, 7, 8);
        act(h, ch('c'));
        if (mode(h) == UiMode::SpellSelection) act(h, select(0));
        const bool reticle = mode(h) == UiMode::TargetSelection;
        check(!reticle && h.rt->game().spell_quantities[7] == 1 && next_member_after(h) == baseline_next,
              "D1 the arena's An Xen Corp asks nothing (no reticle), is spent, and ends the turn");
    }
    {
        DosProfile dos;
        int baseline_next = -1;
        {
            Run b(kParty);
            if (enter_arena(b)) { act(b, ch(' ')); baseline_next = next_member_after(b); }
        }
        Run h(kParty);
        enter_arena(h);
        caster_party(h, 28, 8);
        act(h, ch('c'));
        for (char16_t c : {u'i', u'z'}) act(h, ch(c));
        act(h, kind(UiActionKind::Confirm));
        const bool getdir = mode(h) == UiMode::TargetSelection && transcript_has(h, "Direction-");
        act(h, ch(' '));
        check(getdir && transcript_has(h, "Direction-Pass") && h.rt->game().spell_quantities[28] == 1 &&
                  next_member_after(h) == baseline_next,
              "D2 Original: In Zu reads \"Direction-\"; Space is the Pass -- spent, the turn ends exactly once");
    }
    {
        DosProfile dos;
        Run h(kParty);
        enter_arena(h);
        caster_party(h, 34, 8);
        act(h, ch('c'));
        for (char16_t c : {u'a', u'x', u'e'}) act(h, ch(c));
        act(h, kind(UiActionKind::Confirm));
        check(mode(h) == UiMode::TargetSelection && transcript_has(h, "Creature: ") && !transcript_has(h, "Aim! "),
              "D3 Original: Charm prints \"Creature: \" before the crosshair (DS 0x45bb)");
    }
    for (int object = 1; object >= 0; --object) {
        Run h(kParty);
        enter_town(h, 2);
        caster_party(h, 5, 3);
        auto &g = h.rt->game();
        auto *q = ctx(h).quest_world;
        const int x = g.position.xy.x + 1, y = g.position.xy.y;
        if (q && q->volatile_tile) q->volatile_tile(q->context, x, y, object ? 0x5b : 0x44);
        act(h, ch('c'));
        if (mode(h) == UiMode::PartySelection) act(h, select(0));
        if (mode(h) == UiMode::SpellSelection) act(h, select(0));
        const bool getdir = mode(h) == UiMode::TargetSelection;
        act(h, dir(Direction::East));
        const int after = q && q->tile_at ? q->tile_at(q->context, x, y) : -1;
        if (object)
            check(getdir && transcript_has(h, "POOF!") && after == 0x44 && g.spell_quantities[5] == 1,
                  "D4 a town An Ylem: Direction-, \"POOF!\", the object east becomes 0x44 (CAST 0x0230)");
        else
            check(getdir && transcript_has(h, "Failed!") && !transcript_has(h, "POOF!"), "D4 on bare floor: \"Failed!\"");
    }
}

void test_c() {
    std::printf("C  the arena's J / P / Y through the real runtime\n");
    for (int dos = 0; dos < 2; ++dos) {
        std::unique_ptr<DosProfile> profile;
        if (dos) profile = std::make_unique<DosProfile>();
        const std::string tag = dos ? " [Original]" : " [T-Deck]";
        int baseline_next = -1;
        {
            Run b(kParty);
            if (!enter_arena(b)) { check(false, "C control: the troll arena opens" + tag); continue; }
            act(b, ch(' '));
            baseline_next = next_member_after(b);
        }
        {
            Run h(kParty);
            enter_arena(h);
            h.rt->game().keys = 3;
            auto *first = player_turn(h);
            const int who = first ? first->member : -1;
            const size_t before = h.rt->ui()->transcript_size();
            act(h, ch('j'));
            const bool prompt = mode(h) == UiMode::TargetSelection;
            check(prompt && transcript_has(h, "Jimmy-"), "C2 J opens the arena getdir after \"Jimmy-\"" + tag);
            bool what = false;
            for (size_t i = before; i < h.rt->ui()->transcript_size(); ++i)
                if (const auto *bl = h.rt->ui()->transcript_at(i)) what |= std::string(bl->text).find("What?") != std::string::npos;
            check(!what, "C1 J is no longer \"What?\"" + tag);
            if (dos) {
                act(h, ch(' '));
                check(transcript_has(h, "Jimmy-Pass") && next_member_after(h) == baseline_next && h.rt->game().keys == 3,
                      "C3 Space at Jimmy- prints \"Jimmy-Pass\", keeps the keys and ends the turn" + tag);
            } else {
                act(h, kind(UiActionKind::Cancel));
                auto *still = player_turn(h);
                check(who >= 0 && still && still->member == who && h.rt->game().keys == 3,
                      "C6 the T-Deck's Cancel at Jimmy- stays free: the same actor still has the turn" + tag);
            }
        }
        {
            Run h(kParty);
            enter_arena(h);
            h.rt->game().keys = 0;
            act(h, ch('j'));
            check(mode(h) != UiMode::TargetSelection && transcript_has(h, "No Keys!") && next_member_after(h) == baseline_next,
                  "C2 J without a key: \"No Keys!\" with no getdir, and the turn ends" + tag);
        }
        {
            Run h(kParty);
            enter_arena(h);
            auto *actor = player_turn(h);
            auto &s = cs(h);
            if (actor) s.map.tiles[actor->position.y * 11 + std::min(10, actor->position.x + 1)] = 0x44;
            act(h, ch('p'));
            const bool prompt = mode(h) == UiMode::TargetSelection;
            act(h, dir(Direction::East));
            check(prompt && transcript_has(h, "Push-") && transcript_has(h, "Won't budge!") && next_member_after(h) == baseline_next,
                  "C4 Push-East onto bare floor: \"Won't budge!\" and the turn ends" + tag);
        }
        for (int word = 0; word < 2; ++word) {
            Run h(kParty);
            enter_arena(h);
            act(h, ch('y'));
            const bool asked = mode(h) == UiMode::TextEntry;
            if (word) for (char16_t c : {u'o', u'r', u't'}) act(h, ch(c));
            act(h, kind(UiActionKind::Confirm));
            const bool said = word ? transcript_has(h, "No effect!") : transcript_has(h, "Nothing");
            check(asked && transcript_has(h, dos ? "Yell what?" : "Yell") && said && next_member_after(h) == baseline_next,
                  std::string("C5 Yell ") + (word ? "ORT: \"No effect!\"" : "with an empty answer: \"Nothing\"") + ", and the turn ends" + tag);
        }
    }
}
// ---- B: the tavern's paid rumour (P2) -------------------------------------------------------------------------------
//   B1 a barkeeper reached by a real (T)alk sells a rumour: after "Fair 'nuff?" Y the gold is paid and the transcript
//      shows the gossip (record 85..88: the subject and the place) and then "says <keeper>." (SHOPPES2 0x0636-0x065c,
//      DS 0x9F82 / 0x9F8A) -- both the T-Deck's and the 1988 model's routes
//   B2 CONTROL: N at "Fair 'nuff?" pays nothing and prints no gossip
constexpr uint8_t kBarkeeperDialog = 0x82;   // dialogs 0x81..0x88 are the shop types in ShopType order
struct Tavern { int location = -1, hour = -1, slot = -1; };
Tavern find_tavern() {
    for (int location = 1; location <= 32; ++location) {
        const auto &npcs = pack->npc_locations[location - 1];
        for (size_t i = 0; i < npcs.count; ++i) {
            if (npcs.slots[i].dialog != kBarkeeperDialog) continue;
            for (int hour = 8; hour < 22; ++hour)
                if (shop_is_open(npcs.slots[i].times, uint8_t(hour))) return {location, hour, npcs.slots[i].slot};
        }
    }
    return {};
}
/** In `location` at `hour`, stand beside the actor of schedule `slot` and face it; true = placed. */
bool beside(Run &h, int location, int slot, int hour, Direction &toward) {
    auto &g = h.rt->game();
    auto &c = h.rt->command_context_for_test();
    g.time.hour = uint8_t(hour);
    h.rt->turn().transport_tile = 0x1c;
    enter_town(h, location);
    if (g.position.map.location != location) return false;
    const NpcActor *npc = nullptr;
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &a = h.rt->actors().actors[i];
        if (a.location == location && a.schedule.slot == slot) npc = &a;
    }
    if (!npc) return false;
    for (const auto d : {Direction::South, Direction::North, Direction::East, Direction::West}) {
        const auto dd = direction_delta(d);
        const int x = npc->x + dd.dx, y = npc->y + dd.dy;
        const int tile = c.terrain->effective(c.world, g.position.map, x, y);
        if (!is_passable(tile, TransportMode::Foot).value || c.shop_services->occupied(c.shop_services->context, x, y)) continue;
        g.position.xy = {uint8_t(x), uint8_t(y)};
        toward = d == Direction::South ? Direction::North : d == Direction::North ? Direction::South
               : d == Direction::East ? Direction::West : Direction::East;
        return true;
    }
    return false;
}
/** Into the tavern's town, beside the barkeeper, (T)alk toward it and be served a drink (the rumour's T is only
 *  answered once served: ShopSession::tavern_served); true = the shop session is back at its menu. */
bool open_tavern(Run &h, const Tavern &t) {
    auto &c = h.rt->command_context_for_test();
    h.rt->game().gold = 5000;
    Direction toward{};
    if (beside(h, t.location, t.slot, t.hour, toward)) {
        act(h, ch('t'));
        act(h, dir(toward));
        if (mode(h) != UiMode::Shop) return false;
        const auto &shop = c.shop_services->session;
        for (int i = 0; i < 4 && shop.phase != ShopPhase::Tavern; ++i) act(h, ch('y'));
        if (shop.phase != ShopPhase::Tavern) return false;
        act(h, ch('d'));
        if (shop.phase == ShopPhase::TavernDrinkDeal) act(h, ch('y'));
        for (int i = 0; i < 4 && shop.phase != ShopPhase::Tavern; ++i) act(h, ch('y'));
        return shop.phase == ShopPhase::Tavern && shop.tavern_served;
    }
    return false;
}
void test_b() {
    std::printf("B  the tavern's paid rumour through the real runtime (P2)\n");
    const Tavern t = find_tavern();
    check(t.location > 0, "B0 a barkeeper with opening hours exists in the pack (location " + n(t.location) + ")");
    if (t.location <= 0) return;
    for (int dos = 0; dos < 2; ++dos) {
        std::unique_ptr<DosProfile> profile;
        if (dos) profile = std::make_unique<DosProfile>();
        const std::string tag = dos ? " [Original]" : " [T-Deck]";
        for (const bool pay : {true, false}) {
            Run h(kParty);
            const bool open = open_tavern(h, t);
            auto &c = h.rt->command_context_for_test();
            const auto &shop = c.shop_services->session;
            if (!open) { check(false, "B1 the tavern opens at its menu" + tag + " (phase " + n(int(shop.phase)) + ")"); continue; }
            act(h, ch('t'));
            const bool asked = mode(h) == UiMode::TextEntry;
            for (char16_t k : {u'b', u'r', u'i', u't'}) act(h, ch(k));
            act(h, kind(UiActionKind::Confirm));
            const bool deal = shop.phase == ShopPhase::RumorDeal;
            const int32_t price = shop.price;
            const std::string keeper = shop.record && shop.record->keeper ? shop.record->keeper : "Merchant";
            const int32_t gold = h.rt->game().gold;
            const size_t mark = h.rt->ui()->transcript_size();
            act(h, ch(pay ? 'y' : 'n'));
            std::string after;
            for (size_t i = mark; i < h.rt->ui()->transcript_size(); ++i)
                if (const auto *b = h.rt->ui()->transcript_at(i)) after += std::string(b->text) + "|";
            const auto says = after.find("says " + keeper + ".");
            if (pay)
                check(asked && deal && price > 0 && h.rt->game().gold == gold - price && says != std::string::npos && says > 0,
                      "B1 a paid rumour: " + n(price) + " gold paid, the gossip then \"says " + keeper + ".\" is shown" + tag +
                          " [" + after + "]");
            else
                check(asked && deal && h.rt->game().gold == gold && says == std::string::npos,
                      "B2 CONTROL: N at \"Fair 'nuff?\" pays nothing and shows no gossip" + tag + " [" + after + "]");
        }
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spp1_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the checks cannot run\n");
        return 1;
    }
    static std::vector<DungeonArena> arenas;
    for (size_t i = 0; i < pack->combat_map_count; ++i) arenas.push_back({pack->combat_map_views[i], pack->combat_sprites + i * 16});
    g_arenas = arenas.data();
    g_arena_count = arenas.size();
    test_g();
    test_f();
    test_a();
    test_c();
    test_e();
    test_d();
    test_b();
    std::printf("SPP1 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
