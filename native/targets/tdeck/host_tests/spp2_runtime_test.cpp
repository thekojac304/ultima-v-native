// Shared Preservation Parity Batch 2 (docs/SHARED_PRESERVATION_PARITY_SPP2.md) -- through the REAL
// AlphaRuntime on the REAL tdeck_board.cpp (the A4-UI2 harness), PACED as the device runs.
//
//   M  WHEN the kernel dispatcher's turn is charged (SPP1 A follow-up). A command whose dispatcher
//      arm returns 1 (K:0x3178) is followed at once by the context loop's turn: MAINOUT 0x0c00 calls
//      the dispatcher, 0x0c0f stores its 1, 0x0c30 tests it and 0x0c39 runs advance_clock(2) and the
//      turn block down to the world phase at 0x0d11; 0x0d1a jumps back to the loop top 0x0a8f, and
//      only then does 0x0b14 read the next key. Mix is K:0x3340 (call CMDS, jmp 0x31ee: 1); its tail
//      CMDS 0x1b81-0x1c1f prints "Mixing..." (DS 0x8ff0), waits 10 ticks (0x617a / 0x7b66), deducts,
//      prints "Done!" (0x8ffc) and returns. So the turn lands after "Done!" with no key in between.
//      SPP1 (5ba2fa46) charged a command that ends on a timer at the NEXT input instead.
//      M1 a paced Mix, no further input: the turn is charged when the "Mixing..." hold ends
//      M2 ... then a delayed Pass: the Mix's turn was already there; the Pass adds its own (two in all)
//      M3 a key pressed during the hold is swallowed and costs nothing; a Pass on the first frame
//         after the hold is its own turn (nothing absorbed, nothing charged twice)
//      M4 (U)se the View scroll (Use K:0x340c returns 1): the map reveal (R-12) is a timed hold, and
//         the Use's turn is charged when it ends, without input
//      M5 a save made right after the hold holds the turn; a save made DURING the hold (the device's
//         Alt+S keeps its meaning in a hold) holds the state before it, the live game still charges
//         it once, and loading that save charges nothing more
//      M6 the reveal is a plain timer: when it runs out and a key arrives before any frame has
//         seen it, the Use's turn is still charged first and the key's own Pass is its own turn
//   N  what the turn brings follows it at once: the town loop's NPC tail (TOWN 0x15bf ->) runs an
//      adjacent NPC's approach in the same pass, before the next key
//      N1 control: a Pass with an approaching NPC beside the party opens the conversation at once
//      N2 an R cancelled at "Player:" (K:0x339a returns 1) does the same (SPP1 kept the approach
//         until after the NEXT key's own command)
//      N3 a fight the turn begins (the world phase 0x1a60: an adjacent troll attacks) plays the
//         battle music at once; SPP1 left the surface's song on until the next key (the music
//         selector runs on key polls, ALPHA4_UI.md's render-driven-transition rule)
//
//   T  TALK.OVL's own answers in a town (ledger D-111). The kernel's T arm (0x33c2) sends a town
//      Talk to TALK 0x041c. No NPC at the target (0x048a, after the counter step 0x046b) prints
//      DS 0x91d6 "\nNobody's here!\n" (0x0491) and returns 0. An NPC whose dialog number is 0 is
//      answered by the converse dispatch 0x031e: "No response!" (DS 0x9168, 0x0364), or, for a
//      guard (object type 0x70, 0x0371), "The guard offers\nno response!" (DS 0x9148, 0x0387).
//      "Funny, no response!" (DS 0xa216) is the kernel's own overworld / dungeon string.
//      T1 a town Talk at an empty cell: "Nobody's here!", not "Funny, no response!" (both profiles)
//      T2 control: the overworld Talk keeps "Funny, no response!"
//      T3 a town NPC with dialog 0 that is no guard: "No response!"
//      T4 a town guard with dialog 0: "The guard offers no response!"; one whose served schedule AI
//         is 4 (approach) skips the guard test (0x0350) and answers "No response!"
//      T5 each opens no conversation and costs one town turn (TALK -> 0, the dispatcher's 1)
//
//   spp2_runtime <openu5-alpha1-resources.bin> [openu5-audio.bin]
#include "a4_ui2_harness.h"
#include "openu5/dialogue.h"
#include "openu5/platform_profile.h"
#include "openu5/world.h"

using namespace a4_ui2;

namespace tdeck {
void host_memory_save_forget_for_test(); // alpha_save_memory_host_stub.cpp
}

namespace {
const std::vector<Member> kParty = {{"Avatar", 'G', 200}, {"Iolo", 'G', 200}, {"Shamino", 'G', 200}};
AudioPackInfo g_audio{};

UiAction ch(char16_t c) { UiAction a; a.kind = UiActionKind::Character; a.character = c; return a; }
UiAction kind(UiActionKind k) { UiAction a; a.kind = k; return a; }
UiAction select(int32_t i) { UiAction a; a.kind = UiActionKind::SelectIndex; a.index = i; return a; }
UiMode mode(Run &h) { return h.rt->ui()->mode(); }
bool transcript_has(Run &h, const char *needle) { return h.transcript().find(needle) != std::string::npos; }

/** One desktop input: handle_action, as native/targets/windows routes a key, then 100 ms of frames. */
bool act(Run &h, const UiAction &a) {
    openu5_host_virtual_clock_us() += 100000;
    const bool routed = h.rt->handle_action(a, tdeck::DeviceShortcut::None, Run::now(), "spp2_runtime");
    h.run(100);
    return routed;
}

struct Snap {
    int64_t turns;
    int clock;
};
Snap snap(Run &h) {
    const auto &g = h.rt->game();
    return {g.turns_since_start, (int(g.time.day) * 24 + g.time.hour) * 60 + g.time.minute};
}
struct Cost {
    int64_t turns;
    int minutes;
};
Cost since(const Snap &a, Run &h) {
    const Snap b = snap(h);
    return {b.turns - a.turns, b.clock - a.clock};
}
std::string show(const Cost &c) { return " (turns +" + n(c.turns) + ", clock +" + n(c.minutes) + " min)"; }

void surface(Run &h) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {80, 80};
    h.run(50);
}
void enter_town(Run &h, int location) {
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {pack->location_x[location - 1], pack->location_y[location - 1]};
    h.key('e');
    h.run(200);
}

/** In Lor from five Sulfur Ash, on the device keys (A12's walk), up to the RETURN on "How much? 1". */
void mix_in_lor(Run &h) {
    auto &g = h.rt->game();
    for (auto &r : g.reagent_quantities) r = 0;
    g.reagent_quantities[0] = 5; // Sulfur Ash
    g.spell_quantities[0] = 2;   // In Lor
    h.key('m');
    h.key('\r'); // In Lor, row 0
    h.key('\r'); // mark Sulfur Ash, row 0
    h.key('m');  // mix
    h.key('1');
    h.key('\r'); // "Mixing..." -- the hold starts here
}
bool mixed(Run &h) { return h.rt->game().spell_quantities[0] == 3 && h.rt->game().reagent_quantities[0] == 4; }
bool holding(Run &h) { return h.rt->transient_probe_for_test().dialogue_pause; }

void test_m() {
    std::printf("M  a command that ends on a timer pays its turn when it ends, not at the next key\n");
    {
        Run h(kParty, true);
        surface(h);
        const Snap before = snap(h);
        mix_in_lor(h);
        const bool held = holding(h);
        const Cost during = since(before, h);
        h.run(2000); // the hold ends on the clock; no key at all
        const Cost c = since(before, h);
        check(held && during.turns == 0, "M1 control: the paced Mix holds \"Mixing...\" and nothing is charged inside it" + show(during));
        check(mixed(h) && transcript_has(h, "Done!") && !holding(h), "M1 control: the hold ended by itself and the Mix is done");
        check(c.turns == 1 && c.minutes == 2,
              "M1 the Mix's world turn is charged when the hold ends, with no further input (MAINOUT 0x0c39 before 0x0b14)" + show(c));
        h.run(10000); // the player waits
        const Cost idle = since(before, h);
        check(idle.turns == 1 && idle.minutes == 2, "M2 waiting charges nothing more" + show(idle));
        h.key('\r'); // Pass
        h.run(200);
        const Cost total = since(before, h);
        check(total.turns == 2 && total.minutes == 4, "M2 a delayed Pass adds its own turn: two in all" + show(total));
    }
    {
        Run h(kParty, true);
        surface(h);
        const Snap before = snap(h);
        mix_in_lor(h);
        h.key('z', false, false, 50000); // pressed inside the hold
        const bool swallowed = mode(h) == UiMode::Exploration && !h.rt->transient_probe_for_test().zstats;
        h.run(2000);
        const Cost c = since(before, h);
        check(swallowed && c.turns == 1 && c.minutes == 2,
              "M3 a key pressed during the hold is swallowed (no Z-stats) and the Mix still costs exactly one turn" + show(c));
    }
    {
        Run h(kParty, true);
        surface(h);
        const Snap before = snap(h);
        mix_in_lor(h);
        int frames = 0;
        while (holding(h) && frames < 1000) { h.run(5); ++frames; }
        const Cost at_release = since(before, h);
        h.key('\r', false, false, 1000); // Pass, on the first frame after the hold
        h.run(200);
        const Cost c = since(before, h);
        check(at_release.turns == 1, "M3 the turn is on the frame the hold ends" + show(at_release));
        check(c.turns == 2 && c.minutes == 4, "M3 an immediate Pass is its own turn: nothing absorbed, nothing twice" + show(c));
    }
    {
        Run h(kParty, true);
        surface(h);
        auto &g = h.rt->game();
        for (auto &q : g.scroll_quantities) q = 0;
        for (auto &q : g.potion_quantities) q = 0;
        g.scroll_quantities[4] = 1; // the View scroll
        const Snap before = snap(h);
        act(h, ch('u'));
        if (mode(h) != UiMode::Exploration) act(h, select(0));
        const bool reveal = h.rt->transient_probe_for_test().map_reveal;
        const Cost during = since(before, h);
        h.run(4000); // the reveal (20 units) and any ceremony run out; no key
        const Cost c = since(before, h);
        check(reveal && transcript_has(h, "View!") && g.scroll_quantities[4] == 0,
              "M4 control: (U)se the View scroll opens the map reveal");
        check(during.turns == 0, "M4 nothing is charged while the reveal holds" + show(during));
        check(c.turns == 1 && c.minutes == 2, "M4 the Use's turn is charged when the reveal ends, without input" + show(c));
    }
    {
        Run h(kParty, true);
        surface(h);
        auto &g = h.rt->game();
        for (auto &q : g.scroll_quantities) q = 0;
        for (auto &q : g.potion_quantities) q = 0;
        g.scroll_quantities[4] = 1;
        const Snap before = snap(h);
        act(h, ch('u'));
        if (mode(h) != UiMode::Exploration) act(h, select(0));
        const bool reveal = h.rt->transient_probe_for_test().map_reveal;
        openu5_host_virtual_clock_us() += 4000000; // the reveal runs out; no frame is drawn
        h.key('\r');                              // Pass
        h.run(200);
        const Cost c = since(before, h);
        check(reveal && c.turns == 2 && c.minutes == 4,
              "M6 a key that comes before the first frame after the reveal: the Use's turn, then the Pass's" + show(c));
    }
    {
        tdeck::host_memory_save_forget_for_test();
        Run h(kParty, true);
        surface(h);
        const Snap before = snap(h);
        mix_in_lor(h);
        h.run(2000);
        h.key('s', true); // Alt+S right after the hold
        h.key('l', true); // Alt+L: the saved game back
        h.run(100);
        const Cost c = since(before, h);
        check(transcript_has(h, "Save complete") && transcript_has(h, "Load complete") && c.turns == 1 && c.minutes == 2,
              "M5 a save right after the Mix holds its turn" + show(c));
    }
    {
        tdeck::host_memory_save_forget_for_test();
        Run h(kParty, true);
        surface(h);
        const Snap before = snap(h);
        mix_in_lor(h);
        const bool held = holding(h);
        h.key('s', true, false, 50000); // Alt+S inside the hold
        const bool saved = transcript_has(h, "Save complete");
        h.run(2000);
        const Cost live = since(before, h);
        h.key('l', true);
        h.run(2000);
        const Cost loaded = since(before, h);
        check(held && saved, "M5 control: Alt+S inside the hold saves (the device shortcuts keep their meaning)");
        check(live.turns == 1 && live.minutes == 2, "M5 the live game still charges the Mix's turn once, when the hold ends" + show(live));
        check(mixed(h) && loaded.minutes == 0,
              "M5 the save made inside the hold has the Mix without its turn, and its load charges nothing" + show(loaded));
    }
}

/** Britain at `hour`: an actor of the town with a talk script, made an approacher (schedule ai 4) beside the party. */
bool approacher_beside(Run &h, int hour) {
    auto &g = h.rt->game();
    auto &c = h.rt->command_context_for_test();
    g.time.hour = uint8_t(hour);
    h.rt->turn().transport_tile = 0x1c;
    enter_town(h, 2);
    if (g.position.map.location != 2 || !c.actors || !c.dialogue_services) return false;
    for (size_t i = 0; i < c.actors->count; ++i) {
        auto &a = c.actors->actors[i];
        if (a.location != 2 || a.z != g.position.map.floor) continue;
        const int dialog = a.schedule.dialog;
        if (dialog <= 0 || dialog >= 128 || !talk_script_for(c.dialogue_services->registry, 2, dialog)) continue;
        for (const auto d : {Direction::South, Direction::North, Direction::East, Direction::West}) {
            const auto dd = direction_delta(d);
            const int x = a.x + dd.dx, y = a.y + dd.dy;
            const int tile = c.terrain->effective(c.world, g.position.map, x, y);
            if (!is_passable(tile, TransportMode::Foot).value || c.shop_services->occupied(c.shop_services->context, x, y)) continue;
            g.position.xy = {uint8_t(x), uint8_t(y)};
            for (auto &ai : a.schedule.ai) ai = 4;
            for (size_t j = 0; j < c.actors->count; ++j) // nobody else beside the party
                if (j != i && c.actors->actors[j].location == 2 &&
                    std::abs(c.actors->actors[j].x - x) + std::abs(c.actors->actors[j].y - y) <= 2) c.actors->actors[j].location = 0;
            return true;
        }
    }
    return false;
}
bool talking(Run &h) {
    const auto *ds = h.rt->command_context_for_test().dialogue_services;
    return ds && ds->session.active;
}

void test_n() {
    std::printf("N  the turn's NPC approach follows it at once\n");
    {
        Run h(kParty, true);
        const bool placed = approacher_beside(h, 12);
        const Snap before = snap(h);
        act(h, kind(UiActionKind::Confirm)); // Pass
        h.run(1000);
        check(placed && talking(h), "N1 control: a Pass with an approaching NPC beside the party opens the conversation at once" + show(since(before, h)));
    }
    {
        Run h(kParty, true);
        const bool placed = approacher_beside(h, 12);
        const Snap before = snap(h);
        act(h, ch('r'));
        act(h, kind(UiActionKind::Cancel)); // "Player: None!" -- K:0x339a still returns 1
        h.run(1000);
        const Cost c = since(before, h);
        check(placed && c.turns == 1 && talking(h),
              "N2 an R cancelled at Player: charges the town turn, and the NPC's approach opens the conversation before any key" + show(c));
    }
}
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
/** On open grass with a troll beside the party (A3-HF3's fight), the surface's song playing. */
bool troll_beside(Run &h) {
    auto &c = h.rt->command_context_for_test();
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    auto &g = h.rt->game();
    g.position.map = {0, 0};
    g.position.xy = {uint8_t(gx), uint8_t(gy)};
    h.run(100);
    c.outdoor->enemies.clear();
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(c);
    troll.x = g.position.xy.x + 1;
    troll.y = g.position.xy.y;
    c.outdoor->enemies.push_back(troll);
    return troll.tile >= 0;
}
bool in_combat(Run &h) { return h.rt->command_context_for_test().combat && h.rt->combat_state_for_test().initialized; }

void test_n3() {
    if (g_audio.state != AudioPackState::Valid) {
        check(false, "N3 the audio pack is not valid -- run `npm run pack:audio`");
        return;
    }
    for (const bool control : {true, false}) {
        Run h(kParty, true, &g_audio);
        const bool placed = troll_beside(h);
        h.run(200);
        const auto song = h.rt->audio().current_music_context();
        if (control) h.key(' '); // Pass: the core's own turn, routed by the key
        else {
            act(h, ch('r'));
            act(h, kind(UiActionKind::Cancel)); // the latch's turn
        }
        int frames = 0;
        while (!in_combat(h) && frames < 400) { h.run(5); ++frames; }
        h.run(50); // no key
        const auto now = h.rt->audio().current_music_context();
        check(placed && song != MusicContext::Combat && in_combat(h) && now == MusicContext::Combat,
              std::string(control ? "N3 control: a Pass beside a troll starts the fight, with its music"
                                  : "N3 an R cancelled beside a troll: the turn starts the fight, and its music plays without another key") +
                  " (" + music_context_name(song) + " -> " + music_context_name(now) + ")");
    }
}
// --- T: TALK.OVL's answers -----------------------------------------------------------------------
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
UiAction dir(Direction d) { UiAction a; a.kind = UiActionKind::Direction; a.direction = d; return a; }
std::string since_mark(Run &h, size_t mark) {
    std::string out;
    for (size_t i = mark; i < h.rt->ui()->transcript_size(); ++i)
        if (const auto *b = h.rt->ui()->transcript_at(i)) out += std::string(b->text) + "|";
    return out;
}
/** In `location` at noon, beside the actor of schedule `slot`; `toward` faces it. */
bool beside_actor(Run &h, int location, int slot, Direction &toward) {
    auto &g = h.rt->game();
    auto &c = h.rt->command_context_for_test();
    g.time.hour = 12;
    h.rt->turn().transport_tile = 0x1c;
    enter_town(h, location);
    if (g.position.map.location != location) return false;
    const NpcActor *npc = nullptr;
    for (size_t i = 0; i < c.actors->count; ++i)
        if (c.actors->actors[i].location == location && c.actors->actors[i].schedule.slot == slot) npc = &c.actors->actors[i];
    if (!npc || npc->z != g.position.map.floor) return false;
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
struct TalkRun {
    bool placed = false, opened = false;
    std::string text;
    Cost cost{};
};
/** Talk toward an NPC of dialog 0 (guard or not) somewhere in the towns; the first one that can be reached. */
TalkRun talk_to_dialog_zero(bool guard, bool approach = false) {
    for (int location = 1; location <= 32; ++location) {
        const auto &npcs = pack->npc_locations[location - 1];
        for (size_t i = 0; i < npcs.count; ++i) {
            const auto &npc = npcs.slots[i];
            if (npc.dialog != 0 || !npc.type || (npc.type == 0x70) != guard) continue;
            Run h(kParty);
            Direction toward{};
            if (!beside_actor(h, location, npc.slot, toward)) continue;
            TalkRun r;
            r.placed = true;
            if (approach) {
                auto &c = h.rt->command_context_for_test();
                for (size_t k = 0; k < c.actors->count; ++k) {
                    auto &a = c.actors->actors[k];
                    if (a.location == location && a.schedule.slot == npc.slot && a.served_slot < 3) a.schedule.ai[a.served_slot] = 4;
                }
            }
            const Snap before = snap(h);
            const size_t mark = h.rt->ui()->transcript_size();
            act(h, ch('t'));
            act(h, dir(toward));
            const auto *ds = h.rt->command_context_for_test().dialogue_services;
            r.opened = (ds && ds->session.active) || mode(h) != UiMode::Exploration;
            r.text = since_mark(h, mark) + " [location " + n(location) + " slot " + n(npc.slot) + " type " + n(npc.type) + "]";
            r.cost = since(before, h);
            return r;
        }
    }
    return {};
}

void test_t() {
    std::printf("T  TALK.OVL's own answers in a town (D-111)\n");
    for (int dos = 0; dos < 2; ++dos) {
        std::unique_ptr<DosProfile> profile;
        if (dos) profile = std::make_unique<DosProfile>();
        const std::string tag = dos ? " [Original]" : " [T-Deck]";
        Run h(kParty);
        enter_town(h, 2);
        auto &g = h.rt->game();
        auto &cx = h.rt->command_context_for_test();
        int nx = -1, ny = -1;
        const auto clear = [&](int x, int y) {
            for (size_t i = 0; i < cx.actors->count; ++i) {
                const auto &a = cx.actors->actors[i];
                if (a.location == g.position.map.location && std::abs(a.x - x) + std::abs(a.y - y) <= 4) return false;
            }
            return true;
        };
        for (int y = 2; y < 30 && nx < 0; ++y)
            for (int x = 2; x < 30 && nx < 0; ++x)
                if (is_passable(cx.terrain->effective(cx.world, g.position.map, x, y), TransportMode::Foot).value &&
                    is_passable(cx.terrain->effective(cx.world, g.position.map, x + 1, y), TransportMode::Foot).value &&
                    !cx.shop_services->occupied(cx.shop_services->context, x, y) &&
                    !cx.shop_services->occupied(cx.shop_services->context, x + 1, y) && clear(x, y)) { nx = x; ny = y; }
        g.position.xy = {uint8_t(nx), uint8_t(ny)};
        const Snap before = snap(h);
        const size_t mark = h.rt->ui()->transcript_size();
        act(h, ch('t'));
        act(h, dir(Direction::East));
        const std::string text = since_mark(h, mark);
        const Cost c = since(before, h);
        const auto *ds = h.rt->command_context_for_test().dialogue_services;
        check(nx >= 0 && text.find("Nobody's here!") != std::string::npos && text.find("Funny") == std::string::npos,
              "T1 a town Talk at an empty cell answers \"Nobody's here!\" (TALK 0x0491, DS 0x91d6)" + tag + ": " + text);
        check(!(ds && ds->session.active) && mode(h) == UiMode::Exploration && c.turns == 1 && c.minutes == 1,
              "T5 ... opens nothing and costs one town turn" + tag + show(c));
    }
    {
        Run h(kParty);
        surface(h);
        const size_t mark = h.rt->ui()->transcript_size();
        act(h, ch('t'));
        act(h, dir(Direction::East));
        const std::string text = since_mark(h, mark);
        check(text.find("Funny, no response!") != std::string::npos && text.find("Nobody") == std::string::npos,
              "T2 control: the overworld Talk keeps the kernel's \"Funny, no response!\" (0x33c2): " + text);
    }
    {
        const TalkRun r = talk_to_dialog_zero(false);
        check(r.placed && r.text.find("No response!") != std::string::npos && r.text.find("Funny") == std::string::npos,
              "T3 an NPC of dialog 0 answers \"No response!\" (TALK 0x0364, DS 0x9168): " + r.text);
        check(r.placed && !r.opened && r.cost.turns == 1, "T5 ... opens nothing and costs one town turn" + show(r.cost));
    }
    {
        const TalkRun r = talk_to_dialog_zero(true);
        check(r.placed && r.text.find("The guard offers\nno response!") != std::string::npos && r.text.find("Funny") == std::string::npos,
              "T4 a guard of dialog 0 answers \"The guard offers no response!\" (TALK 0x0387, DS 0x9148): " + r.text);
        check(r.placed && !r.opened && r.cost.turns == 1, "T5 ... opens nothing and costs one town turn" + show(r.cost));
    }
    {
        const TalkRun r = talk_to_dialog_zero(true, true);
        check(r.placed && r.text.find("|No response!|") != std::string::npos && r.text.find("guard") == std::string::npos,
              "T4 a guard of dialog 0 whose AI is 4 (approach) answers \"No response!\" (TALK 0x0350 skips the guard test): " + r.text);
    }
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: spp2_runtime <openu5-alpha1-resources.bin>\n");
        return 2;
    }
    if (!load_pack(argv[1])) {
        std::printf("RED the pack does not load -- the checks cannot run\n");
        return 1;
    }
    if (argc > 2) g_audio = tdeck::load_audio_pack_info(argv[2]);
    test_m();
    test_n();
    test_n3();
    test_t();
    std::printf("SPP2 runtime: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
