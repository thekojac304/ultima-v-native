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
#include "openu5/platform_profile.h"
#include "openu5/quest_state.h"

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
    std::printf("SPP1 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
