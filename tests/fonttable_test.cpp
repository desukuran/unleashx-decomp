// CFontTable: fonts by name in any case, replacing, and loading an unknown
// name as a font file when the file exists (CXBFont is a host stand-in).
#include <cstdio>

#include "../src/render/Fonts.h"

static int failures = 0;

static void expect(bool ok, const char *what)
{
	if (!ok)
	{
		printf("FAIL: %s\n", what);
		failures++;
	}
}

int main(int, char **argv)
{
	CFontTable table;
	CXBFont a, b;
	CXBFont *found = NULL;

	expect(table.Add("MainFont", NULL, FALSE) == E_FAIL, "no font is refused");
	expect(table.Add("MainFont", &a, FALSE) == S_OK, "add");
	expect(table.Find("mainfont", &found) == S_OK && found == &a, "found in any case");
	expect(table.Add("MAINFONT", &b, FALSE) == S_OK, "replace");
	expect(table.Find("MainFont", &found) == S_OK && found == &b, "replaced");
	expect(table.Find("NoSuchFont", &found) == E_FAIL && found == NULL, "unknown name, no such file");
	expect(table.Find("MainFont", NULL) == E_FAIL, "no output pointer");

	// An existing file is loaded as a font and kept (owned by the table).
	const char *file = argv[0];
	expect(table.Find(file, &found) == S_OK && found != NULL, "a file is loaded");
	CXBFont *loaded = found;
	expect(table.Find(file, &found) == S_OK && found == loaded, "loaded once");

	if (failures == 0)
		printf("fonttable_test: ok\n");
	return failures != 0;
}
