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
    std::printf("SPP1 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
