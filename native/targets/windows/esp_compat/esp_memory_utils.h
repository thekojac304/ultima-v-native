#pragma once
// Ultima V Native for Windows: names the heap a pointer lives in, for one
// diagnostic trace. A desktop has a single heap ("internal").
inline bool esp_ptr_external_ram(const void *) { return false; }
inline bool esp_ptr_internal(const void *p) { return p != nullptr; }
