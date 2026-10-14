#include "openu5/spell_entry.h"

#include <cstring>

#include "openu5/magic.h"

namespace openu5 {
namespace {
// DS:0x1b7a, indexed by letter (re/notes/cast-input.md section 3). J and O
// have no rune: their pointers are null, and CAST2.OVL 0x00fa/0x0101 skip them.
constexpr const char *kRuneWords[26] = {
    "AN", "BET", "CORP", "DES", "EX", "FLAM", "GRAV", "HUR", "IN", nullptr, "KAL", "LOR", "MANI",
    "NOX", nullptr, "POR", "QUAS", "REL", "SANCT", "TYM", "UUS", "VAS", "WIS", "XEN", "YLEM", "ZU",
};

char upper(char c) { return c >= 'a' && c <= 'z' ? char(c - 'a' + 'A') : c; }

// At most eight letters: an insertion sort. A library sort over these tiny
// arrays trips GCC 16's -Warray-bounds at -O3 (a false positive, but the
// host and firmware builds use -Werror).
void sort_letters(char *p, size_t n) {
    for (size_t i = 1; i < n; ++i)
        for (size_t j = i; j > 0 && p[j - 1] > p[j]; --j) {
            const char t = p[j];
            p[j] = p[j - 1];
            p[j - 1] = t;
        }
}

/** A spell name's syllable initials, sorted: "In Vas Por Ylem" -> "IPVY". */
size_t sorted_initials(const char *name, char *out, size_t capacity) {
    size_t n = 0;
    bool word_start = true;
    for (const char *p = name; p && *p; ++p) {
        if (*p == ' ') {
            word_start = true;
            continue;
        }
        if (word_start && n < capacity) out[n++] = upper(*p);
        word_start = false;
    }
    sort_letters(out, n);
    return n;
}
} // namespace

const char *rune_word_for_initial(char initial) {
    const char c = upper(initial);
    return c >= 'A' && c <= 'Z' ? kRuneWords[c - 'A'] : nullptr;
}

int32_t spell_from_initials(const char *initials, size_t count) {
    if (!initials || count == 0) return kSpellEntryNone;
    if (count > kSpellEntryMaxSyllables) return kSpellEntryNoMatch;
    char typed[kSpellEntryMaxSyllables]{};
    for (size_t i = 0; i < count; ++i) typed[i] = upper(initials[i]);
    sort_letters(typed, count);
    // CAST2.OVL 0x01e2: the sorted entry against the 48 pre-sorted rows.
    for (int32_t spell = 0; spell < 48; ++spell) {
        const auto *definition = spell_definition(SpellId(spell));
        if (!definition || !definition->name) continue;
        char want[8]{};
        const size_t n = sorted_initials(definition->name, want, sizeof(want));
        if (n == count && std::memcmp(want, typed, count) == 0) return spell;
    }
    return kSpellEntryNoMatch;
}

size_t spell_entry_echo(const char *initials, size_t count, char *out, size_t capacity) {
    if (!out || !capacity) return 0;
    size_t n = 0;
    out[0] = 0;
    for (size_t i = 0; i < count && initials; ++i) {
        const char *word = rune_word_for_initial(initials[i]);
        for (const char *p = word; p && *p && n + 1 < capacity; ++p) out[n++] = *p;
        if (word && n + 1 < capacity) out[n++] = ' ';
    }
    out[n] = 0;
    return n;
}

size_t spell_entry_initials(const char16_t *echo, size_t length, char *out, size_t capacity) {
    size_t n = 0;
    bool word_start = true;
    for (size_t i = 0; echo && i < length; ++i) {
        const char16_t c = echo[i];
        if (c == u' ') {
            word_start = true;
            continue;
        }
        if (word_start && n < capacity && c < 0x80) out[n++] = upper(char(c));
        word_start = false;
    }
    return n;
}

} // namespace openu5
