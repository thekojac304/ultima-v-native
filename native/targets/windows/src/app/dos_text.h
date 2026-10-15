#pragma once
// Ultima V Native for Windows: the DOS game's own short names and glyphs for
// the 1988 right-panel lists (openu5::DosText), for Original mode.
//
// The names are the player's: the string tables are read from the resource
// pack's data.json (the extractor's dump of the player's DATA.OVL, identity-
// locked with the pack) -- the potion colours, the reagents, the spells, the
// (U)se entries and the DGROUP literals by address. The (R)eady picker's two
// equipment tables (DS 0x1962 names, DS 0x1ae8 glyphs) are pointer/byte tables
// data.json does not carry; they come from the copies the repository already
// tracks for the web port, generated into the build tree (cmake/DosTables.cmake).
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace tdeck {
class AlphaResourcePack;
}

namespace uv {

class DosTextTables {
  public:
    /** Reads data.json from an open pack; false (and empty tables) if it cannot. */
    bool load(const tdeck::AlphaResourcePack &pack);
    /** Parses a data.json document (exposed for the tests). */
    bool load_json(const std::string &json);
    /** openu5::DosText lookup: nullptr when the table has no such entry. */
    const char *text(uint8_t table, int index) const;
    bool loaded() const { return loaded_; }

  private:
    bool loaded_ = false;
    std::vector<std::string> potions_, reagents_, spells_, use_items_;
    std::map<int, std::string> literals_; // by DS address
    std::string glyph_[48];
};

} // namespace uv
