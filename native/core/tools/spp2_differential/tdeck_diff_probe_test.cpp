// T-Deck preservation differential (SPP1; SPP2 adds walk I) -- not registered in any CMake file.
// The same raw T-Deck key walks (keyboard, trackball, Mic, Alt chords) through the REAL
// AlphaRuntime on the REAL Board (a4_ui2 harness, default PlatformProfile = the device),
// built once from the baseline tree (e089f2c5) and once from the SPP1 branch. After every
// input it prints hashes of the panel GRAM, the transcript (text + channel + flags),
// GameState, the arena, the audio calls and the last save's GAM bytes. SPP1 changes T-Deck
// behaviour on purpose: every walk whose output differs is classified by its first
// differing step (spp1_probe_compare.py), and each class must be an SPP1 correction.
//
//   tdeck_diff_probe <openu5-alpha1-resources.bin> <openu5-audio.bin>
#include "a4_ui2_harness.h"
#include "openu5/combat.h"
#include "openu5/commands.h"
#include "openu5/debug_developer.h"
#include "openu5/frontend_settings.h"
#include "openu5/narrative_scene.h"
#include "openu5/outdoor.h"
#include "openu5/presentation.h"
#include "openu5/quest_state.h"
#include "openu5/shop_orchestration.h"
#include "openu5/shops.h"
#include "openu5/world.h"
#ifdef PROBE_DOS
#include "openu5/platform_profile.h"
#endif

namespace tdeck {
std::vector<uint8_t> host_memory_save_gam_for_test();
void host_memory_save_forget_for_test();
} // namespace tdeck

using namespace a4_ui2;

namespace {
AudioPackInfo g_audio{};
bool g_paced = false; // --paced: the device's scene and dialogue pacers, as on hardware
uint64_t fnv(const void *p, size_t n, uint64_t h = 1469598103934665603ull) {
    const auto *b = static_cast<const uint8_t *>(p);
    for (size_t i = 0; i < n; ++i) h = (h ^ b[i]) * 1099511628211ull;
    return h;
}
uint64_t fnv_str(const std::string &s, uint64_t h = 1469598103934665603ull) { return fnv(s.data(), s.size(), h); }

CombatState &cs(Run &h) { return h.rt->combat_state_for_test(); }
CommandContext &ctx(Run &h) { return h.rt->command_context_for_test(); }
bool in_combat(Run &h) { return ctx(h).combat && cs(h).initialized; }

std::string esc(const char *t) {
    std::string o;
    for (const char *p = t; *p; ++p) {
        if (*p == '\n') o += "\\n";
        else if (uint8_t(*p) < 32 || uint8_t(*p) > 126) { char b[8]; std::snprintf(b, sizeof b, "\\x%02x", uint8_t(*p)); o += b; }
        else o += *p;
    }
    return o;
}

struct Probe {
    Run &h;
    std::string walk;
    int step = 0;
    Probe(Run &run, std::string name) : h(run), walk(std::move(name)) { snap("start"); }
    void snap(const std::string &label) {
        const uint64_t gram = fnv(bus::gram(), size_t(320) * 240 * sizeof(uint16_t));
        uint64_t text = 1469598103934665603ull;
        const auto *ui = h.rt->ui();
        std::string tail;
        const size_t blocks = ui->transcript_size();
        for (size_t i = 0; i < blocks; ++i)
            if (const auto *b = ui->transcript_at(i)) {
                text = fnv(b->text, b->length, text);
                const uint32_t meta[2] = {uint32_t(b->channel), uint32_t(b->flags)};
                text = fnv(meta, sizeof meta, text);
                if (i + 3 >= blocks) tail += "[" + esc(b->text) + "]";
            }
        const uint64_t state = fnv(&h.rt->game(), sizeof(GameState));
        uint64_t arena = 0;
        if (in_combat(h)) {
            auto &s = cs(h);
            arena = fnv(&s.current, sizeof s.current);
            for (int i = 0; i < s.count; ++i) {
                const auto &a = s.actors[i];
                const int32_t v[6] = {a.position.x, a.position.y, int32_t(a.hp), int32_t(a.status), a.member, a.enemy ? 1 : 0};
                arena = fnv(v, sizeof v, arena);
            }
        }
        uint64_t audio = 1469598103934665603ull;
        for (const auto &c : h.audio.calls) {
            audio = fnv_str(c.what, audio);
            const int64_t v[2] = {c.value, c.us};
            audio = fnv(v, sizeof v, audio);
        }
        const auto gam = tdeck::host_memory_save_gam_for_test();
        const uint64_t save = gam.empty() ? 0 : fnv(gam.data(), gam.size());
        const auto &cur = h.board.console_cursor_state();
        std::printf("%s %03d %-14s gram=%016llx text=%016llx state=%016llx mode=%d req=%d prompt=[%s] arena=%016llx "
                    "audio=%016llx save=%016llx cursor=%d,%d,%d turns=%lld clock=%d:%02d:%02d seed=%08x tail=%s\n",
                    walk.c_str(), step++, label.c_str(), (unsigned long long)gram, (unsigned long long)text,
                    (unsigned long long)state, int(ui->mode()), int(ui->request()), esc(ui->prompt()).c_str(),
                    (unsigned long long)arena, (unsigned long long)audio, (unsigned long long)save, int(cur.visible),
                    cur.x, cur.y, (long long)h.rt->game().turns_since_start, int(h.rt->game().time.day),
                    int(h.rt->game().time.hour), int(h.rt->game().time.minute), unsigned(h.rt->game().rng.get_seed()),
                    tail.c_str());
    }
    // One input token: a char key, or one of the named raw inputs.
    void input(const std::string &tok) {
        if (tok == "RET") h.key('\r');
        else if (tok == "BS") h.key('\b');
        else if (tok == "MIC") h.mic();
        else if (tok == "UP") h.up();
        else if (tok == "DOWN") h.down();
        else if (tok == "LEFT") h.ball(RawInputKind::TrackballLeft);
        else if (tok == "RIGHT") h.ball(RawInputKind::TrackballRight);
        else if (tok == "CLICK") {
            openu5_host_virtual_clock_us() += 100000;
            tdeck::RawInputEvent e{};
            e.kind = RawInputKind::TrackballClick;
            e.transition = tdeck::KeyTransition::Pressed;
            h.raw(e);
            openu5_host_virtual_clock_us() += 60000;
            e.transition = tdeck::KeyTransition::Released;
            h.raw(e);
            h.render();
        } else if (tok == "ALT-M") h.key('m', true);
        else if (tok == "ALT-S") h.key('s', true);
        else if (tok == "ALT-L") h.key('l', true);
        else if (tok == "WAIT") h.run(2000);
        else if (tok == "^K") h.key(0x0b);   // not on the T-Deck keyboard: the shared path only
        else if (tok == "^V") h.key(0x16);
        else if (tok.size() == 1) h.key(uint8_t(tok[0]));
        else std::printf("BAD TOKEN %s\n", tok.c_str());
        h.run(400);
        snap(tok);
    }
    void script(const std::vector<std::string> &tokens) {
        for (const auto &t : tokens) input(t);
    }
};

const std::vector<Member> kParty = {{"Avatar", 'G', 120}, {"Iolo", 'G', 110}, {"Shamino", 'G', 100}};

void stock(Run &h) {
    auto &g = h.rt->game();
    for (int i = 0; i < 48; ++i) g.spell_quantities[i] = 3;
    for (int i = 0; i < 8; ++i) g.reagent_quantities[i] = 9, g.potion_quantities[i] = 2, g.scroll_quantities[i] = 2;
    for (int i = 0; i < 48; i += 3) g.equipment_quantities[i] = 1;
    g.keys = 2;
    g.gems = 2;
    g.torches = 3;
    for (int i = 0; i < 3; ++i) {
        auto &m = g.party.characters[i];
        m.current_mp = 30;
        m.level = 4;
        m.intelligence = 28;
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
            for (int dy = -6; dy <= 6 && ok; ++dy)
                for (int dx = -6; dx <= 6 && ok; ++dx)
                    ok = tile_animation_kind(m.value.tile_at(x + dx, y + dy)) == TileAnimationKind::Static;
            if (ok) { gx = x; gy = y; return true; }
        }
    return false;
}
bool to_grass(Run &h) {
    int gx = -1, gy = -1;
    if (!inland_grass(gx, gy)) return false;
    DebugTeleportRequest r{};
    r.kind = DebugDestinationKind::Britannia;
    r.x = gx;
    r.y = gy;
    if (apply_debug_teleport(ctx(h), r).status != DebugTeleportStatus::Applied) return false;
    ctx(h).outdoor->enemies.clear();
    h.render(true);
    h.run(200);
    return true;
}
bool teleport(Run &h, DebugDestinationKind kind, int location, int floor = 0) {
    DebugTeleportRequest r{};
    r.kind = kind;
    r.location = location;
    r.floor = floor;
    r.standard_entry = true;
    const bool ok = apply_debug_teleport(ctx(h), r).status == DebugTeleportStatus::Applied;
    h.render(true);
    h.run(200);
    return ok;
}
bool enter_arena(Run &h) {
    if (!to_grass(h)) return false;
    auto &c = ctx(h);
    OutdoorEnemy troll{};
    troll.definition = 41;
    troll.tile = troll_tile(c);
    troll.x = h.rt->game().position.xy.x + 1;
    troll.y = h.rt->game().position.xy.y;
    c.outdoor->enemies.push_back(troll);
    h.key(' ');
    h.run(1500);
    for (int t = 0; t < 20000 && in_combat(h) && h.rt->ui()->mode() != UiMode::Combat; t += 5) h.run(5);
    return in_combat(h);
}

const std::vector<std::vector<std::string>> kScripts = {
    {"MIC"},
    {"RET", "RET", "RET", "MIC", "MIC"},
    {"DOWN", "DOWN", "RET", "DOWN", "RET", "BS", "MIC"},
    {"2", "RET", "a", "RET", "MIC"},
    {"RIGHT", "RET", "MIC"},
    {"y", "n", "BS", "CLICK", "MIC"},
    {"0", "1", "BS", "RET", "UP", "RET", "MIC", "MIC"},
    {"i", "n", " ", "l", "o", "r", "RET", "RET", "MIC"},
};

// --- A: every command letter on the surface, every response script, fresh each time ---
void walk_world(bool active) {
    const char *letters = "abcdefghijklmnopqrstuvwxyz";
    for (const char *L = letters; *L; ++L)
        for (size_t s = 0; s < kScripts.size(); ++s) {
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            if (!to_grass(h)) { std::printf("A no grass\n"); return; }
            if (active) h.rt->game().party.active_character = 1;
            Probe p(h, std::string(active ? "Aact-" : "A-") + *L + "-" + std::to_string(s));
            p.input(std::string(1, *L));
            p.script(kScripts[s]);
        }
}

// --- B: the arena: a troll fight, each command family and response ---
void walk_arena() {
    const std::vector<std::vector<std::string>> scripts = {
        {"a", "RIGHT", "WAIT"},
        {"a", "MIC", "WAIT"},
        {"a", "BS", "RET", "WAIT"},
        {"c", "RET", "RET", "RET", "RIGHT", "WAIT"},
        {"c", "MIC", "WAIT"},
        {"c", "DOWN", "RET", "MIC", "WAIT"},
        {"r", "RET", "RET", "MIC", "WAIT"},
        {"r", "MIC", "WAIT"},
        {"u", "RET", "RET", "WAIT"},
        {"u", "MIC", "WAIT"},
        {"z", "RET", "MIC", "WAIT"},
        {"z", "DOWN", "MIC", "WAIT"},
        {"RET", "WAIT"},
        {"BS", "WAIT"},
        {"MIC", "WAIT"},
        {"g", "RIGHT", "WAIT"},
        {"m", "RET", "WAIT"},
        {"e", "WAIT"},
        {"x", "WAIT"},
        {" ", "WAIT"},
        {"RIGHT", "LEFT", "UP", "DOWN", "WAIT"},
        {"b", "y", "t", "WAIT"},
        {"c", "DOWN", "DOWN", "DOWN", "RET", "RIGHT", "RET", "WAIT"},
        {"c", "DOWN", "DOWN", "DOWN", "DOWN", "DOWN", "RET", "RET", "RET", "WAIT"},
        {"c", "UP", "RET", "RIGHT", "RIGHT", "RET", "WAIT"},
        {"u", "DOWN", "RET", "RET", "RET", "WAIT"},
        {"u", "DOWN", "DOWN", "DOWN", "RET", "RIGHT", "RET", "WAIT"},
        {"r", "DOWN", "DOWN", "RET", "WAIT", "MIC"},
        {"a", "RIGHT", "RET", "WAIT"},
        {"a", "RET", "WAIT"},
        {"a", "a", "WAIT"},
        {"z", "z", "WAIT"},
        {"1", "RET", "WAIT"},
        // SPP1 C: the arena's J / P / Y
        {"j", "RIGHT", "WAIT"},
        {"j", "MIC", "WAIT"},
        {"j", "BS", "RET", "WAIT"},
        {"p", "RIGHT", "WAIT"},
        {"p", "UP", "WAIT"},
        {"p", "MIC", "WAIT"},
        {"y", "o", "r", "t", "RET", "WAIT"},
        {"y", "RET", "WAIT"},
        {"y", "MIC", "WAIT"},
        {"^K", "WAIT"},
    };
    for (size_t s = 0; s < scripts.size(); ++s) {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!enter_arena(h)) { std::printf("B no arena\n"); return; }
        Probe p(h, "B-" + std::to_string(s));
        p.script(scripts[s]);
        p.script({"a", "RIGHT", "RET", "WAIT", "a", "RIGHT", "RET", "WAIT", "a", "LEFT", "RET", "WAIT"});
    }
}

// --- C: shops (dialogs 0x81..0x88) and an ordinary conversation, by Talk ---
const NpcSlot *find_slot(uint8_t dialog, int &location, int &hour) {
    for (size_t l = 0; l < pack->location_count; ++l) {
        const auto &n = pack->npc_locations[l];
        for (size_t i = 0; i < n.count; ++i) {
            if (n.slots[i].dialog != dialog) continue;
            for (int hr = 8; hr < 20; ++hr)
                if (shop_is_open(n.slots[i].times, uint8_t(hr))) {
                    location = int(l) + 1;
                    hour = hr;
                    return &n.slots[i];
                }
        }
    }
    return nullptr;
}
const NpcActor *find_actor(Run &h, int slot) {
    for (size_t i = 0; i < h.rt->actors().count; ++i) {
        const auto &a = h.rt->actors().actors[i];
        if (a.location == h.rt->game().position.map.location && a.schedule.slot == slot) return &a;
    }
    return nullptr;
}
bool talk_to(Run &h, int location, int hour, int slot) {
    auto &g = h.rt->game();
    g.time.hour = uint8_t(hour);
    g.position.map = {0, 0};
    g.transport = TransportMode::Foot;
    h.rt->turn().transport_tile = 0x1c;
    g.party.active_character = 0;
    g.position.xy = {uint8_t(pack->location_x[location - 1]), uint8_t(pack->location_y[location - 1])};
    h.key('e');
    h.run(400);
    if (g.position.map.location != location) return false;
    const auto *a = find_actor(h, slot);
    if (!a) return false;
    constexpr Direction dirs[] = {Direction::South, Direction::North, Direction::East, Direction::West};
    for (auto d : dirs) {
        const auto dd = direction_delta(d);
        const int x = a->x + dd.dx, y = a->y + dd.dy;
        const int t = ctx(h).terrain->effective(ctx(h).world, g.position.map, x, y);
        if (!is_passable(t, TransportMode::Foot).value || ctx(h).shop_services->occupied(ctx(h).shop_services->context, x, y))
            continue;
        g.position.xy = {uint8_t(x), uint8_t(y)};
        h.render(true);
        h.key('t');
        h.ball(d == Direction::South ? RawInputKind::TrackballUp : d == Direction::North ? RawInputKind::TrackballDown
               : d == Direction::East ? RawInputKind::TrackballLeft : RawInputKind::TrackballRight);
        h.run(400);
        return true;
    }
    return false;
}
void walk_shops() {
    const std::vector<std::vector<std::string>> scripts = {
        {"y", "b", "RET", "a", "RET", "y", "y", "n", "MIC", "MIC", "MIC"},
        {"y", "s", "RET", "DOWN", "RET", "y", "n", "MIC", "MIC"},
        {"y", "h", "a", "y", "y", "c", "b", "y", "n", "MIC", "MIC"},
        {"n", "MIC", "MIC", "RET"},
        {"y", "1", "RET", "2", "RET", "y", "n", "MIC", "MIC"},
        {"MIC", "MIC", "MIC"},
        {"y", "DOWN", "DOWN", "CLICK", "y", "BS", "MIC", "MIC"},
        {"y", "r", "a", "y", "MIC", "MIC"},
        // SPP1 B: served, then a rumour (the barkeeper), and the same keys at every other shop
        {"y", "d", "y", "y", "t", "b", "r", "i", "t", "RET", "y", "WAIT", "MIC", "MIC"},
        {"y", "d", "y", "y", "t", "b", "r", "i", "t", "RET", "n", "WAIT", "MIC", "MIC"},
        {"y", "a", "y", "y", "t", "h", "o", "n", "e", "RET", "y", "WAIT", "y", "MIC"},
    };
    for (uint8_t dialog = 0x81; dialog <= 0x88; ++dialog) {
        int location = 0, hour = 0;
        const NpcSlot *slot = find_slot(dialog, location, hour);
        if (!slot) { std::printf("C-%02x none\n", dialog); continue; }
        for (size_t s = 0; s < scripts.size(); ++s) {
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            h.rt->game().gold = 5000;
            h.rt->game().party.characters[1].current_hp = 20;
            h.rt->game().party.characters[2].status = 'P';
            char name[32];
            std::snprintf(name, sizeof name, "C-%02x-%zu", dialog, s);
            if (!talk_to(h, location, hour, slot->slot)) { std::printf("%s no talk\n", name); continue; }
            Probe p(h, name);
            p.script(scripts[s]);
        }
    }
    // An ordinary conversation: the first non-shop NPC of the first town.
    for (size_t l = 0; l < 4; ++l) {
        const auto &n = pack->npc_locations[l];
        for (size_t i = 0; i < n.count && i < 6; ++i) {
            if (!n.slots[i].dialog || n.slots[i].dialog >= 0x81) continue;
            int hour = -1;
            for (int hr = 9; hr < 18 && hour < 0; ++hr) hour = hr;
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            if (!talk_to(h, int(l) + 1, hour, n.slots[i].slot)) continue;
            Probe p(h, "T-" + std::to_string(l) + "-" + std::to_string(i));
            p.script({"n", "a", "m", "e", "RET", "WAIT", "RET", "j", "o", "b", "RET", "WAIT", "RET", "BS", "x", "RET",
                      "RET", "MIC", "RET", "b", "y", "e", "RET", "WAIT", "RET", "MIC"});
        }
    }
}

// --- D: towns and dungeons, letter by letter with two scripts ---
void walk_places() {
    struct Place { const char *name; DebugDestinationKind kind; int location; int floor; };
    const Place places[] = {{"Dtown", DebugDestinationKind::SmallMap, 1, 0},
                            {"Dpalace", DebugDestinationKind::SmallMap, 18, 0},
                            {"Ddun", DebugDestinationKind::Dungeon, 33, 0},
                            {"Ddun40", DebugDestinationKind::Dungeon, 40, 2}};
    const char *letters = "abcdefghijklmnopqrstuvwxyz";
    for (const auto &pl : places)
        for (const char *L = letters; *L; ++L)
            for (size_t s : {size_t(0), size_t(1), size_t(3), size_t(6)}) {
                Run h(kParty, g_paced, &g_audio);
                stock(h);
                if (!teleport(h, pl.kind, pl.location, pl.floor)) { std::printf("%s no teleport\n", pl.name); break; }
                Probe p(h, std::string(pl.name) + "-" + *L + "-" + std::to_string(s));
                p.input(std::string(1, *L));
                p.script(kScripts[s]);
            }
}

// --- G: SPP1 E -- the dungeon (L)ook with its member pick and direction, and (D)rink ---
void walk_dungeon_look() {
    const std::vector<std::vector<std::string>> scripts = {
        {"l", "RET", "UP", "WAIT"},
        {"l", "RET", "RIGHT", "WAIT"},
        {"l", "RET", "DOWN", "WAIT"},
        {"l", "RET", "CLICK", "WAIT"},
        {"l", "RET", "MIC", "WAIT"},
        {"l", "MIC", "WAIT"},
        {"l", "DOWN", "RET", "LEFT", "WAIT"},
        {"d", "RET", "y", "WAIT"},
        {"d", "RET", "n", "WAIT"},
        {"d", "MIC", "WAIT"},
        {"^K", "WAIT"},
        {"^V", "WAIT"},
    };
    for (int where : {33, 40})
        for (size_t s = 0; s < scripts.size(); ++s) {
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            if (!teleport(h, DebugDestinationKind::Dungeon, where, where == 40 ? 2 : 0)) { std::printf("G no teleport\n"); return; }
            Probe p(h, "G-" + std::to_string(where) + "-" + std::to_string(s));
            p.script(scripts[s]);
        }
}

// --- H: SPP1 G / A -- Ctrl-K / Ctrl-V and the dispatcher's turn on the surface and in a town ---
void walk_controls() {
    const std::vector<std::vector<std::string>> scripts = {
        {"^K", "WAIT", "RET"},
        {"^V", "WAIT", "RET"},
        {"^K", "^V", "a", "RIGHT", "WAIT"},
        {"r", "RET", "RET", "RET", "WAIT"},
        {"z", "RET", "MIC", "MIC", "WAIT"},
        {"n", "1", "2", "RET", "WAIT"},
        {"u", "RET", "RET", "WAIT"},
        {"x", "WAIT"},
        {"m", "RET", "RET", "WAIT"},
    };
    for (int place = 0; place < 2; ++place)
        for (size_t s = 0; s < scripts.size(); ++s) {
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            if (place == 0 ? !to_grass(h) : !teleport(h, DebugDestinationKind::SmallMap, 1)) { std::printf("H no place\n"); return; }
            Probe p(h, "H-" + std::to_string(place) + "-" + std::to_string(s));
            p.script(scripts[s]);
        }
}

// --- E: the system menu, saving, loading, settings and the camp ---
void walk_menus() {
    const std::vector<std::vector<std::string>> scripts = {
        {"ALT-S", "WAIT", "RET", "WAIT", "ALT-L", "WAIT", "RET", "WAIT", "RET"},
        {"ALT-M", "DOWN", "RET", "WAIT", "RET", "MIC", "MIC"},
        {"ALT-M", "DOWN", "DOWN", "RET", "DOWN", "RET", "WAIT", "MIC", "MIC"},
        {"ALT-M", "DOWN", "DOWN", "DOWN", "RET", "DOWN", "DOWN", "RIGHT", "LEFT", "RET", "MIC", "MIC"},
        {"ALT-M", "UP", "UP", "RET", "WAIT", "MIC"},
        {"ALT-M", "UP", "RET", "WAIT", "RET", "WAIT"},
        {"q", "y", "WAIT", "RET", "MIC"},
        {"q", "n", "MIC"},
        {"h", "3", "RET", "n", "WAIT", "WAIT", "RET"},
        {"h", "5", "RET", "y", "1", "WAIT", "WAIT", "RET"},
        {"h", "MIC", "RET"},
        {"n", "1", "3", "RET", "MIC"},
        {"n", "2", "MIC", "RET"},
        {"z", "RIGHT", "RIGHT", "DOWN", "RET", "LEFT", "MIC"},
        {"y", "h", "e", "l", "l", "o", "RET", "WAIT"},
        {"y", "RET", "WAIT"},
        {"v", "WAIT", "RET"},
        {"p", "RET", "WAIT"},
    };
    for (size_t s = 0; s < scripts.size(); ++s) {
        tdeck::host_memory_save_forget_for_test();
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!to_grass(h)) return;
        Probe p(h, "E-" + std::to_string(s));
        p.script(scripts[s]);
    }
    tdeck::host_memory_save_forget_for_test();
}
// --- F: the shrines: (E)nter, Visit?, virtue, mantra, cycles ---
std::string ascii(TalkText t) {
    std::string o;
    for (char16_t c : t) o += char(c < 128 ? c : '?');
    return o;
}
void walk_shrines() {
    const auto &sh = pack->shrine_data;
    for (int v = 0; v < sh.count && v < 3; ++v)
        for (int variant = 0; variant < 5; ++variant) {
            Run h(kParty, g_paced, &g_audio);
            stock(h);
            auto &g = h.rt->game();
            if (variant >= 3) { g.quest.shrine_quest = 1; g.quest.shrine_visited = 0xff; }
            g.position.map = {0, 0};
            g.transport = TransportMode::Foot;
            h.rt->turn().transport_tile = 0x1c;
            g.party.active_character = 0;
            g.position.xy = {uint8_t(sh.x[v]), uint8_t(sh.y[v])};
            h.render(true);
            h.run(200);
            Probe p(h, "F-" + std::to_string(v) + "-" + std::to_string(variant));
            p.input("e");
            std::vector<std::string> keys;
            const std::string virtue = ascii(sh.virtues[v]), mantra = ascii(sh.mantras[v]);
            if (variant == 1) { p.script({"n", "WAIT", "RET"}); continue; }
            keys.push_back("y");
            for (char c : (variant == 2 ? std::string("wrong") : virtue)) keys.push_back(std::string(1, char(c | 0x20)));
            keys.push_back("RET");
            for (char c : mantra) keys.push_back(std::string(1, char(c | 0x20)));
            keys.push_back("BS");
            keys.push_back(std::string(1, char(mantra.back() | 0x20)));
            keys.push_back("RET");
            if (variant == 4) { keys.push_back("3"); keys.push_back("RET"); }
            for (int i = 0; i < 6; ++i) { keys.push_back("WAIT"); keys.push_back("RET"); }
            keys.push_back("MIC");
            p.script(keys);
        }
}
} // namespace

// --- I (SPP2): the walks that reach what SPP2 changes on the device -------------------------
// A Mix to completion and the View scroll's reveal (commands that end on a timer: their turn at
// the hold's end), a Hole up cancelled (D-107 (2), deferred: no change expected), a town Talk at
// an empty cell (D-111) and an R cancelled beside an approaching NPC (the turn's approach).
void walk_spp2() {
    for (int s = 0; s < 3; ++s) {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!to_grass(h)) { std::printf("I no grass\n"); return; }
        auto &g = h.rt->game();
        for (auto &r : g.reagent_quantities) r = 0;
        g.reagent_quantities[0] = 5;
        Probe p(h, "I-mix-" + std::to_string(s));
        p.script({"m", "RET", "RET", "m", "1", "RET"});
        if (s == 0) p.script({"WAIT", "RET"});
        else if (s == 1) p.script({"RET", "WAIT"});
        else p.script({"WAIT", "WAIT", "z", "MIC"});
    }
    {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!to_grass(h)) return;
        auto &g = h.rt->game();
        for (auto &q : g.scroll_quantities) q = 0;
        for (auto &q : g.potion_quantities) q = 0;
        g.scroll_quantities[4] = 1;
        Probe p(h, "I-view");
        p.script({"u", "RET", "WAIT", "WAIT", "RET"});
    }
    for (int s = 0; s < 2; ++s) {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (s ? !teleport(h, DebugDestinationKind::SmallMap, 2) : !to_grass(h)) return;
        Probe p(h, std::string("I-hole-") + (s ? "town" : "world"));
        p.script({"h", "MIC", "WAIT"});
    }
    {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!teleport(h, DebugDestinationKind::SmallMap, 2)) return;
        auto &g = h.rt->game();
        auto &c = ctx(h);
        int nx = -1, ny = -1;
        for (int y = 2; y < 30 && nx < 0; ++y)
            for (int x = 2; x < 30 && nx < 0; ++x) {
                bool ok = is_passable(c.terrain->effective(c.world, g.position.map, x, y), TransportMode::Foot).value &&
                          is_passable(c.terrain->effective(c.world, g.position.map, x + 1, y), TransportMode::Foot).value &&
                          !c.shop_services->occupied(c.shop_services->context, x, y) &&
                          !c.shop_services->occupied(c.shop_services->context, x + 1, y);
                for (size_t i = 0; i < c.actors->count && ok; ++i)
                    ok = !(c.actors->actors[i].location == 2 && std::abs(c.actors->actors[i].x - x) + std::abs(c.actors->actors[i].y - y) <= 4);
                if (ok) { nx = x; ny = y; }
            }
        if (nx < 0) return;
        g.position.xy = {uint8_t(nx), uint8_t(ny)};
        h.run(100);
        Probe p(h, "I-talk-empty");
        p.script({"t", "RIGHT", "WAIT", "RET"});
    }
    {
        Run h(kParty, g_paced, &g_audio);
        stock(h);
        if (!teleport(h, DebugDestinationKind::SmallMap, 2)) return;
        auto &g = h.rt->game();
        auto &c = ctx(h);
        bool placed = false;
        for (size_t i = 0; i < c.actors->count && !placed; ++i) {
            auto &a = c.actors->actors[i];
            if (a.location != 2 || a.z != g.position.map.floor || a.schedule.dialog == 0 || a.schedule.dialog >= 128) continue;
            for (const auto d : {Direction::South, Direction::North, Direction::East, Direction::West}) {
                const auto dd = direction_delta(d);
                const int x = a.x + dd.dx, y = a.y + dd.dy;
                if (!is_passable(c.terrain->effective(c.world, g.position.map, x, y), TransportMode::Foot).value ||
                    c.shop_services->occupied(c.shop_services->context, x, y)) continue;
                g.position.xy = {uint8_t(x), uint8_t(y)};
                for (auto &ai : a.schedule.ai) ai = 4;
                placed = true;
                break;
            }
        }
        if (!placed) return;
        h.run(100);
        Probe p(h, "I-approach");
        p.script({"r", "MIC", "WAIT", "MIC", "MIC"});
    }
}

int main(int argc, char **argv) {
    if (argc < 3 || !load_pack(argv[1])) {
        std::printf("usage: tdeck_diff_probe <pack> <openu5-audio.bin>\n");
        return 2;
    }
    g_audio = tdeck::load_audio_pack_info(argv[2]);
    if (g_audio.state != AudioPackState::Valid) {
        std::printf("audio pack invalid\n");
        return 2;
    }
#ifdef PROBE_DOS
    // Positive control (HEAD only): the Windows Original profile's 1988 switches.
    if (argc > 4 && !std::strcmp(argv[4], "--dos")) {
        PlatformProfile p{};
        p.typed_spell_names = [](void *) { return true; };
        p.quit_save_key = [](void *) { return true; };
        p.dos_interface = [](void *) { return true; };
        set_platform_profile(p);
    }
#endif
    for (int i = 3; i < argc; ++i) g_paced = g_paced || !std::strcmp(argv[i], "--paced");
    const char *only = argc > 3 && argv[3][0] != '-' ? argv[3] : "ABCDEFGHI";
    if (std::strchr(only, 'A')) { walk_world(false); walk_world(true); }
    if (std::strchr(only, 'B')) walk_arena();
    if (std::strchr(only, 'C')) walk_shops();
    if (std::strchr(only, 'D')) walk_places();
    if (std::strchr(only, 'E')) walk_menus();
    if (std::strchr(only, 'F')) walk_shrines();
    if (std::strchr(only, 'G')) walk_dungeon_look();
    if (std::strchr(only, 'H')) walk_controls();
    if (std::strchr(only, 'I')) walk_spp2();
    std::printf("PROBE DONE\n");
    return 0;
}
