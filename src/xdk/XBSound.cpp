// The XDK sample framework's sound (CXBSound, here CSound) and its WAV file
// reader (CWaveFile, CRiffChunk), as UnleashX links them: the whole WAV is
// loaded into one DirectSound buffer. Xbox only. See audio/Sound.h.
#include "../audio/Sound.h"

HRESULT XBUtil_FindMediaFile(char *dst, const char *src);   // XBResource.cpp

static HRESULT LastErrorResult()
{
	DWORD error = GetLastError();
	return (int)error > 0 ? HRESULT_FROM_WIN32(error) : (HRESULT)error;
}

// A RIFF chunk found by its four-character code under a parent. 0x18 bytes.
class CRiffChunk
{
public:
	// 0x000947C0 (this in eax)
	CRiffChunk() : m_fccChunkId(0), m_pParentChunk(NULL), m_hFile(INVALID_HANDLE_VALUE), m_dwDataOffset(0),
	               m_dwDataSize(0), m_dwFlags(0) {}

	// 0x000947AD (this in eax)
	void Initialize(DWORD fccChunkId, const CRiffChunk *parent, HANDLE file)
	{
		m_fccChunkId = fccChunkId;
		m_pParentChunk = parent;
		m_hFile = file;
	}

	// 0x00094715: finds the chunk among its parent's (after the form type
	// of a RIFF parent).
	HRESULT Open()
	{
		LONG offset = 0;
		if (m_pParentChunk != NULL)
		{
			offset = m_pParentChunk->m_dwDataOffset;
			if (m_pParentChunk->m_fccChunkId == 'FFIR')
				offset += 4;
		}
		for (;;)
		{
			DWORD header[2];
			DWORD read;
			if (SetFilePointer(m_hFile, offset, NULL, FILE_BEGIN) == INVALID_SET_FILE_POINTER ||
			    !ReadFile(m_hFile, header, sizeof(header), &read, NULL))
				return LastErrorResult();
			if (read == 0)
				return E_FAIL;
			if (header[0] == m_fccChunkId)
			{
				m_dwFlags |= 1;
				m_dwDataSize = header[1];
				m_dwDataOffset = offset + 8;
				return S_OK;
			}
			offset += 8 + header[1];
		}
	}

	// 0x000946BB
	HRESULT ReadData(LONG offset, void *data, DWORD size)
	{
		DWORD read;
		if (SetFilePointer(m_hFile, m_dwDataOffset + offset, NULL, FILE_BEGIN) != INVALID_SET_FILE_POINTER &&
		    ReadFile(m_hFile, data, size, &read, NULL))
			return S_OK;
		return LastErrorResult();
	}

	DWORD m_fccChunkId;                   // +0x00
	const CRiffChunk *m_pParentChunk;     // +0x04
	HANDLE m_hFile;                       // +0x08
	DWORD m_dwDataOffset;                 // +0x0C
	DWORD m_dwDataSize;                   // +0x10
	DWORD m_dwFlags;                      // +0x14
};

// A WAV file. 0x7C bytes.
class CWaveFile
{
public:
	// 0x00094D71
	CWaveFile() : m_hFile(INVALID_HANDLE_VALUE) {}
	~CWaveFile() { Close(); }

	// 0x000946A5
	void Close()
	{
		if (m_hFile != INVALID_HANDLE_VALUE)
		{
			CloseHandle(m_hFile);
			m_hFile = INVALID_HANDLE_VALUE;
		}
	}

	// 0x00094C64: the RIFF, fmt and data chunks must be there; wsmp and smpl
	// (loop points) are looked up but optional.
	HRESULT Open(const char *path)
	{
		Close();
		m_hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
		if (m_hFile == INVALID_HANDLE_VALUE)
			return LastErrorResult();
		m_RiffChunk.Initialize('FFIR', NULL, m_hFile);
		m_FormatChunk.Initialize(' tmf', &m_RiffChunk, m_hFile);
		m_DataChunk.Initialize('atad', &m_RiffChunk, m_hFile);
		m_WaveSampleChunk.Initialize('pmsw', &m_RiffChunk, m_hFile);
		m_SamplerChunk.Initialize('lpms', &m_RiffChunk, m_hFile);
		HRESULT hr = m_RiffChunk.Open();
		if (FAILED(hr))
			return hr;
		hr = m_FormatChunk.Open();
		if (FAILED(hr))
			return hr;
		hr = m_DataChunk.Open();
		if (FAILED(hr))
			return hr;
		m_WaveSampleChunk.Open();
		m_SamplerChunk.Open();
		DWORD form;
		hr = m_RiffChunk.ReadData(0, &form, sizeof(form));
		if (FAILED(hr))
			return hr;
		return form == 'EVAW' ? S_OK : HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	}

	// 0x00094C18: up to 0x28 bytes of format, the rest zeroed.
	HRESULT GetFormat(WAVEFORMATEX *format, DWORD size)
	{
		DWORD chunkSize = m_FormatChunk.m_dwDataSize;
		if (format == NULL)
			return E_INVALIDARG;
		if (chunkSize > 0x28)
			return E_FAIL;
		HRESULT hr = m_FormatChunk.ReadData(0, format, chunkSize);
		if (FAILED(hr))
			return hr;
		if (chunkSize < 0x28)
			memset((BYTE *)format + chunkSize, 0, 0x28 - chunkSize);
		return S_OK;
	}

	DWORD GetDataSize() const { return m_DataChunk.m_dwDataSize; }

	// 0x000950AD
	HRESULT ReadSample(DWORD position, void *buffer, DWORD size, DWORD *read)
	{
		if (position + size > m_DataChunk.m_dwDataSize)
			size = m_DataChunk.m_dwDataSize - position;
		HRESULT hr = S_OK;
		if (size != 0)
			hr = m_DataChunk.ReadData(position, buffer, size);
		if (read != NULL)
			*read = size;
		return hr;
	}

private:
	HANDLE m_hFile;                       // +0x00
	CRiffChunk m_RiffChunk;               // +0x04
	CRiffChunk m_FormatChunk;             // +0x1C
	CRiffChunk m_DataChunk;               // +0x34
	CRiffChunk m_WaveSampleChunk;         // +0x4C
	CRiffChunk m_SamplerChunk;            // +0x64
};

// 0x00094699 (this in eax)
CSound::CSound()
{
	m_pBuffer = NULL;
	m_dwBufferSize = 0;
	m_pSampleData = NULL;
}

// (0x0002B63D deletes after this)
CSound::~CSound()
{
	Destroy();
}

// 0x00094ACD: the buffer for the format in m_waveFile; with data, it plays
// from there instead of a copy (up to 5.3 MB, whole blocks).
static HRESULT CreateBuffer(CSound *sound, DWORD flags, const void *data, DWORD size)
{
	memset(&sound->m_dsbd, 0, sizeof(sound->m_dsbd));
	sound->m_dsbd.dwFlags = flags;
	sound->m_dsbd.dwSize = sizeof(DSBUFFERDESC);
	sound->m_dsbd.lpwfxFormat = (WAVEFORMATEX *)sound->m_waveFile;
	if (data == NULL)
	{
		if (size > 0x550000)
			size = 0x550000;
		WORD blockAlign = ((WAVEFORMATEX *)sound->m_waveFile)->nBlockAlign;
		sound->m_dsbd.dwBufferBytes = blockAlign != 0 ? size - size % blockAlign : size;
	}
	HRESULT hr = DirectSoundCreateBuffer(&sound->m_dsbd, &sound->m_pBuffer);
	if (FAILED(hr))
		return hr;
	if (data != NULL)
	{
		hr = IDirectSoundBuffer_SetBufferData(sound->m_pBuffer, (void *)data, size);
		if (FAILED(hr))
			return hr;
	}
	return S_OK;
}

// 0x0009511D: the whole file into one buffer.
HRESULT CSound::Create(const char *file, DWORD flags)
{
	char path[MAX_PATH];
	HRESULT hr = XBUtil_FindMediaFile(path, file);
	if (FAILED(hr))
		return hr;
	CWaveFile wave;
	hr = wave.Open(path);
	if (SUCCEEDED(hr))
	{
		hr = wave.GetFormat((WAVEFORMATEX *)m_waveFile, sizeof(m_waveFile));
		if (SUCCEEDED(hr))
		{
			m_dwBufferSize = wave.GetDataSize();
			hr = CreateBuffer(this, flags, NULL, m_dwBufferSize);
			if (SUCCEEDED(hr))
			{
				void *data1 = NULL, *data2 = NULL;
				DWORD size1 = 0, size2 = 0;
				hr = IDirectSoundBuffer_Lock(m_pBuffer, 0, m_dsbd.dwBufferBytes, &data1, &size1, &data2, &size2, 0);
				if (SUCCEEDED(hr))
				{
					hr = wave.ReadSample(0, data1, size1, NULL);
					if (SUCCEEDED(hr))
						hr = IDirectSoundBuffer_Unlock(m_pBuffer, data1, size1, data2, size2);
				}
			}
		}
	}
	wave.Close();
	return hr;
}

// 0x00094AA3 (this in esi)
void CSound::Destroy()
{
	if (m_pBuffer != NULL)
	{
		IDirectSoundBuffer_Release(m_pBuffer);
		m_pBuffer = NULL;
	}
	if (m_pSampleData != NULL)
		VirtualFree(m_pSampleData, 0, MEM_RELEASE);
	m_pSampleData = NULL;
}

// 0x00094A84 (this in eax)
HRESULT CSound::Play(DWORD flags)
{
	if (m_pBuffer == NULL)
		return E_INVALIDARG;
	return IDirectSoundBuffer_Play(m_pBuffer, 0, 0, flags);
}

// 0x00094A70 (this in eax)
HRESULT CSound::Stop()
{
	if (m_pBuffer == NULL)
		return E_INVALIDARG;
	return IDirectSoundBuffer_Stop(m_pBuffer);
}
