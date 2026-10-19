/* Data types applied to the Ghidra program by tools/ghidra_scripts/ApplyTypes.java.
 * Plain C only (Ghidra's C parser). Layouts are MSVC 7.1 / XDK 5933, 32-bit. */

typedef unsigned char BYTE;
typedef unsigned short WORD;
typedef unsigned long DWORD;
typedef int BOOL;

/* std::basic_string<char> (Dinkumware, VC7.1): small-string buffer of 16. 0x1c bytes */
typedef struct std_string {
	BYTE _Alval[4];
	union {
		char _Buf[16];
		char *_Ptr;
	} _Bx;
	DWORD _Mysize;
	DWORD _Myres;   /* < 16 => data is in _Buf */
} std_string;

/* std::basic_string<wchar_t>: small-string buffer of 8 wchar_t. 0x1c bytes */
typedef struct std_wstring {
	BYTE _Alval[4];
	union {
		unsigned short _Buf[8];
		unsigned short *_Ptr;
	} _Bx;
	DWORD _Mysize;
	DWORD _Myres;   /* < 8 => data is in _Buf */
} std_wstring;

/* std::vector<T> (VC7.1). 0x10 bytes */
typedef struct std_vector {
	BYTE _Alval[4];
	void *_Myfirst;
	void *_Mylast;
	void *_Myend;
} std_vector;
