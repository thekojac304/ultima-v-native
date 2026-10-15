#include "app/dos_text.h"

#include <cstdlib>
#include <memory>

#include "alpha_resources.h"
#include "openu5/platform_profile.h"

namespace uv {
namespace {

#include "dos_equipment_tables.inc"

// A minimal JSON reader for data.json: objects, arrays, strings, numbers and
// literals, kept as a small tree. Strings are bytes: \uXXXX below 0x100 is
// that byte (the dump escapes DATA.OVL's control bytes, e.g. "\u001c + ").
struct Json {
    enum Kind { Null, Bool, Number, String, Array, Object } kind = Null;
    double number = 0;
    std::string text;
    std::vector<std::unique_ptr<Json>> items;
    std::vector<std::string> keys; // Object: keys[i] names items[i]
    const Json *get(const char *key) const {
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == key) return items[i].get();
        return nullptr;
    }
};

class Reader {
  public:
    explicit Reader(const std::string &s) : s_(s) {}
    std::unique_ptr<Json> value() {
        space();
        if (i_ >= s_.size()) return nullptr;
        auto v = std::make_unique<Json>();
        const char c = s_[i_];
        if (c == '{') {
            v->kind = Json::Object;
            ++i_;
            space();
            if (peek('}')) { ++i_; return v; }
            for (;;) {
                space();
                std::string key;
                if (!string(key)) return nullptr;
                space();
                if (!peek(':')) return nullptr;
                ++i_;
                auto item = value();
                if (!item) return nullptr;
                v->keys.push_back(std::move(key));
                v->items.push_back(std::move(item));
                space();
                if (peek(',')) { ++i_; continue; }
                if (peek('}')) { ++i_; return v; }
                return nullptr;
            }
        }
        if (c == '[') {
            v->kind = Json::Array;
            ++i_;
            space();
            if (peek(']')) { ++i_; return v; }
            for (;;) {
                auto item = value();
                if (!item) return nullptr;
                v->items.push_back(std::move(item));
                space();
                if (peek(',')) { ++i_; continue; }
                if (peek(']')) { ++i_; return v; }
                return nullptr;
            }
        }
        if (c == '"') {
            v->kind = Json::String;
            return string(v->text) ? std::move(v) : nullptr;
        }
        if (s_.compare(i_, 4, "true") == 0) { v->kind = Json::Bool; v->number = 1; i_ += 4; return v; }
        if (s_.compare(i_, 5, "false") == 0) { v->kind = Json::Bool; i_ += 5; return v; }
        if (s_.compare(i_, 4, "null") == 0) { i_ += 4; return v; }
        char *end = nullptr;
        v->kind = Json::Number;
        v->number = std::strtod(s_.c_str() + i_, &end);
        if (end == s_.c_str() + i_) return nullptr;
        i_ = size_t(end - s_.c_str());
        return v;
    }

  private:
    void space() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\n' || s_[i_] == '\r' || s_[i_] == '\t')) ++i_;
    }
    bool peek(char c) const { return i_ < s_.size() && s_[i_] == c; }
    bool string(std::string &out) {
        if (!peek('"')) return false;
        ++i_;
        while (i_ < s_.size()) {
            const char c = s_[i_++];
            if (c == '"') return true;
            if (c != '\\') { out.push_back(c); continue; }
            if (i_ >= s_.size()) return false;
            const char e = s_[i_++];
            switch (e) {
            case 'n': out.push_back('\n'); break;
            case 'r': out.push_back('\r'); break;
            case 't': out.push_back('\t'); break;
            case 'b': out.push_back('\b'); break;
            case 'f': out.push_back('\f'); break;
            case 'u': {
                if (i_ + 4 > s_.size()) return false;
                const unsigned long code = std::strtoul(s_.substr(i_, 4).c_str(), nullptr, 16);
                i_ += 4;
                out.push_back(char(code < 0x100 ? code : '?'));
                break;
            }
            default: out.push_back(e); break;
            }
        }
        return false;
    }
    const std::string &s_;
    size_t i_ = 0;
};

std::vector<std::string> strings_of(const Json *array) {
    std::vector<std::string> out;
    if (!array || array->kind != Json::Array) return out;
    for (const auto &item : array->items) out.push_back(item->kind == Json::String ? item->text : std::string());
    return out;
}

} // namespace

bool DosTextTables::load(const tdeck::AlphaResourcePack &pack) {
    const uint32_t length = pack.entry_length("data.json");
    if (!length) return false;
    std::string json(length, '\0');
    if (pack.read_entry("data.json", json.data(), json.size()) != ESP_OK) return false;
    return load_json(json);
}

bool DosTextTables::load_json(const std::string &json) {
    *this = DosTextTables{};
    for (int i = 0; i < 48; ++i) glyph_[i] = std::string(1, char(kDosEquipmentGlyph[i]));
    Reader reader(json);
    const auto root = reader.value();
    if (!root || root->kind != Json::Object) return false;
    potions_ = strings_of(root->get("potions"));
    reagents_ = strings_of(root->get("reagents"));
    spells_ = strings_of(root->get("spells"));
    std::vector<std::string> scrolls, phases;
    // Every pool is a run of NUL-terminated DATA.OVL strings from its file
    // offset (DS + 0x10): index each string by its DS address.
    if (const Json *pools = root->get("stringPools"); pools && pools->kind == Json::Array) {
        for (const auto &pool : pools->items) {
            const Json *name = pool->get("name"), *offset = pool->get("offset");
            const auto strings = strings_of(pool->get("strings"));
            if (!offset || offset->kind != Json::Number) continue;
            int at = int(offset->number) - 0x10;
            for (const auto &s : strings) {
                literals_.emplace(at, s);
                at += int(s.size()) + 1;
            }
            if (name && name->text == "scrollSpellCodes") scrolls = strings;
            if (name && name->text == "parenDigitStrings") phases = strings;
        }
    }
    // The (U)se table, DS 0x1916 (re/notes/use-picker-panel.md section 4): the
    // eight scroll codes ("*VL"), eight potion sigils ("!"), the carpet, keys
    // and regalia, eight moonstone phases ("(0"), the shards and the six
    // tools -- 38 entries, in the extended table's id order.
    const auto special = strings_of(root->get("specialItemNames"));
    const auto shards = strings_of(root->get("shards"));
    const auto special2 = strings_of(root->get("specialItemNames2"));
    if (scrolls.size() == 8 && phases.size() == 8 && special.size() == 5 && shards.size() == 3 &&
        special2.size() == 6) {
        use_items_ = scrolls;
        for (int i = 0; i < 8; ++i) use_items_.push_back("!");
        use_items_.insert(use_items_.end(), special.begin(), special.end());
        use_items_.insert(use_items_.end(), phases.begin(), phases.end());
        use_items_.insert(use_items_.end(), shards.begin(), shards.end());
        use_items_.insert(use_items_.end(), special2.begin(), special2.end());
    }
    loaded_ = potions_.size() == 8 && reagents_.size() == 8 && spells_.size() == 48 && use_items_.size() == 38;
    return loaded_;
}

const char *DosTextTables::text(uint8_t table, int index) const {
    using openu5::DosText;
    auto at = [&](const std::vector<std::string> &v) -> const char * {
        return index >= 0 && size_t(index) < v.size() ? v[size_t(index)].c_str() : nullptr;
    };
    switch (DosText(table)) {
    case DosText::EquipmentShort: return index >= 0 && index < 48 ? kDosEquipmentShort[index] : nullptr;
    case DosText::EquipmentGlyph: return index >= 0 && index < 48 ? glyph_[index].c_str() : nullptr;
    case DosText::UseItem: return at(use_items_);
    case DosText::PotionColour: return at(potions_);
    case DosText::Reagent: return at(reagents_);
    case DosText::Spell: return at(spells_);
    case DosText::Literal: {
        const auto it = literals_.find(index);
        return it == literals_.end() ? nullptr : it->second.c_str();
    }
    }
    return nullptr;
}

} // namespace uv
