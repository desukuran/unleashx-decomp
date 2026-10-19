// XIP archive reading: the header, file entries and names, extraction.
#include <cstdlib>
#include <cstring>

#include "XipArchive.h"

// 0x000BDCBE
HRESULT CXipArchive::Open(const char *path)
{
	// (The original also fails on a NULL this.)
	Close();
	memset(this, 0, sizeof(*this));
	m_pFile = fopen(path, "rb");
	if (m_pFile == NULL)
		return E_INVALIDARG;
	if (fread(this, 1, 0x10, m_pFile) != 0x10 || strncmp(m_magic, "XIP0", 4) != 0)
	{
		fclose(m_pFile);
		return E_FAIL;
	}
	m_pEntries = (SXipFileEntry *)malloc(m_wFileCount * sizeof(SXipFileEntry));
	if (m_pEntries == NULL)
	{
		fclose(m_pFile);
		return E_FAIL;
	}
	if (fread(m_pEntries, 1, m_wFileCount * sizeof(SXipFileEntry), m_pFile) != m_wFileCount * sizeof(SXipFileEntry))
	{
		free(m_pEntries);
		fclose(m_pFile);
		return E_FAIL;
	}
	m_pNameTable = (DWORD *)malloc(m_wFileCount * 4);
	if (m_pNameTable == NULL)
	{
		fclose(m_pFile);
		return E_FAIL;
	}
	if (fread(m_pNameTable, 1, m_wFileCount * 4, m_pFile) == m_wFileCount * 4U)
	{
		// The names run from after the name records up to the data.
		m_dwNamesOffset = 0x10 + (m_wNameCount + m_wFileCount * 4) * 4;
		m_dwNamesSize = m_dwDataOffset - m_dwNamesOffset;
		m_pNames = (char *)malloc(m_dwNamesSize + 1);
		if (m_pNames != NULL)
		{
			if (fread(m_pNames, 1, m_dwNamesSize, m_pFile) == m_dwNamesSize)
			{
				m_pNames[m_dwNamesSize] = '\0';
				return S_OK;
			}
			free(m_pNames);
		}
	}
	free(m_pNameTable);
	free(m_pEntries);
	fclose(m_pFile);
	return E_FAIL;
}

// 0x000BDC0F
void CXipArchive::Close()
{
	if (m_pEntries != NULL)
	{
		free(m_pEntries);
		m_pEntries = NULL;
	}
	if (m_pNameTable != NULL)
	{
		free(m_pNameTable);
		m_pNameTable = NULL;
	}
	if (m_pFile != NULL)
	{
		fclose(m_pFile);
		m_pFile = NULL;
	}
	if (m_pNames != NULL)
	{
		free(m_pNames);
		m_pNames = NULL;
	}
	memset(this, 0, sizeof(*this));
}

// The index-th of the NUL-separated names. Quirk: the names are taken in
// order; the name table isn't used. (names in eax)
static const char *GetXipName(const char *names, int index)   // 0x000BDBDB
{
	for (int i = 0; i < index; i++)
	{
		names += strlen(names) + 1;
	}
	return names;
}

// 0x000BDC67
HRESULT CXipArchive::GetItem(unsigned index, SXipItem *item)
{
	if (item == NULL || index >= m_wFileCount)
		return E_FAIL;
	memset(item, 0, sizeof(*item));
	item->entry = m_pEntries[index];
	strcpy(item->name, GetXipName(m_pNames, index));
	return S_OK;
}

// 0x000BDAC0
HRESULT CXipArchive::Extract(unsigned index, const char *path)
{
	if (index >= m_wFileCount)
		return E_FAIL;
	const SXipFileEntry &entry = m_pEntries[index];
	if (entry.type == XIP_TYPE_DIRECTORY)
		return S_OK;
	FILE *out = fopen(path, "wb");
	if (out == NULL)
		return E_FAIL;
	HRESULT hr = S_OK;
	char buffer[0x1000];
	fseek(m_pFile, m_dwDataOffset + entry.offset, SEEK_SET);
	int blocks = (int)(entry.size >> 12);
	for (int i = 0; i < blocks; i++)
	{
		if (fread(buffer, 1, sizeof(buffer), m_pFile) != sizeof(buffer) ||
		    fwrite(buffer, 1, sizeof(buffer), out) != sizeof(buffer))
		{
			hr = E_FAIL;
			break;
		}
	}
	int rest = (int)(entry.size & 0xFFF);
	if (hr == S_OK && rest > 0)
	{
		if (fread(buffer, 1, rest, m_pFile) != (size_t)rest || fwrite(buffer, 1, rest, out) != (size_t)rest)
			hr = E_FAIL;
	}
	fclose(out);
	return hr;
}
