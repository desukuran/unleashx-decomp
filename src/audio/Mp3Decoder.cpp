// CAudioDecoder and CDirectSound: MP3 decoding with libmad into a
// streaming DirectSound buffer. See Mp3Decoder.h.
#include "Mp3Decoder.h"

#include <cstring>

#include "../common/XboxKernel.h"
#include "../system/FileIO.h"

#define READ_BUFFER_SIZE 0x20000
#define FIRST_FRAME_BUFFER 0x2000
#define XING_MAGIC 0x58696E67   // "Xing"

// ---- CDirectSound ----

// 0x0006F63C
CDirectSound::CDirectSound(DWORD sampleRate, WORD channels, DWORD bufferSize, WORD bitsPerSample)
{
	m_bPlaying = FALSE;
	m_bValid = FALSE;
	m_dwWritePos = 0;
	m_dwBufferSize = 0;
	m_lVolume = 0;
	memset(&m_format, 0, sizeof(m_format));
	m_format.wFormatTag = WAVE_FORMAT_PCM;
	m_format.nChannels = channels;
	m_format.nSamplesPerSec = sampleRate;
	m_format.wBitsPerSample = bitsPerSample;
	m_format.nBlockAlign = (WORD)(bitsPerSample * channels / 8);
	m_format.nAvgBytesPerSec = m_format.nBlockAlign * sampleRate;
	m_format.cbSize = 0;
	// At least two seconds, and 80 KB.
	if (bufferSize > m_format.nAvgBytesPerSec)
		m_dwBufferSize = bufferSize;
	else if (m_format.nAvgBytesPerSec * 2 >= 0x14000)
		m_dwBufferSize = m_format.nAvgBytesPerSec * 2;
	else
		m_dwBufferSize = 0x14000;
	m_dwBufferSize -= m_dwBufferSize % m_format.nBlockAlign;
	m_pBuffer = NULL;

	DSBUFFERDESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.dwSize = sizeof(desc);
	desc.dwFlags = DSBCAPS_CTRLVOLUME;
	desc.dwBufferBytes = m_dwBufferSize;
	desc.lpwfxFormat = &m_format;
	if (FAILED(DirectSoundCreateBuffer(&desc, &m_pBuffer)))
	{
		m_pBuffer = NULL;
		return;
	}
	Clear();
	m_dwWriteCursor = (DWORD)-1;
	IDirectSoundBuffer_SetCurrentPosition(m_pBuffer, 0);
	IDirectSoundBuffer_SetHeadroom(m_pBuffer, 1200);
	Stop();
	// The first data goes a tenth of the buffer ahead.
	m_dwWritePos = m_dwBufferSize / 10;
	SetVolume(m_lVolume);
	m_bValid = TRUE;
	m_dwTotalWritten = 0;
}

// 0x0006F61B
CDirectSound::~CDirectSound()
{
	Stop();
	if (m_pBuffer != NULL)
	{
		IDirectSoundBuffer_Release(m_pBuffer);
		m_pBuffer = NULL;
	}
}

// 0x0006F1CE
void CDirectSound::Stop()
{
	if (!m_bValid)
		return;
	IDirectSoundBuffer_StopEx(m_pBuffer, 0, DSBSTOPEX_IMMEDIATE);
	DWORD status;
	do
		IDirectSoundBuffer_GetStatus(m_pBuffer, &status);
	while (status & DSBSTATUS_PLAYING);
	m_dwWritePos = 0;
	m_dwTotalWritten = 0;
	m_bPlaying = FALSE;
}

// 0x0006F203
void CDirectSound::SetVolume(LONG volume)
{
	if (!m_bValid)
		return;
	if (volume != DSBVOLUME_MIN)
		m_lVolume = volume;
	if (volume > 0)
		volume = 0;
	IDirectSoundBuffer_SetVolume(m_pBuffer, volume);
}

// 0x0006F47F
void CDirectSound::Mute(BOOL mute)
{
	SetVolume(mute ? DSBVOLUME_MIN : m_lVolume);
}

// 0x0006F224
// (As CAudioBuf::SetMixBins.)
HRESULT CDirectSound::SetMixBins(const DSMIXBINS *mixBins)
{
	if (m_pBuffer == NULL || m_format.nChannels > 2)
		return E_FAIL;
	if (mixBins != NULL)
	{
		DSVOICEPROPS props;
		IDirectSoundBuffer_GetVoiceProperties(m_pBuffer, &props);
		if (props.dwMixBinCount == mixBins->dwMixBinCount)
			return S_OK;
		if (mixBins->dwMixBinCount > 2)
		{
			WAVEFORMATEXTENSIBLE format;
			memset(&format, 0, sizeof(format));
			format.Format = m_format;
			format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
			format.Format.cbSize = 0x16;
			format.dwChannelMask = 0x3F;
			format.SubFormat = KSDATAFORMAT_SUBTYPE_PCM;
			IDirectSoundBuffer_SetFormat(m_pBuffer, &format.Format);
		}
	}
	return IDirectSoundBuffer_SetMixBins(m_pBuffer, mixBins);
}

// 0x0006F368
BOOL CDirectSound::IsPlaying()
{
	DWORD status;
	return m_bValid && IDirectSoundBuffer_GetStatus(m_pBuffer, &status) == S_OK && (status & DSBSTATUS_PLAYING);
}

// 0x0006F38E
DWORD CDirectSound::GetQueued()
{
	if (!m_bValid)
		return 0;
	DWORD status;
	if (IDirectSoundBuffer_GetStatus(m_pBuffer, &status) == S_OK && !(status & DSBSTATUS_PLAYING))
		return m_dwWritePos;
	DWORD play;
	DWORD write;
	if (IDirectSoundBuffer_GetCurrentPosition(m_pBuffer, &play, &write) != S_OK)
		return 0;
	if (play < m_dwWritePos)
		return m_dwWritePos - play;
	return (m_dwBufferSize - play) + m_dwWritePos;
}

// 0x0006F3EE
void CDirectSound::Play()
{
	if (!m_bValid)
		return;
	DWORD status;
	if (IDirectSoundBuffer_GetStatus(m_pBuffer, &status) == S_OK)
	{
		BOOL ok = TRUE;
		if (status & DSBSTATUS_PAUSED)
		{
			IDirectSoundBuffer_Pause(m_pBuffer, DSBPAUSE_RESUME);
			Sleep(5);
			ok = IDirectSoundBuffer_GetStatus(m_pBuffer, &status) == S_OK;
		}
		if (ok && !(status & DSBSTATUS_PLAYING))
			IDirectSoundBuffer_Play(m_pBuffer, 0, 0, DSBPLAY_LOOPING | DSBPLAY_FROMSTART);
	}
	SetVolume(m_lVolume);
	m_bPlaying = TRUE;
}

// 0x0006F454
void CDirectSound::Pause()
{
	if (m_pBuffer == NULL)
		return;
	DWORD status;
	if (IDirectSoundBuffer_GetStatus(m_pBuffer, &status) == S_OK && (status & DSBSTATUS_PLAYING))
		IDirectSoundBuffer_Pause(m_pBuffer, DSBPAUSE_PAUSE);
}

// 0x0006F496
void CDirectSound::Clear()
{
	if (!IsValid())
		return;
	Mute(TRUE);
	void *data = NULL;
	DWORD size;
	IDirectSoundBuffer_Lock(m_pBuffer, 0, m_dwBufferSize, &data, &size, NULL, NULL, DSBLOCK_ENTIREBUFFER);
	if (data != NULL)
		memset(data, 0, size);
	IDirectSoundBuffer_Unlock(m_pBuffer, data, size, NULL, 0);
	m_dwWritePos = 0;
	Mute(FALSE);
}

// 0x0006F507
// (The lock is retried until it works. Quirk: when the write wraps but
// there's no second part, nothing is unlocked.)
void CDirectSound::Write(const void *data, DWORD size)
{
	if (!m_bValid || size == 0)
		return;
	m_dwTotalWritten += size;
	void *part1 = NULL;
	DWORD size1 = 0;
	void *part2 = NULL;
	DWORD size2 = 0;
	while (IDirectSoundBuffer_Lock(m_pBuffer, m_dwWritePos, size, &part1, &size1, &part2, &size2, 0) != S_OK)
	{
	}
	if (size1 < size)
	{
		// Around the end of the buffer.
		memcpy(part1, data, size1);
		if (size2 != 0 && part2 != NULL)
		{
			memcpy(part2, (const BYTE *)data + size1, size2);
			m_dwWritePos = size2;
			IDirectSoundBuffer_Unlock(m_pBuffer, part1, size1, part2, size2);
			return;
		}
	}
	else
	{
		m_dwWritePos += size;
		memcpy(part1, data, size);
		IDirectSoundBuffer_Unlock(m_pBuffer, part1, size, part2, 0);
		if (m_dwWritePos < m_dwBufferSize)
			return;
	}
	m_dwWritePos = 0;
}

// 0x0006F78A
void CDirectSound::GetPlayPosition(DWORD *play, DWORD *queued)
{
	if (!m_bValid)
		return;
	if (m_dwWriteCursor == (DWORD)-1)
	{
		m_dwWriteCursor = 0;
		if (queued != NULL)
			*queued = m_dwBufferSize;
		IDirectSoundBuffer_SetCurrentPosition(m_pBuffer, 0);
	}
	else if (play != NULL)
	{
		HRESULT hr = IDirectSoundBuffer_GetCurrentPosition(m_pBuffer, play, &m_dwWriteCursor);
		if (queued != NULL && SUCCEEDED(hr))
			*queued = GetQueued();
	}
}

// ---- madplay's dither ----

// The error feedback and random state of one channel.
struct SDither
{
	mad_fixed_t error[3];
	mad_fixed_t random;
};
// Shared by all decoders. (0x00279534, 0x00279524)
static SDither s_ditherLeft;
static SDither s_ditherRight;

static unsigned long DitherRandom(unsigned long state)
{
	return state * 0x0019660DL + 0x3C6EF35FL;
}

// madplay's audio_linear_dither for 16 bits.
static int DitherSample(mad_fixed_t sample, SDither *dither)
{
	const int scaleBits = MAD_F_FRACBITS + 1 - 16;
	const mad_fixed_t mask = (1L << scaleBits) - 1;
	sample += dither->error[0] - dither->error[1] + dither->error[2];
	dither->error[2] = dither->error[1];
	dither->error[1] = dither->error[0] / 2;
	mad_fixed_t output = sample + (1L << (scaleBits - 1));
	unsigned long random = DitherRandom((unsigned long)dither->random);
	output += (mad_fixed_t)(random & mask) - (dither->random & mask);
	dither->random = (mad_fixed_t)random;
	const mad_fixed_t max = 0x0FFFFFFF;
	const mad_fixed_t min = -0x10000000;
	if (output >= max)
	{
		output = max;
		if (sample > max)
			sample = max;
	}
	else if (output < min)
	{
		output = min;
		if (sample < min)
			sample = min;
	}
	output &= ~mask;
	dither->error[0] = sample - output;
	return output >> scaleBits;
}

static void PutSample(BYTE *&out, int sample)
{
	*out++ = (BYTE)sample;
	*out++ = (BYTE)(sample >> 8);
}

// ---- CAudioDecoder ----

// 0x0006E184
CAudioDecoder::CAudioDecoder()
{
	m_pReadBuffer = NULL;
	m_pOutBuffer = NULL;
	m_pFile = NULL;
	m_pSound = NULL;
	m_bDecoding = false;
	m_bFirst = true;
	m_bDraining = FALSE;
	m_dwEndPosition = 0;
}

// 0x0006ED7C
CAudioDecoder::~CAudioDecoder()
{
	Close();
	if (m_pSound != NULL)
	{
		m_pSound->Stop();
		delete m_pSound;
		m_pSound = NULL;
	}
	if (m_pReadBuffer != NULL)
	{
		XPhysicalFree(m_pReadBuffer);   // (MmFreeContiguousMemory)
		m_pReadBuffer = NULL;
	}
	if (m_pOutBuffer != NULL)
	{
		delete[] m_pOutBuffer;
		m_pOutBuffer = NULL;
	}
}

// 0x0006E56E
void CAudioDecoder::FreeDecoder()
{
	if (m_pFile != NULL)
	{
		mad_frame_finish(&m_frame);
		mad_stream_finish(&m_stream);
	}
	if (m_pSound != NULL)
	{
		m_pSound->Mute(TRUE);
		m_pSound->Stop();
		delete m_pSound;
		m_pSound = NULL;
	}
}

// 0x0006EC95
void CAudioDecoder::Close()
{
	FreeDecoder();
	if (m_pFile != NULL)
	{
		m_pFile->Close();
		delete m_pFile;
		m_pFile = NULL;
	}
	if (m_pOutBuffer != NULL)
	{
		delete[] m_pOutBuffer;
		m_pOutBuffer = NULL;
	}
}

// 0x0006ECDC
void CAudioDecoder::Reset()
{
	Close();
	m_bPaused = false;
	m_bStartedPlaying = false;
	m_bFirst = true;
	if (m_pReadBuffer == NULL)
		m_pReadBuffer = (BYTE *)XPhysicalAlloc(READ_BUFFER_SIZE + MAD_BUFFER_GUARD, (uintptr_t)-1, 0,
		                                       4 /* PAGE_READWRITE */);
	if (m_pReadBuffer != NULL)
		memset(m_pReadBuffer, 0, READ_BUFFER_SIZE + MAD_BUFFER_GUARD);
	if (m_pOutBuffer != NULL)
		delete[] m_pOutBuffer;   // (already freed by Close)
	m_pGuard = NULL;
	m_dwDuration = 0;
	m_dwBitRate = 0;
	m_dwSampleRate = 0;
	m_bStereo = FALSE;
	m_unknown14 = 0;
	m_nEof = 0;
	m_nFrames = 0;
	m_dwBytesRead = 0;
	m_bDraining = FALSE;
	m_dwEndPosition = 0;
	m_unknown5948 = 0;
	m_dwLastPlayCursor = 0;
	m_unknown593C = 0xA8B9FF7E;
}

// 0x0006E1BE
// madplay's parse_xing. (xing in esi, bits in eax)
static int ParseXing(SXingHeader *xing, mad_bitptr ptr, unsigned int bits)
{
	if (bits >= 64 && mad_bit_read(&ptr, 32) == XING_MAGIC)
	{
		xing->flags = mad_bit_read(&ptr, 32);
		bits -= 64;
		if (xing->flags & XING_FRAMES)
		{
			if (bits < 32)
				goto fail;
			xing->frames = mad_bit_read(&ptr, 32);
			bits -= 32;
		}
		if (xing->flags & XING_BYTES)
		{
			if (bits < 32)
				goto fail;
			xing->bytes = mad_bit_read(&ptr, 32);
			bits -= 32;
		}
		if (xing->flags & XING_TOC)
		{
			if (bits < 800)
				goto fail;
			for (int i = 0; i < 100; i++)
				xing->toc[i] = (BYTE)mad_bit_read(&ptr, 8);
			bits -= 800;
		}
		if (xing->flags & XING_SCALE)
		{
			if (bits < 32)
				goto fail;
			xing->scale = mad_bit_read(&ptr, 32);
		}
		return 0;
	}
fail:
	xing->flags = 0;
	return -1;
}

// 0x0006E281
// Decodes from the start: the first frame if it carries a Xing header,
// else the second.
int FindFirstFrame(SXingHeader *xing, IFile *file, mad_header *header)
{
	BYTE *buffer = new BYTE[FIRST_FRAME_BUFFER];
	if (buffer == NULL)
		return -1;
	mad_stream stream;
	mad_frame frame;
	mad_stream_init(&stream);
	mad_frame_init(&frame);
	if (xing != NULL)
		xing->flags = 0;
	DWORD filled = 0;
	int frames = 0;
	int result = 0;
	for (;;)
	{
		if (filled < FIRST_FRAME_BUFFER)
		{
			DWORD read = file->Read(buffer + filled, FIRST_FRAME_BUFFER - filled);
			if (read == 0)
			{
				result = -1;
				break;
			}
			filled += read;
		}
		mad_stream_buffer(&stream, buffer, filled);
		BOOL found = FALSE;
		for (;;)
		{
			if (mad_frame_decode(&frame, &stream) == -1)
			{
				if (MAD_RECOVERABLE(stream.error))
					continue;
				break;
			}
			if (frames++ != 0 || (xing != NULL && ParseXing(xing, stream.anc_ptr, stream.anc_bitlen) == 0))
			{
				found = TRUE;
				break;
			}
		}
		if (found || frames != 0)
		{
			if (header != NULL)
				*header = frame.header;
			break;
		}
		if (stream.error != MAD_ERROR_BUFLEN)
		{
			result = -1;
			break;
		}
		// Keep the partial frame for the next read.
		filled -= (DWORD)(stream.next_frame - buffer);
		memmove(buffer, stream.next_frame, filled);
	}
	mad_frame_finish(&frame);
	mad_stream_finish(&stream);
	delete[] buffer;
	return result;
}

// 0x0006EDE1
BOOL CAudioDecoder::Open(const char *path, BOOL skipId3)
{
	Reset();
	if (m_pReadBuffer == NULL)
		return FALSE;
	if (m_pFile != NULL)
		delete m_pFile;
	m_pFile = CreateFileObject(path);
	if (m_pFile == NULL || !m_pFile->OpenRead(path, 0x1000, FALSE) || m_pFile->IsEOF())
		return FALSE;
	// An ID3v2 tag: its size is four 7-bit bytes.
	DWORD start = 0;
	if ((BYTE)skipId3)
	{
		BYTE tag[10];
		memset(tag, 0, sizeof(tag));
		m_pFile->Read(tag, 10);
		if (memcmp(tag, "ID3", 3) == 0)
			start = (((tag[6] << 7 | tag[7]) << 7 | tag[8]) << 7 | tag[9]) + 10;
		m_pFile->Seek(start, NULL, FILE_BEGIN);
	}
	mad_stream_init(&m_stream);
	mad_frame_init(&m_frame);
	mad_synth_init(&m_synth);
	m_timer = mad_timer_zero;
	Sleep(100);
	m_xing.flags = 0;
	if (FindFirstFrame(&m_xing, m_pFile, &m_frame.header) == -1)
	{
		Close();
		return FALSE;
	}
	DWORD fileSize = (BYTE)skipId3 ? m_pFile->GetSize() : 0;
	m_pFile->Seek(start, NULL, FILE_BEGIN);
	// The length: the Xing frame count, else the size at this bit rate.
	mad_timer_t length = mad_timer_zero;
	if (!(m_xing.flags & XING_FRAMES))
	{
		mad_timer_set(&length, 0, 1, m_frame.header.bitrate >> 3);
		mad_timer_multiply(&length, fileSize);
	}
	else
	{
		length = m_frame.header.duration;
		mad_timer_multiply(&length, m_xing.frames);
	}
	m_dwDuration = mad_timer_count(length, MAD_UNITS_SECONDS);
	m_dwSampleRate = m_frame.header.samplerate;
	m_unknown14 = 1;
	m_dwBitRate = m_frame.header.bitrate / 1000;
	m_bStereo = m_frame.header.mode != MAD_MODE_SINGLE_CHANNEL;
	if (m_dwDuration == 0 || m_frame.header.bitrate / 1000 == 0 || m_frame.header.samplerate == 0)
		return FALSE;
	if (m_pFile->IsEOF())
	{
		Close();
		return FALSE;
	}
	// A second of output per block (8 KB for files shorter than that).
	DWORD outSize = m_dwSampleRate * 4;
	if (fileSize < outSize)
		outSize = 0x2000;
	if (outSize != m_dwOutSize && m_pOutBuffer != NULL)
	{
		delete[] m_pOutBuffer;
		m_pOutBuffer = NULL;
	}
	m_dwOutSize = outSize;
	if (m_pOutBuffer == NULL)
	{
		m_pOutBuffer = new BYTE[m_dwOutSize];
		Sleep(5);
		if (m_pOutBuffer == NULL)
		{
			m_dwOutSize >>= 1;
			m_pOutBuffer = new BYTE[m_dwOutSize];
			Sleep(5);
			if (m_pOutBuffer == NULL)
			{
				Close();
				return FALSE;
			}
		}
	}
	memset(m_pOutBuffer, 0, m_dwOutSize);
	m_unknown5948 = 0;
	m_pOut = m_pOutBuffer;
	m_pOutEnd = m_pOutBuffer + m_dwOutSize;
	if (m_pSound != NULL)
	{
		m_pSound->Clear();
		delete m_pSound;
		m_pSound = NULL;
	}
	m_pSound = new CDirectSound(m_dwSampleRate, 2, m_dwOutSize * 4, 16);
	if (m_pSound->IsValid())
	{
		m_pSound->m_dwWriteCursor = (DWORD)-1;
		return TRUE;
	}
	delete m_pSound;
	m_pSound = NULL;
	m_bActive = false;
	Close();
	return FALSE;
}

// 0x0006F16C
void CAudioDecoder::Stop()
{
	m_bActive = false;
	while (m_bDecoding)
		Sleep(5);
	m_bDraining = FALSE;
	m_bFirst = m_bActive;
	Sleep(50);
	if (m_pSound != NULL)
	{
		m_pSound->Clear();
		m_pSound->Stop();
	}
	Close();
}

// 0x0006E4FD
BOOL CAudioDecoder::IsPlaying()
{
	if (m_bPaused)
		return TRUE;
	if (m_pSound != NULL && m_bActive)
		return m_pSound->IsPlaying();
	return FALSE;
}

// 0x0006E3A8
BOOL CAudioDecoder::Prefill()
{
	if (!m_bPaused)
	{
		while (Decode())
		{
			if (m_pSound->GetQueued() >= m_dwOutSize)
				break;
		}
		Sleep(5);
	}
	m_pSound->m_dwWriteCursor = (DWORD)-1;
	return TRUE;
}

// 0x0006E44D
void CAudioDecoder::Start()
{
	if (m_bFirst)
	{
		m_bActive = true;
		m_bFirst = false;
	}
	if (m_pSound != NULL && m_bActive)
	{
		while (m_bDecoding)
			Sleep(5);
		m_pSound->Clear();
		m_pSound->Mute(TRUE);
		Prefill();
		m_pSound->Play();
		m_pSound->SetVolume(m_lVolume);
	}
	m_bStartedPlaying = true;
}

// 0x0006E524
void CAudioDecoder::Pause()
{
	m_bPaused = true;
	if (m_pSound != NULL)
		m_pSound->Pause();
}

// 0x0006E4CE
void CAudioDecoder::Resume()
{
	m_bPaused = false;
	m_unknown5968 = false;
	if (m_pSound != NULL)
	{
		m_pSound->SetVolume(m_lVolume);
		m_pSound->Play();
	}
}

// 0x0006E53F
void CAudioDecoder::SetVolume(LONG volume)
{
	if (m_pSound != NULL)
		m_pSound->SetVolume(volume);
	m_lVolume = volume;
}

// 0x0006E557
HRESULT CAudioDecoder::SetMixBins(const DSMIXBINS *mixBins)
{
	if (m_pSound == NULL)
		return E_FAIL;
	return m_pSound->SetMixBins(mixBins);
}

// 0x0006E3F1
ULONGLONG CAudioDecoder::GetQueuedTime()
{
	if (m_bPaused || m_pSound == NULL)
		return 0;
	ULONGLONG samples = m_pSound->GetQueued() / ((m_bStereo & 0x7FFFFFFF) != 0 ? 2 : 1);
	return samples * 1000 / m_dwSampleRate;
}

// (Inlined three times in Decode.)
void CAudioDecoder::FlushOutput(DWORD size)
{
	m_pSound->Write(m_pOutBuffer, size);
	memset(m_pOutBuffer, 0, m_dwOutSize);
	m_pOut = m_pOutBuffer;
}

// 0x0006E603
BOOL CAudioDecoder::Decode()
{
	if (m_bPaused || m_bFirst)
		return TRUE;
	if (m_pSound == NULL || m_pOutBuffer == NULL || m_pFile == NULL)
		return FALSE;

	if (m_bDraining)
	{
		// The file is done: wait for the play cursor to pass the end.
		DWORD play;
		m_pSound->GetPlayPosition(&play, NULL);
		Sleep(5);
		m_bDecoding = false;
		if (play < m_dwEndPosition && m_dwEndPosition != 0 && play != m_dwLastPlayCursor)
		{
			m_dwLastPlayCursor = play;
			return TRUE;
		}
		m_pSound->Clear();
		m_pSound->Stop();
		m_dwEndPosition = 0;
		m_bDraining = FALSE;
		return FALSE;
	}

	// Enough queued already?
	if (m_bActive && m_pSound->GetQueued() >= m_dwOutSize * 3)
		return TRUE;
	m_bDecoding = true;
	BOOL more = TRUE;
	int frames = 0;
	if (m_nEof == 0)
	{
		BOOL enough = FALSE;
		for (;;)
		{
			if (m_stream.buffer == NULL || m_stream.error == MAD_ERROR_BUFLEN)
			{
				// More of the file, after what's left of the last read.
				BYTE *to;
				DWORD keep;
				DWORD space;
				if (m_stream.next_frame == NULL)
				{
					to = m_pReadBuffer;
					keep = 0;
					space = READ_BUFFER_SIZE;
				}
				else
				{
					keep = (DWORD)(m_stream.bufend - m_stream.next_frame);
					memmove(m_pReadBuffer, m_stream.next_frame, keep);
					to = m_pReadBuffer + keep;
					space = READ_BUFFER_SIZE - keep;
				}
				DWORD read = m_pFile->Read(to, space);
				m_dwBytesRead += read;
				BOOL eof = m_pFile->IsEOF();
				if (read == 0)
				{
					// (Quirk: only a read failure, not the end of the
					// file, marks it ended.)
					if (!eof)
						m_nEof = 1;
					more = FALSE;
					break;
				}
				if (eof)
				{
					// libmad needs zeros after the last frame.
					m_pGuard = to + read;
					memset(m_pGuard, 0, MAD_BUFFER_GUARD);
					read += MAD_BUFFER_GUARD;
				}
				mad_stream_buffer(&m_stream, m_pReadBuffer, read + keep);
				m_stream.error = 0;
			}
			if (mad_frame_decode(&m_frame, &m_stream) == 0)
			{
				m_nFrames++;
				mad_timer_add(&m_timer, m_frame.header.duration);
				mad_synth_frame(&m_synth, &m_frame);
				const mad_pcm &pcm = m_synth.pcm;
				for (int i = 0; i < pcm.length; i++)
				{
					int left = DitherSample(pcm.samples[0][i], &s_ditherLeft);
					PutSample(m_pOut, left);
					if (!m_bStereo)
						PutSample(m_pOut, left);
					else
						PutSample(m_pOut, DitherSample(pcm.samples[1][i], &s_ditherRight));
					if (m_pOut == m_pOutEnd)
					{
						FlushOutput(m_dwOutSize);
						m_unknown5948 = 0;
						m_dwEndPosition = m_dwOutSize;
						if (frames != 0 || m_pSound->GetQueued() > m_dwOutSize * 2)
							enough = TRUE;
					}
				}
				frames++;
			}
			else if (!MAD_RECOVERABLE(m_stream.error) && m_stream.error != MAD_ERROR_BUFLEN)
			{
				m_nEof = 1;
				more = FALSE;
				break;
			}
			if (!m_bActive || enough)
				break;
		}
	}
	// What's left of the block.
	if (m_pOut != m_pOutBuffer && m_nEof != 2)
	{
		DWORD size = (DWORD)(m_pOut - m_pOutBuffer);
		m_pSound->Write(m_pOutBuffer, size);
		m_dwEndPosition = size;
		memset(m_pOutBuffer, 0, m_dwOutSize);
		m_pOut = m_pOutBuffer;
	}
	m_bDecoding = false;
	if (!more)
	{
		if (!m_bActive)
			return FALSE;
		// Play out what's queued.
		m_bDraining = TRUE;
		m_pSound->GetPlayPosition(&m_dwLastPlayCursor, NULL);
		m_dwEndPosition += m_dwLastPlayCursor;
	}
	return TRUE;
}
