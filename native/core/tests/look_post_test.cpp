// LOOK-POLISH1 (D-117): (L)ook at a shop sign's post.
//
// LOOKOBJ look_dispatch 0x0510-0x0550, before look_generic: while the tile is
// 0xE0, 0xE1 or 0xE2 -- the cells a hanging sign's post and bracket occupy --
// it moves to the cell that tile points at (0xE0: y-1, 0xE1: x+1, 0xE2: x-1)
// and re-reads the map (get_tile_ptr, raw 0xffffa172). Every chain in the
// 1988 maps ends on a sign (0xF0..0xF9), whose LOOK2.DAT phrase is printed.
// LOOK2.DAT's own phrase for the three post tiles is a placeholder, "parched
// desert", that the game never prints. Reported on Windows Original: Britain,
// the brick road, Look North -> "Thou dost see parched desert."
//
// usage: look_post_tests <original/u5/ultima5>
#include "openu5/look.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

using namespace openu5;

namespace {
int checks = 0, failures = 0;
void check(bool value, const std::string &what) {
    ++checks;
    if (!value) {
        ++failures;
        std::printf("FAIL %s\n", what.c_str());
    }
}

std::vector<uint8_t> read_file(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// LOOK2.DAT: a table of uint16 offsets, one per tile, into a pool of NUL-ended
// phrases (extractor/src/parsers/look2.ts).
std::vector<std::string> look2_phrases(const std::vector<uint8_t> &bytes) {
    size_t pool = bytes.size();
    for (size_t i = 0; i * 2 + 1 < pool; ++i) pool = std::min<size_t>(pool, bytes[i * 2] | (bytes[i * 2 + 1] << 8));
    std::vector<std::string> out(pool / 2);
    for (size_t i = 0; i < out.size(); ++i) {
        size_t at = bytes[i * 2] | (bytes[i * 2 + 1] << 8);
        while (at < bytes.size() && bytes[at]) out[i] += char(bytes[at++]);
    }
    return out;
}

struct Probe {
    std::vector<std::string> messages;
    static void emit(void *context, const GameEvent &event) {
        if (event.kind == GameEventKind::Message) static_cast<Probe *>(context)->messages.emplace_back(event.text ? event.text : "");
    }
};

struct Look {
    const std::vector<std::string> *phrases = nullptr;
    // One Look from (x, y) toward `direction` on `map`; returns the printed line.
    std::string at(const ActiveMap &map, int x, int y, Direction direction, NpcActors *actors = nullptr) const {
        GameState game{};
        game.position = {{uint8_t(x), uint8_t(y)}, map.id};
        game.time.hour = 12;
        TurnState turn{};
        TravelState travel{};
        CommandState commands{};
        WorldData world{};
        CommandContext context{game, turn, travel, commands, world};
        LookServices services{};
        services.context = const_cast<std::vector<std::string> *>(phrases);
        services.describe = [](void *p, int32_t tile) {
            auto &v = *static_cast<std::vector<std::string> *>(p);
            return tile >= 0 && size_t(tile) < v.size() ? v[size_t(tile)].c_str() : "";
        };
        context.look = &services;
        context.actors = actors;
        Probe probe;
        Command command{};
        command.kind = CommandKind::Look;
        command.direction = direction;
        command.has_direction = true;
        const auto status = world_look(context, command, map, {&probe, Probe::emit}, rng_source(game.rng));
        if (status != CommandStatus::Success) return "<status " + std::to_string(int(status)) + ">";
        std::string all;
        for (const auto &m : probe.messages) all += (all.empty() ? "" : " / ") + m;
        return all;
    }
};

ActiveMap small_map(const uint8_t *tiles, uint8_t location, int8_t floor) {
    ActiveMap map{};
    map.id = {LocationId(location), floor};
    map.kind = MapKind::Small;
    map.geometry = {32, 32, false};
    map.tiles = tiles;
    map.edge_fill_tile = tiles[1023];
    return map;
}
} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: look_post_tests <original/u5/ultima5>\n");
        return 2;
    }
    const std::string dir = argv[1];
    const auto look2 = look2_phrases(read_file(dir + "/LOOK2.DAT"));
    const auto towne = read_file(dir + "/TOWNE.DAT");
    if (look2.size() != 512 || towne.size() != 16384) {
        std::printf("RED the game files are missing or not the 1988 ones (LOOK2.DAT %zu phrases, TOWNE.DAT %zu B)\n",
                    look2.size(), towne.size());
        return 1;
    }
    // The placeholders this batch is about: LOOK2.DAT says "parched desert" for
    // the three post tiles, and names the signs and the flagpole.
    check(look2[0xe0] == "parched desert" && look2[0xe1] == "parched desert" && look2[0xe2] == "parched desert",
          "L0 LOOK2.DAT's phrase for 0xE0..0xE2 is the placeholder 'parched desert'");
    check(look2[0xf2] == "the sign of an armoury" && look2[0xf0] == "the sign of a provisioner" &&
              look2[0xf6] == "the sign of an inn",
          "L0 LOOK2.DAT names the signs");
    const Look look{&look2};

    // L1 -- the report. Britain (TOWNE.DAT map 2, location 2 floor 0): the
    // armoury's sign hangs at (9,19) over its posts (8,19) 0xE1, (10,19) 0xE2 and
    // (8..10,20) 0xE0; the road runs below them on row 21.
    const ActiveMap britain = small_map(towne.data() + 2 * 1024, 2, 0);
    check(britain.tile_at(9, 21) == 0x44 && britain.tile_at(9, 20) == 0xe0 && britain.tile_at(9, 19) == 0xf2,
          "L1 Britain's map has the road at (9,21), a post at (9,20) and the armoury sign at (9,19)");
    {
        const std::string seen = look.at(britain, 9, 21, Direction::North);
        check(seen == "Thou dost see the sign of an armoury",
              "L1 Britain (9,21) Look North -> 'Thou dost see the sign of an armoury' (was: '" + seen + "')");
        check(seen.find("parched desert") == std::string::npos, "L1 never 'parched desert' at Britain's posts");
    }
    // The posts beside it chase twice: (8,20) 0xE0 -> (8,19) 0xE1 -> (9,19).
    check(look.at(britain, 8, 21, Direction::North) == "Thou dost see the sign of an armoury",
          "L1 Britain (8,21) North: 0xE0 then 0xE1 -> the armoury sign");
    check(look.at(britain, 10, 21, Direction::North) == "Thou dost see the sign of an armoury",
          "L1 Britain (10,21) North: 0xE0 then 0xE2 -> the armoury sign");

    // L2 -- all four directions, each onto a different post tile.
    check(look.at(britain, 8, 18, Direction::South) == "Thou dost see the sign of an armoury",
          "L2 South onto 0xE1 (8,19) -> x+1 -> the armoury sign");
    check(look.at(britain, 11, 8, Direction::East) == "Thou dost see the sign of a provisioner",
          "L2 East onto 0xE1 (12,8) -> x+1 -> the provisioner's sign (13,8)");
    check(look.at(britain, 19, 8, Direction::West) == "Thou dost see the sign of an inn",
          "L2 West onto 0xE2 (18,8) -> x-1 -> the inn's sign (17,8)");
    check(look.at(britain, 12, 10, Direction::North) == "Thou dost see the sign of a provisioner",
          "L2 North onto 0xE0 (12,9) -> y-1 -> 0xE1 -> x+1 -> the provisioner's sign");
    // Control: Look at the sign itself and at ordinary terrain is unchanged.
    check(look.at(britain, 9, 20, Direction::North) == "Thou dost see the sign of an armoury",
          "L2 control: the sign itself");
    check(look.at(britain, 15, 21, Direction::North) == "Thou dost see cobble", "L2 control: the road ahead is cobble");
    check(look.at(britain, 12, 21, Direction::North) == "Thou dost see grass", "L2 control: grass is grass");

    // L3 -- every post in every 1988 town, castle and keep map: Look at it from the
    // cell below (or above, on the last row) prints its chain's sign, as the
    // binary does; never the placeholder.
    int posts = 0;
    for (const char *file : {"TOWNE.DAT", "CASTLE.DAT", "KEEP.DAT", "DWELLING.DAT"}) {
        const auto bytes = read_file(dir + "/" + file);
        check(bytes.size() == 16384, std::string("L3 ") + file + " is 16 maps");
        for (size_t m = 0; m < bytes.size() / 1024; ++m) {
            const ActiveMap map = small_map(bytes.data() + m * 1024, uint8_t(1 + m / 2), int8_t(m % 2));
            for (int y = 0; y < 32; ++y)
                for (int x = 0; x < 32; ++x) {
                    int tile = map.tile_at(x, y);
                    if (tile < 0xe0 || tile > 0xe2) continue;
                    ++posts;
                    int cx = x, cy = y;
                    while (tile >= 0xe0 && tile <= 0xe2) {
                        if (tile == 0xe0) --cy;
                        else if (tile == 0xe1) ++cx;
                        else --cx;
                        tile = map.tile_at(cx, cy);
                    }
                    const bool below = y < 31;
                    const std::string seen = look.at(map, x, below ? y + 1 : y - 1, below ? Direction::North : Direction::South);
                    char where[64];
                    std::snprintf(where, sizeof(where), "%s map %zu (%d,%d)", file, m, x, y);
                    check(tile >= 0xf0 && tile <= 0xf9 && tile != 0xf8,
                          std::string("L3 the chain from ") + where + " ends on a sign");
                    check(tile >= 0 && seen == "Thou dost see " + look2[size_t(tile)],
                          std::string("L3 ") + where + " -> '" + seen + "'");
                }
        }
    }
    check(posts == 122, "L3 the 1988 maps hold 122 post cells (TOWNE 71, CASTLE 45, KEEP 6) (counted " + std::to_string(posts) + ")");

    // L4 -- boundaries. The overworld wraps: a post on row 0 points at row 255.
    {
        std::vector<uint8_t> world(65536, 5);
        world[0 * 256 + 7] = 0xe0;
        world[255 * 256 + 7] = 0xf3;
        world[100 * 256 + 255] = 0xe1;
        world[100 * 256 + 0] = 0xf7;
        ActiveMap map{{0, 0}, MapKind::Overworld, {256, 256, true}, world.data(), -1};
        check(look.at(map, 7, 1, Direction::North) == "Thou dost see the sign of a healer",
              "L4 the overworld wraps the chase north: (7,0) 0xE0 -> (7,255)");
        check(look.at(map, 254, 100, Direction::East) == "Thou dost see the sign of an apothecary",
              "L4 the overworld wraps the chase east: (255,100) 0xE1 -> (0,100)");
        // The chased cell is wrapped before the actors are read: a figure there is found.
        NpcActors edge_actors{};
        edge_actors.count = 1;
        edge_actors.actors[0].location = 0;
        edge_actors.actors[0].z = 0;
        edge_actors.actors[0].x = 7;
        edge_actors.actors[0].y = 255;
        edge_actors.actors[0].schedule.type = 1;
        check(look.at(map, 7, 1, Direction::North, &edge_actors) == "Thou dost see a chest",
              "L4 the wrapped cell's actor is what the chase sees: (7,0) 0xE0 -> (7,255)");
        // A town's edge: 0xE0 on row 0 points off the map; the read is off-map and
        // the chase stops there (no 1988 map has such a post).
        std::vector<uint8_t> town(1024, 0x44);
        town[0 * 32 + 4] = 0xe0;
        const ActiveMap edge = small_map(town.data(), 9, 0);
        const std::string seen = look.at(edge, 4, 1, Direction::North);
        check(seen == "Thou dost see darkness.", "L4 a post pointing off a town's map stops off-map ('" + seen + "')");
        // A loop the 1988 data never has (0xE1 beside 0xE2) must not hang the game.
        town[5 * 32 + 4] = 0xe1;
        town[5 * 32 + 5] = 0xe2;
        const std::string loop = look.at(edge, 4, 6, Direction::North);
        check(loop.rfind("Thou dost see", 0) == 0, "L4 a post loop ends ('" + loop + "')");
    }

    // L5 -- the map is re-read with the actors on it, as the first read is
    // (get_tile_ptr is the composited view; game.ts look() npcAt): a figure on
    // the sign's cell is what the chase finds.
    {
        std::vector<uint8_t> town(towne.begin() + 2 * 1024, towne.begin() + 3 * 1024);
        const ActiveMap map = small_map(town.data(), 2, 0);
        NpcActors actors{};
        auto &actor = actors.actors[0];
        actors.count = 1;
        actor.location = 2;
        actor.z = 0;
        actor.x = 9;
        actor.y = 19;
        actor.schedule.type = 1; // LOOK2.DAT's second table, tile 0x101: "a chest"
        check(look.at(map, 9, 21, Direction::North, &actors) == "Thou dost see a chest",
              "L5 the chase re-reads the composited map: an actor on the sign's cell");
    }

    std::printf("LOOK-POLISH1 look_post: %d/%d checks\n", checks - failures, checks);
    return failures ? 1 : 0;
}
