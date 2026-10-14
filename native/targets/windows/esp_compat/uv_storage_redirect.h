#pragma once
// Ultima V Native for Windows: force-included (MSVC /FI, GCC -include) into
// the shared device sources (native/targets/tdeck/main/*.cpp) so their file
// calls reach the data directory that stands in for the T-Deck's SD card.
// See src/platform/storage.h.
//
// Every standard header those sources use is included FIRST, and only then
// are the path-taking calls renamed, so no library declaration is rewritten.
// The calls that take a FILE* (fread, fwrite, fseek, ftell, fflush, fclose)
// need no mapping and are left alone.
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <sys/stat.h>
#include <sys/types.h>
#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#if !defined(S_ISDIR)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#endif
#if !defined(S_ISREG)
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#endif

std::FILE *uv_fopen(const char *path, const char *mode);
int uv_rename(const char *from, const char *to);
int uv_unlink(const char *path);
int uv_mkdir(const char *path, int mode);
int uv_stat(const char *path, struct stat *out);
int uv_fsync(int fd);
int uv_fileno(std::FILE *file);

namespace std {
using ::uv_fopen;
using ::uv_rename;
} // namespace std

#define fopen uv_fopen
#define rename uv_rename
#define unlink uv_unlink
#define mkdir(p, m) uv_mkdir(p, m)
#define stat(p, b) uv_stat(p, b)
#define fsync uv_fsync
#define fileno uv_fileno
