#pragma once
#include <cstddef>
#include <cstdint>

namespace openu5 {

// Windows port, desktop Original mode (WINDOWS_PORT_PLAN.md): the 1988 game's
// typed spell name, CAST2.OVL:0x00DE (re/notes/cast-input.md). Cast's
// "Spell name:" and Mix's "For what spell?" both read it: the player types the
// INITIAL of each syllable (at most four; A-Z except J and O, which have no
// rune), the game echoes each syllable's rune word in capitals, Backspace
// takes the last one back, RETURN or Space ends the entry and ESC ends it
// empty. The initials are matched in ANY order against the 48 spells (the
// table at DS:0x1c30 holds them pre-sorted): "IL" and "LI" are both In Lor,
// and a repeated initial counts once (0x02b2-0x02cb): "IIL" is In Lor too.
//
// The T-Deck chooses the spell from a list instead (a declared device
// adaptation); this is only used where the platform profile asks for it.

constexpr size_t kSpellEntryMaxSyllables = 4;
/** The longest echo: four syllables of at most five letters, each followed by a space. */
constexpr size_t kSpellEntryEchoBytes = kSpellEntryMaxSyllables * 6 + 1;

/** The rune word an initial stands for ("IN" for 'I'), or nullptr (J, O, not a letter). Case-insensitive. */
const char *rune_word_for_initial(char initial);

/** The DS:0x1c30 matcher's result: 0..47 the spell, or one of these. */
constexpr int32_t kSpellEntryNone = -1;    // nothing typed (or ESC): Cast prints "None!"
constexpr int32_t kSpellEntryNoMatch = -2; // typed, no such spell: Cast prints "No effect!"

/**
 * The spell whose syllable initials are exactly `initials[0..count)` in any
 * order (spell names come from spell_definition(), so the table and the
 * names cannot disagree).
 */
int32_t spell_from_initials(const char *initials, size_t count);

/** The echo of `count` initials: "IN LOR " (each word followed by a space). Returns its length. */
size_t spell_entry_echo(const char *initials, size_t count, char *out, size_t capacity);

/** The initials back from an echo such as "IN LOR " (each word's first letter); returns how many. */
size_t spell_entry_initials(const char16_t *echo, size_t length, char *out, size_t capacity);

} // namespace openu5
