#pragma once
// Ultima V Native for Windows (MSVC only): the shared device sources include
// <unistd.h> for unlink/fsync/fileno. MSVC has no such header; the calls are
// provided by uv_storage_redirect.h (force-included), so this only pulls in
// the CRT headers that declare the POSIX-named functions.
#include <direct.h>
#include <io.h>
#include <process.h>
