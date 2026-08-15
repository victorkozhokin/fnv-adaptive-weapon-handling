#include "util/paths.h"
#include "game/types.h"
#include "util/win32.h"

namespace paths {
namespace {

// Kept where the original script-based version of this mod put its settings.
constexpr auto kSubDir  = "Data\\Config\\AWHConfig\\";
constexpr auto kIniName = "AWHConfig.ini";
constexpr auto kLogName = "AWHConfig.log";

char g_dir[512];
char g_ini[512];
char g_log[512];

// Appends as much of `text` as fits, always leaving room for the terminator.
UInt32 Append(char *buffer, UInt32 size, UInt32 offset, const char *text)
{
	while (*text != '\0' && offset + 1 < size)
		buffer[offset++] = *text++;

	buffer[offset] = '\0';
	return offset;
}

void Join(char *buffer, UInt32 size, const char *directory, const char *name)
{
	const auto offset = Append(buffer, size, 0, directory);
	Append(buffer, size, offset, name);
}

// Creates every missing component of g_dir, so the log can be opened even on a
// fresh install where the directory does not exist yet.
void CreateTree()
{
	char partial[sizeof(g_dir)];
	UInt32 length = 0;

	for (const char *p = g_dir; *p != '\0' && length + 1 < sizeof(partial); p++) {
		partial[length++] = *p;

		if (*p != '\\' && *p != '/')
			continue;

		partial[length] = '\0';

		// Skip a bare drive root such as "C:\".
		if (length >= 2 && partial[length - 2] == ':')
			continue;

		CreateDirectoryA(partial, nullptr);
	}
}

} // namespace

void Init(const char *runtimeDirectory)
{
	UInt32 offset = 0;

	if (runtimeDirectory != nullptr && *runtimeDirectory != '\0') {
		offset = Append(g_dir, sizeof(g_dir), 0, runtimeDirectory);

		if (offset > 0 && g_dir[offset - 1] != '\\' && g_dir[offset - 1] != '/')
			offset = Append(g_dir, sizeof(g_dir), offset, "\\");
	} else {
		offset = Append(g_dir, sizeof(g_dir), 0, ".\\");
	}

	Append(g_dir, sizeof(g_dir), offset, kSubDir);

	CreateTree();

	Join(g_ini, sizeof(g_ini), g_dir, kIniName);
	Join(g_log, sizeof(g_log), g_dir, kLogName);
}

const char *Directory() { return g_dir; }
const char *Ini() { return g_ini[0] != '\0' ? g_ini : ".\\Data\\Config\\AWHConfig\\AWHConfig.ini"; }
const char *Log() { return g_log[0] != '\0' ? g_log : ".\\Data\\Config\\AWHConfig\\AWHConfig.log"; }

} // namespace paths
