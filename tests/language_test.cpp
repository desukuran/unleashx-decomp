// Host test: built-in strings and a language file.
#include <cstdio>

#include "../src/ui/Language.h"

int main(int argc, char **argv)
{
	printf("built-in: OK=%s CANCEL=%s MSG_COPY_CHECK=%s\n", GetStringA(STR_OK), GetStringA(STR_CANCEL),
	       GetStringA(STR_MSG_COPY_CHECK));
	if (argc > 1)
	{
		LoadLanguageFile(argv[1]);
		printf("%s: OK=%s CANCEL=%s BROWSE_FOLDER=%s SORT=%s\n", argv[1], GetStringA(STR_OK), GetStringA(STR_CANCEL),
		       GetStringA(STR_BROWSE_FOLDER), GetStringA(STR_SORT));
		printf("wide: %ls\n", GetString(STR_CANCEL));
	}
	return 0;
}
