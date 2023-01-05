// Adapted from the Rainmeter AudioLevel plugin
// https://github.com/rainmeter/rainmeter/blob/master/Plugins/PluginAudioLevel/

/* Copyright (C) 2014 Rainmeter Project Developers
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>. */

#include <algorithm>
#include <array>
#include <cassert>
#include <codecvt>
#include <locale>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

#include <windows.h>

#include <propkey.h>

#include <audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>

#include <extension_api.h>
#include <extension_support.h>

#include "kissfft/kiss_fftr.h"

#include <fmt/core.h>
#include <fmt/xchar.h>

#define WINDOWS_BUG_WORKAROUND 1
#define CLAMP01(x)             max(0.0, min(1.0, (x)))

constexpr auto TWOPI            = (2 * 3.14159265358979323846);
constexpr auto REFTIMES_PER_SEC = 10000000;
constexpr auto EMPTY_TIMEOUT    = 0.500;
constexpr auto DEVICE_TIMEOUT   = 1.500;
constexpr auto QUERY_TIMEOUT    = (1.0 / 60);

#define EXT_FULLNAME "Audio Visualization Data"
#define EXT_NAME     "win_audio_viz"

#define qlog(l, ...)                                                      \
  {                                                                       \
    auto msg = fmt::format("{}: {}", EXT_NAME, fmt::format(__VA_ARGS__)); \
    quasar_log(l, msg.c_str());                                           \
  }

#define debug(...) qlog(QUASAR_LOG_DEBUG, __VA_ARGS__)
#define info(...)  qlog(QUASAR_LOG_INFO, __VA_ARGS__)
#define warn(...)  qlog(QUASAR_LOG_WARNING, __VA_ARGS__)

#define EXIT_ON_ERROR(hres) \
  if (FAILED(hres))         \
  {                         \
    goto Exit;              \
  }
#define SAFE_RELEASE(punk) \
  if ((punk) != nullptr)   \
  {                        \
    (punk)->Release();     \
    (punk) = nullptr;      \
  }

struct Measure
{
    enum Port
    {
        PORT_OUTPUT,
        PORT_INPUT,
    };

    enum Channel
    {
        CHANNEL_FL,
        CHANNEL_FR,
        CHANNEL_C,
        CHANNEL_LFE,
        CHANNEL_BL,
        CHANNEL_BR,
        CHANNEL_SL,
        CHANNEL_SR,
        MAX_CHANNELS,
        CHANNEL_SUM = MAX_CHANNELS
    };

    enum Type
    {
        TYPE_RMS,
        TYPE_PEAK,
        TYPE_FFT,
        TYPE_BAND,
        TYPE_FFTFREQ,
        TYPE_BANDFREQ,
        TYPE_FORMAT,
        TYPE_DEV_STATUS,
        TYPE_DEV_NAME,
        TYPE_DEV_ID,
        TYPE_DEV_LIST,
        // ... //
        NUM_TYPES
    };

    enum Format
    {
        FMT_INVALID,
        FMT_PCM_S16,
        FMT_PCM_F32,
        // ... //
        NUM_FORMATS
    };

    struct BandInfo
    {
        float freq;
        float x;
    };

    Port                 m_port;     // port specifier (not used, only output is supported in win_audio_viz)
    Channel              m_channel;  // channel specifier (not used, all channels are processed in win_audio_viz)

    Format               m_format;       // format specifier (detected in init)
    std::array<int, 2>   m_envRMS;       // RMS attack/decay times in ms (parsed from options)
    std::array<int, 2>   m_envPeak;      // peak attack/decay times in ms (parsed from options)
    std::array<int, 2>   m_envFFT;       // FFT attack/decay times in ms (parsed from options)
    int                  m_fftSize;      // size of FFT (parsed from options)
    int                  m_fftOverlap;   // number of samples between FFT calculations
    int                  m_nBands;       // number of frequency bands (parsed from options)
    double               m_gainRMS;      // RMS gain (parsed from options)
    double               m_gainPeak;     // peak gain (parsed from options)
    double               m_freqMin;      // min freq for band measurement
    double               m_freqMax;      // max freq for band measurement
    double               m_sensitivity;  // dB range for FFT/Band return values (parsed from options)

    IMMDeviceEnumerator* m_enum;       // audio endpoint enumerator
    IMMDevice*           m_dev;        // audio endpoint device
    WAVEFORMATEX*        m_wfx;        // audio format info
    IAudioClient*        m_clAudio;    // audio client instance
    IAudioCaptureClient* m_clCapture;  // capture client instance
#if (WINDOWS_BUG_WORKAROUND)
    IAudioClient*       m_clBugAudio;   // audio client for dummy silent channel
    IAudioRenderClient* m_clBugRender;  // render client for dummy silent channel
#endif
    std::wstring                                 m_reqID;      // requested device ID (parsed from options)
    std::wstring                                 m_devName;    // device friendly name (detected in init)
    std::array<float, 2>                         m_kRMS;       // RMS attack/decay filter constants
    std::array<float, 2>                         m_kPeak;      // peak attack/decay filter constants
    std::array<float, 2>                         m_kFFT;       // FFT attack/decay filter constants
    std::array<double, MAX_CHANNELS>             m_rms;        // current RMS levels
    std::array<double, MAX_CHANNELS>             m_peak;       // current peak levels
    double                                       m_pcMult;     // performance counter inv frequency
    LARGE_INTEGER                                m_pcFill;     // performance counter on last full buffer
    LARGE_INTEGER                                m_pcPoll;     // performance counter on last device poll
    std::array<kiss_fftr_cfg, MAX_CHANNELS>      m_fftCfg;     // FFT states for each channel
    std::array<std::vector<float>, MAX_CHANNELS> m_fftIn;      // buffer for each channel's FFT input
    std::array<std::vector<float>, MAX_CHANNELS> m_fftOut;     // buffer for each channel's FFT output
    std::vector<float>                           m_fftKWdw;    // window function coefficients
    std::vector<float>                           m_fftTmpIn;   // temp FFT processing buffer
    kiss_fft_cpx*                                m_fftTmpOut;  // temp FFT processing buffer
    int                                          m_fftBufW;    // write index for input ring buffers
    int                                          m_fftBufP;    // decremental counter - process FFT at zero
    std::vector<float>                           m_bandFreq;   // buffer of band max frequencies
    std::array<std::vector<float>, MAX_CHANNELS> m_bandOut;    // buffer of band values

    Measure() :
        m_port(PORT_OUTPUT),
        m_channel(CHANNEL_SUM),
        m_format(FMT_INVALID),
        m_fftSize(0),
        m_fftOverlap(0),
        m_nBands(0),
        m_gainRMS(1.0),
        m_gainPeak(1.0),
        m_freqMin(20.0),
        m_freqMax(20000.0),
        m_sensitivity(35.0),
        m_enum(NULL),
        m_dev(NULL),
        m_wfx(NULL),
        m_clAudio(NULL),
        m_clCapture(NULL),
#if (WINDOWS_BUG_WORKAROUND)
        m_clBugAudio(NULL),
        m_clBugRender(NULL),
#endif
        m_fftKWdw{},
        m_fftTmpIn{},
        m_fftTmpOut(NULL),
        m_fftBufW(0),
        m_fftBufP(0),
        m_bandFreq{},
        m_reqID{},
        m_devName{}
    {
        m_envRMS[0]  = 300;
        m_envRMS[1]  = 300;
        m_envPeak[0] = 50;
        m_envPeak[1] = 2500;
        m_envFFT[0]  = 300;
        m_envFFT[1]  = 300;
        m_kRMS[0]    = 0.0f;
        m_kRMS[1]    = 0.0f;
        m_kPeak[0]   = 0.0f;
        m_kPeak[1]   = 0.0f;
        m_kFFT[0]    = 0.0f;
        m_kFFT[1]    = 0.0f;

        for (size_t iChan = 0; iChan < MAX_CHANNELS; ++iChan)
        {
            m_rms[iChan]    = 0.0;
            m_peak[iChan]   = 0.0;
            m_fftCfg[iChan] = NULL;
        }

        LARGE_INTEGER pcFreq;
        QueryPerformanceFrequency(&pcFreq);
        m_pcMult = 1.0 / (double) pcFreq.QuadPart;
    }

    HRESULT DeviceInit();
    void    DeviceRelease();
};

const CLSID          CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID            IID_IMMDeviceEnumerator  = __uuidof(IMMDeviceEnumerator);
const IID            IID_IAudioClient         = __uuidof(IAudioClient);
const IID            IID_IAudioCaptureClient  = __uuidof(IAudioCaptureClient);
const IID            IID_IAudioRenderClient   = __uuidof(IAudioRenderClient);

quasar_data_source_t sources[]                = {
    {       "rms",                    16,    0, 0},
    {      "peak",                    16,    0, 0},
    {       "fft",                    16,    0, 0},
    {      "band",                    16,    0, 0},
    {   "fftfreq", QUASAR_POLLING_CLIENT, 5000, 0},
    {  "bandfreq", QUASAR_POLLING_CLIENT, 5000, 0},
    {    "format", QUASAR_POLLING_CLIENT, 5000, 0},
    {"dev_status", QUASAR_POLLING_CLIENT, 5000, 0},
    {  "dev_name", QUASAR_POLLING_CLIENT, 5000, 0},
    {    "dev_id", QUASAR_POLLING_CLIENT, 5000, 0},
    {  "dev_list", QUASAR_POLLING_CLIENT, 5000, 0}
};

namespace
{
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>    converter;

    Measure*                                                  m                   = nullptr;
    bool                                                      startup_initialized = false;

    std::unordered_map<size_t, Measure::Type>                 m_typemap;
    quasar_ext_handle                                         extHandle = nullptr;
    std::array<std::vector<double>, Measure::Type::NUM_TYPES> output;
}  // namespace

HRESULT Measure::DeviceInit()
{
    HRESULT         hr;
    IPropertyStore* props                = NULL;
    REFERENCE_TIME  hnsRequestedDuration = REFTIMES_PER_SEC;

    // get the device handle
    assert(m_enum && !m_dev);

    // if a specific ID was requested, search for that one, otherwise get the default
    if (!m_reqID.empty() && m_reqID != L"Default")
    {
        hr = m_enum->GetDevice(m_reqID.c_str(), &m_dev);
        if (hr != S_OK)
        {
            warn("Audio {} device '{}' not found (error 0x{}).", m_port == PORT_OUTPUT ? "output" : "input", converter.to_bytes(m_reqID), hr);
        }
    }
    else
    {
        hr = m_enum->GetDefaultAudioEndpoint(m_port == PORT_OUTPUT ? eRender : eCapture, eConsole, &m_dev);
    }

    EXIT_ON_ERROR(hr);

    // store device name

    if (m_dev->OpenPropertyStore(STGM_READ, &props) == S_OK)
    {
        PROPVARIANT varName;
        PropVariantInit(&varName);

        if (props->GetValue(PKEY_Device_FriendlyName, &varName) == S_OK)
        {
            m_devName = std::wstring{varName.pwszVal};
        }

        PropVariantClear(&varName);
    }

    SAFE_RELEASE(props);

#if (WINDOWS_BUG_WORKAROUND)
    // get an extra audio client for the dummy silent channel
    hr = m_dev->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**) &m_clBugAudio);
    if (hr != S_OK)
    {
        warn("Failed to create audio client for Windows bug workaround.");
    }
#endif

    // get the main audio client
    hr = m_dev->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (void**) &m_clAudio);
    if (hr != S_OK)
    {
        warn("Failed to create audio client.");
    }

    EXIT_ON_ERROR(hr);

    // parse audio format - Note: not all formats are supported.
    hr = m_clAudio->GetMixFormat(&m_wfx);
    EXIT_ON_ERROR(hr);

    switch (m_wfx->wFormatTag)
    {
        case WAVE_FORMAT_PCM:
            if (m_wfx->wBitsPerSample == 16)
            {
                m_format = FMT_PCM_S16;
            }
            break;

        case WAVE_FORMAT_IEEE_FLOAT:
            m_format = FMT_PCM_F32;
            break;

        case WAVE_FORMAT_EXTENSIBLE:
            if (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_wfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            {
                m_format = FMT_PCM_F32;
            }
            break;
    }

    if (m_format == FMT_INVALID)
    {
        warn("Invalid sample format.  Only PCM 16b integer or PCM 32b float are supported.");
    }

    // output buffers
    output[Measure::TYPE_RMS].resize(m_wfx->nChannels, 0.0);
    output[Measure::TYPE_PEAK].resize(m_wfx->nChannels, 0.0);

    // setup FFT buffers
    if (m_fftSize)
    {
        // output buffers
        output[Measure::TYPE_FFTFREQ].resize((m_fftSize / 2) + 1, 0.0);
        output[Measure::TYPE_FFT].resize((m_fftSize / 2) + 1, 0.0);

        for (size_t iChan = 0; iChan < m_wfx->nChannels; ++iChan)
        {
            m_fftCfg[iChan] = kiss_fftr_alloc(m_fftSize, 0, NULL, NULL);
            m_fftIn[iChan].resize(m_fftSize, 0.0f);
            m_fftOut[iChan].resize(m_fftSize, 0.0f);
        }

        m_fftKWdw.resize(m_fftSize, 0.0f);
        m_fftTmpIn.resize(m_fftSize, 0.0f);
        m_fftTmpOut = (kiss_fft_cpx*) calloc(m_fftSize * sizeof(kiss_fft_cpx), 1);
        m_fftBufP   = m_fftSize - m_fftOverlap;

        // calculate window function coefficients (http://en.wikipedia.org/wiki/Window_function#Hann_.28Hanning.29_window)
        for (size_t iBin = 0; iBin < m_fftSize; ++iBin)
        {
            m_fftKWdw[iBin] = (float) (0.5 * (1.0 - cos(TWOPI * iBin / (m_fftSize - 1))));
        }
    }

    // calculate band frequencies and allocate band output buffers
    if (m_nBands)
    {
        // output buffers
        output[Measure::TYPE_BANDFREQ].resize(m_nBands, 0.0);
        output[Measure::TYPE_BAND].resize(m_nBands, 0.0);

        m_bandFreq.resize(m_nBands, 0.0f);
        const double step = (log(m_freqMax / m_freqMin) / m_nBands) / log(2.0);
        m_bandFreq[0]     = (float) (m_freqMin * pow(2.0, step / 2.0));

        for (size_t iBand = 1; iBand < m_nBands; ++iBand)
        {
            m_bandFreq[iBand] = (float) (m_bandFreq[iBand - 1] * pow(2.0, step));
        }

        for (size_t iChan = 0; iChan < m_wfx->nChannels; ++iChan)
        {
            m_bandOut[iChan].resize(m_nBands, 0.0f);
        }
    }

#if (WINDOWS_BUG_WORKAROUND)
    // ---------------------------------------------------------------------------------------
    // Windows bug workaround: create a silent render client before initializing loopback mode
    // see:
    // http://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/c7ba0a04-46ce-43ff-ad15-ce8932c00171/loopback-recording-causes-digital-stuttering?forum=windowspro-audiodevelopment
    if (m_port == PORT_OUTPUT)
    {
        hr = m_clBugAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, hnsRequestedDuration, 0, m_wfx, NULL);
        EXIT_ON_ERROR(hr);

        // get the frame count
        UINT32 nFrames;
        hr = m_clBugAudio->GetBufferSize(&nFrames);
        EXIT_ON_ERROR(hr);

        // create a render client
        hr = m_clBugAudio->GetService(IID_IAudioRenderClient, (void**) &m_clBugRender);
        EXIT_ON_ERROR(hr);

        // get the buffer
        BYTE* buffer;
        hr = m_clBugRender->GetBuffer(nFrames, &buffer);
        EXIT_ON_ERROR(hr);

        // release it
        hr = m_clBugRender->ReleaseBuffer(nFrames, AUDCLNT_BUFFERFLAGS_SILENT);
        EXIT_ON_ERROR(hr);

        // start the stream
        hr = m_clBugAudio->Start();
        EXIT_ON_ERROR(hr);
    }
    // ---------------------------------------------------------------------------------------
#endif

    // initialize the audio client
    hr = m_clAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, m_port == PORT_OUTPUT ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0, hnsRequestedDuration, 0, m_wfx, NULL);
    if (hr != S_OK)
    {
        // Compatibility with the Nahimic audio driver
        // https://github.com/rainmeter/rainmeter/commit/0a3dfa35357270512ec4a3c722674b67bff541d6
        // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/bd8cd9f2-974f-4a9f-8e9c-e83001819942/iaudioclient-initialize-failure

        // initialization failed, try to use stereo waveformat
        m_wfx->nChannels       = 2;
        m_wfx->nBlockAlign     = (2 * m_wfx->wBitsPerSample) / 8;
        m_wfx->nAvgBytesPerSec = m_wfx->nSamplesPerSec * m_wfx->nBlockAlign;

        hr = m_clAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, m_port == PORT_OUTPUT ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0, hnsRequestedDuration, 0, m_wfx, NULL);
        if (hr != S_OK)
        {
            // stereo waveformat didnt work either, throw an error
            warn("Failed to initialize audio client.");
        }
    }
    EXIT_ON_ERROR(hr);

    // initialize the audio capture client
    hr = m_clAudio->GetService(IID_IAudioCaptureClient, (void**) &m_clCapture);
    if (hr != S_OK)
    {
        warn("Failed to create audio capture client.");
    }
    EXIT_ON_ERROR(hr);

    // start the stream
    hr = m_clAudio->Start();
    if (hr != S_OK)
    {
        warn("Failed to start the stream.");
    }
    EXIT_ON_ERROR(hr);

    // initialize the watchdog timer
    QueryPerformanceCounter(&m_pcFill);

    return S_OK;

Exit:
    DeviceRelease();
    return hr;
}

void Measure::DeviceRelease()
{
#if (WINDOWS_BUG_WORKAROUND)
    if (m_clBugAudio)
    {
        m_clBugAudio->Stop();
    }
    SAFE_RELEASE(m_clBugRender);
    SAFE_RELEASE(m_clBugAudio);
#endif

    if (m_clAudio)
    {
        m_clAudio->Stop();
    }

    SAFE_RELEASE(m_clCapture);

    if (m_wfx)
    {
        CoTaskMemFree(m_wfx);
        m_wfx = NULL;
    }

    SAFE_RELEASE(m_clAudio);
    SAFE_RELEASE(m_dev);

    for (size_t iChan = 0; iChan < Measure::MAX_CHANNELS; ++iChan)
    {
        if (m_fftCfg[iChan])
            kiss_fftr_free(m_fftCfg[iChan]);
        m_fftCfg[iChan] = NULL;

        m_rms[iChan]    = 0.0;
        m_peak[iChan]   = 0.0;
    }

    if (m_fftTmpOut)
    {
        free(m_fftTmpOut);
        m_fftTmpOut = NULL;
        kiss_fft_cleanup();
    }

    m_format = FMT_INVALID;
}

bool win_audio_viz_init(quasar_ext_handle handle)
{
    assert(m);

    // process types
    m_typemap[sources[0].uid]  = Measure::TYPE_RMS;
    m_typemap[sources[1].uid]  = Measure::TYPE_PEAK;
    m_typemap[sources[2].uid]  = Measure::TYPE_FFT;
    m_typemap[sources[3].uid]  = Measure::TYPE_BAND;
    m_typemap[sources[4].uid]  = Measure::TYPE_FFTFREQ;
    m_typemap[sources[5].uid]  = Measure::TYPE_BANDFREQ;
    m_typemap[sources[6].uid]  = Measure::TYPE_FORMAT;
    m_typemap[sources[7].uid]  = Measure::TYPE_DEV_STATUS;
    m_typemap[sources[8].uid]  = Measure::TYPE_DEV_NAME;
    m_typemap[sources[9].uid]  = Measure::TYPE_DEV_ID;
    m_typemap[sources[10].uid] = Measure::TYPE_DEV_LIST;

    QueryPerformanceCounter(&m->m_pcPoll);

    m->DeviceInit();
    startup_initialized = true;

    return true;
}

bool win_audio_viz_shutdown(quasar_ext_handle handle)
{
    m->DeviceRelease();
    SAFE_RELEASE(m->m_enum);

    delete m;

    return true;
}

bool win_audio_viz_get_data(size_t srcUid, quasar_data_handle hData, char* args)
{
    size_t type = m_typemap[srcUid];

    switch (type)
    {
        case Measure::TYPE_FFTFREQ:
            {
                if (m->m_clCapture && m->m_fftSize)
                {
                    for (size_t i = 0; i <= (m->m_fftSize / 2); i++)
                    {
                        output[type][i] = (double) (i * m->m_wfx->nSamplesPerSec / m->m_fftSize);
                    }

                    quasar_set_data_double_array(hData, output[type].data(), output[type].size());
                    return true;
                }
                break;
            }

        case Measure::TYPE_BANDFREQ:
            {
                if (m->m_clCapture && m->m_nBands)
                {
                    for (size_t i = 0; i < m->m_nBands; i++)
                    {
                        output[type][i] = m->m_bandFreq[i];
                    }

                    quasar_set_data_double_array(hData, output[type].data(), output[type].size());
                    return true;
                }
                break;
            }

        case Measure::TYPE_DEV_STATUS:
            {
                if (m->m_dev)
                {
                    DWORD state;
                    bool  bst = false;
                    if (m->m_dev->GetState(&state) == S_OK && state == DEVICE_STATE_ACTIVE)
                    {
                        bst = true;
                    }

                    quasar_set_data_bool(hData, bst);
                    return true;
                }
                break;
            }

        case Measure::TYPE_FORMAT:
            {
                const char* s_fmtName[Measure::NUM_FORMATS] = {
                    "<invalid>",  // FMT_INVALID
                    "PCM 16b",    // FMT_PCM_S16
                    "PCM 32b",    // FMT_PCM_F32
                };

                if (m->m_wfx)
                {
                    auto ws = fmt::format("{}Hz {} {}ch", m->m_wfx->nSamplesPerSec, s_fmtName[m->m_format], m->m_wfx->nChannels);

                    quasar_set_data_string(hData, ws.c_str());
                    return true;
                }
                break;
            }

        case Measure::TYPE_DEV_NAME:
            {
                quasar_set_data_string(hData, converter.to_bytes(m->m_devName).c_str());
                return true;
            }

        case Measure::TYPE_DEV_ID:
            {
                if (m->m_dev)
                {
                    LPWSTR pwszID = NULL;
                    if (m->m_dev->GetId(&pwszID) == S_OK)
                    {
                        quasar_set_data_string(hData, converter.to_bytes(pwszID).c_str());
                        CoTaskMemFree(pwszID);
                        return true;
                    }
                }
                break;
            }

        case Measure::TYPE_DEV_LIST:
            {
                if (m->m_enum)
                {
                    IMMDeviceCollection* collection = NULL;
                    if (m->m_enum->EnumAudioEndpoints(m->m_port == Measure::PORT_OUTPUT ? eRender : eCapture,
                            DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED,
                            &collection) == S_OK)
                    {
                        char** list = nullptr;
                        UINT   nDevices;

                        collection->GetCount(&nDevices);

                        if (nDevices > 0)
                        {
                            list = new char*[nDevices];
                        }

                        for (ULONG iDevice = 0; iDevice < nDevices; ++iDevice)
                        {
                            IMMDevice*      device = NULL;
                            IPropertyStore* props  = NULL;
                            if (collection->Item(iDevice, &device) == S_OK && device->OpenPropertyStore(STGM_READ, &props) == S_OK)
                            {
                                LPWSTR      id = NULL;
                                PROPVARIANT varName;
                                PropVariantInit(&varName);

                                if (device->GetId(&id) == S_OK && props->GetValue(PKEY_Device_FriendlyName, &varName) == S_OK)
                                {
                                    auto device   = fmt::format(L"{}: {}", id, varName.pwszVal);
                                    list[iDevice] = _strdup(converter.to_bytes(device).c_str());
                                }

                                if (id)
                                    CoTaskMemFree(id);

                                PropVariantClear(&varName);
                            }

                            SAFE_RELEASE(props);
                            SAFE_RELEASE(device);
                        }

                        // set data
                        if (list)
                        {
                            quasar_set_data_string_array(hData, list, nDevices);

                            // cleanup
                            for (size_t i = 0; i < nDevices; i++)
                            {
                                free(list[i]);
                            }

                            delete list;
                        }
                    }

                    SAFE_RELEASE(collection);

                    return true;
                }
                break;
            }

        default:
            break;
    }

    LARGE_INTEGER pcCur;
    QueryPerformanceCounter(&pcCur);

    // query the buffer
    if (m->m_clCapture && (pcCur.QuadPart - m->m_pcPoll.QuadPart) * m->m_pcMult >= QUERY_TIMEOUT)
    {
        BYTE*   buffer;
        UINT32  nFrames;
        DWORD   flags;
        UINT64  pos;
        HRESULT hr;

        while ((hr = m->m_clCapture->GetBuffer(&buffer, &nFrames, &flags, &pos, NULL)) == S_OK)
        {
            // measure RMS and peak levels
            float rms[Measure::MAX_CHANNELS];
            float peak[Measure::MAX_CHANNELS];
            for (int iChan = 0; iChan < Measure::MAX_CHANNELS; ++iChan)
            {
                rms[iChan]  = (float) m->m_rms[iChan];
                peak[iChan] = (float) m->m_peak[iChan];
            }

            // loops unrolled for float, 16b and mono, stereo
            if (m->m_format == Measure::FMT_PCM_F32)
            {
                float* s = (float*) buffer;
                if (m->m_wfx->nChannels == 1)
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        float xL   = (float) *s++;
                        float sqrL = xL * xL;
                        float absL = abs(xL);
                        rms[0]     = sqrL + m->m_kRMS[(sqrL < rms[0])] * (rms[0] - sqrL);
                        peak[0]    = absL + m->m_kPeak[(absL < peak[0])] * (peak[0] - absL);
                        rms[1]     = rms[0];
                        peak[1]    = peak[0];
                    }
                }
                else if (m->m_wfx->nChannels == 2)
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        float xL   = (float) *s++;
                        float xR   = (float) *s++;
                        float sqrL = xL * xL;
                        float sqrR = xR * xR;
                        float absL = abs(xL);
                        float absR = abs(xR);
                        rms[0]     = sqrL + m->m_kRMS[(sqrL < rms[0])] * (rms[0] - sqrL);
                        rms[1]     = sqrR + m->m_kRMS[(sqrR < rms[1])] * (rms[1] - sqrR);
                        peak[0]    = absL + m->m_kPeak[(absL < peak[0])] * (peak[0] - absL);
                        peak[1]    = absR + m->m_kPeak[(absR < peak[1])] * (peak[1] - absR);
                    }
                }
                else
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        for (unsigned int iChan = 0; iChan < m->m_wfx->nChannels; ++iChan)
                        {
                            float x     = (float) *s++;
                            float sqrX  = x * x;
                            float absX  = abs(x);
                            rms[iChan]  = sqrX + m->m_kRMS[(sqrX < rms[iChan])] * (rms[iChan] - sqrX);
                            peak[iChan] = absX + m->m_kPeak[(absX < peak[iChan])] * (peak[iChan] - absX);
                        }
                    }
                }
            }
            else if (m->m_format == Measure::FMT_PCM_S16)
            {
                INT16* s = (INT16*) buffer;
                if (m->m_wfx->nChannels == 1)
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        float xL   = (float) *s++ * 1.0f / 0x7fff;
                        float sqrL = xL * xL;
                        float absL = abs(xL);
                        rms[0]     = sqrL + m->m_kRMS[(sqrL < rms[0])] * (rms[0] - sqrL);
                        peak[0]    = absL + m->m_kPeak[(absL < peak[0])] * (peak[0] - absL);
                        rms[1]     = rms[0];
                        peak[1]    = peak[0];
                    }
                }
                else if (m->m_wfx->nChannels == 2)
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        float xL   = (float) *s++ * 1.0f / 0x7fff;
                        float xR   = (float) *s++ * 1.0f / 0x7fff;
                        float sqrL = xL * xL;
                        float sqrR = xR * xR;
                        float absL = abs(xL);
                        float absR = abs(xR);
                        rms[0]     = sqrL + m->m_kRMS[(sqrL < rms[0])] * (rms[0] - sqrL);
                        rms[1]     = sqrR + m->m_kRMS[(sqrR < rms[1])] * (rms[1] - sqrR);
                        peak[0]    = absL + m->m_kPeak[(absL < peak[0])] * (peak[0] - absL);
                        peak[1]    = absR + m->m_kPeak[(absR < peak[1])] * (peak[1] - absR);
                    }
                }
                else
                {
                    for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                    {
                        for (unsigned int iChan = 0; iChan < m->m_wfx->nChannels; ++iChan)
                        {
                            float x     = (float) *s++ * 1.0f / 0x7fff;
                            float sqrX  = x * x;
                            float absX  = abs(x);
                            rms[iChan]  = sqrX + m->m_kRMS[(sqrX < rms[iChan])] * (rms[iChan] - sqrX);
                            peak[iChan] = absX + m->m_kPeak[(absX < peak[iChan])] * (peak[iChan] - absX);
                        }
                    }
                }
            }

            for (int iChan = 0; iChan < Measure::MAX_CHANNELS; ++iChan)
            {
                m->m_rms[iChan]  = rms[iChan];
                m->m_peak[iChan] = peak[iChan];
            }

            // process FFTs (optional)
            if (m->m_fftSize)
            {
                float*      sF32   = (float*) buffer;
                INT16*      sI16   = (INT16*) buffer;
                const float scalar = (float) (1.0 / sqrt(m->m_fftSize));

                for (unsigned int iFrame = 0; iFrame < nFrames; ++iFrame)
                {
                    // fill ring buffers (demux streams)
                    for (unsigned int iChan = 0; iChan < m->m_wfx->nChannels; ++iChan)
                    {
                        (m->m_fftIn[iChan])[m->m_fftBufW] = m->m_format == Measure::FMT_PCM_F32 ? *sF32++ : (float) *sI16++ * 1.0f / 0x7fff;
                    }

                    m->m_fftBufW = (m->m_fftBufW + 1) % m->m_fftSize;

                    // if overlap limit reached, process FFTs for each channel
                    if (!--m->m_fftBufP)
                    {
                        for (unsigned int iChan = 0; iChan < m->m_wfx->nChannels; ++iChan)
                        {
                            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                            {
                                // copy from the ring buffer to temp space
                                memcpy(&m->m_fftTmpIn[0], &(m->m_fftIn[iChan])[m->m_fftBufW], (m->m_fftSize - m->m_fftBufW) * sizeof(float));
                                if (m->m_fftSize - m->m_fftBufW < m->m_fftTmpIn.size())
                                {
                                    memcpy(&m->m_fftTmpIn[m->m_fftSize - m->m_fftBufW], &m->m_fftIn[iChan][0], m->m_fftBufW * sizeof(float));
                                }

                                // apply the windowing function
                                for (int iBin = 0; iBin < m->m_fftSize; ++iBin)
                                {
                                    m->m_fftTmpIn[iBin] *= m->m_fftKWdw[iBin];
                                }

                                kiss_fftr(m->m_fftCfg[iChan], &m->m_fftTmpIn[0], m->m_fftTmpOut);
                            }
                            else
                            {
                                memset(m->m_fftTmpOut, 0, m->m_fftSize * sizeof(kiss_fft_cpx));
                            }

                            // filter the bin levels as with peak measurements
                            for (int iBin = 0; iBin < m->m_fftSize; ++iBin)
                            {
                                float x0 = (m->m_fftOut[iChan])[iBin];
                                float x1 = (m->m_fftTmpOut[iBin].r * m->m_fftTmpOut[iBin].r + m->m_fftTmpOut[iBin].i * m->m_fftTmpOut[iBin].i) * scalar;
                                x0       = x1 + m->m_kFFT[(x1 < x0)] * (x0 - x1);
                                (m->m_fftOut[iChan])[iBin] = x0;
                            }
                        }

                        m->m_fftBufP = m->m_fftSize - m->m_fftOverlap;
                    }
                }

                // integrate FFT results into log-scale frequency bands
                if (m->m_nBands)
                {
                    const float df     = (float) m->m_wfx->nSamplesPerSec / m->m_fftSize;
                    const float scalar = 2.0f / (float) m->m_wfx->nSamplesPerSec;
                    for (unsigned int iChan = 0; iChan < m->m_wfx->nChannels; ++iChan)
                    {
                        std::fill(m->m_bandOut[iChan].begin(), m->m_bandOut[iChan].end(), 0.0f);
                        int   iBin  = 0;
                        int   iBand = 0;
                        float f0    = 0.0f;

                        while (iBin <= (m->m_fftSize / 2) && iBand < m->m_nBands)
                        {
                            float  fLin1 = ((float) iBin + 0.5f) * df;
                            float  fLog1 = m->m_bandFreq[iBand];
                            float  x     = (m->m_fftOut[iChan])[iBin];
                            float& y     = (m->m_bandOut[iChan])[iBand];

                            if (fLin1 <= fLog1)
                            {
                                y += (fLin1 - f0) * x * scalar;
                                f0 = fLin1;
                                iBin += 1;
                            }
                            else
                            {
                                y += (fLog1 - f0) * x * scalar;
                                f0 = fLog1;
                                iBand += 1;
                            }
                        }
                    }
                }
            }

            // release the buffer
            m->m_clCapture->ReleaseBuffer(nFrames);

            // mark the time of last buffer update
            m->m_pcFill = pcCur;
        }
        // detect device disconnection
        switch (hr)
        {
            case AUDCLNT_S_BUFFER_EMPTY:
                // Windows bug: sometimes when shutting down a playback application, it doesn't zero
                // out the buffer.  Detect this by checking the time since the last successful fill
                // and resetting the volumes if past the threshold.
                if (((pcCur.QuadPart - m->m_pcFill.QuadPart) * m->m_pcMult) >= EMPTY_TIMEOUT)
                {
                    for (int iChan = 0; iChan < Measure::MAX_CHANNELS; ++iChan)
                    {
                        m->m_rms[iChan]  = 0.0;
                        m->m_peak[iChan] = 0.0;
                    }
                }
                break;

            case AUDCLNT_E_BUFFER_ERROR:
            case AUDCLNT_E_DEVICE_INVALIDATED:
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                m->DeviceRelease();
                break;
        }

        m->m_pcPoll = pcCur;
    }
    else if (!m->m_clCapture && (pcCur.QuadPart - m->m_pcPoll.QuadPart) * m->m_pcMult >= DEVICE_TIMEOUT)
    {
        // poll for new devices
        assert(m->m_enum);
        assert(!m->m_dev);
        m->DeviceInit();
        m->m_pcPoll = pcCur;
    }

    switch (type)
    {
        case Measure::TYPE_RMS:
            {
                for (size_t i = 0; i < m->m_wfx->nChannels; i++)
                {
                    output[type][i] = CLAMP01(sqrt(m->m_rms[i]) * m->m_gainRMS);
                }

                quasar_set_data_double_array(hData, output[type].data(), output[type].size());

                return true;
            }

        case Measure::TYPE_PEAK:
            {
                for (size_t i = 0; i < m->m_wfx->nChannels; i++)
                {
                    output[type][i] = CLAMP01(m->m_peak[i] * m->m_gainPeak);
                }

                quasar_set_data_double_array(hData, output[type].data(), output[type].size());

                return true;
            }

        case Measure::TYPE_FFT:
            {
                if (m->m_clCapture && m->m_fftSize)
                {
                    double x;

                    for (size_t i = 0; i <= (m->m_fftSize / 2); i++)
                    {
                        if (m->m_channel == Measure::CHANNEL_SUM)
                        {
                            if (m->m_wfx->nChannels >= 2)
                            {
                                x = (m->m_fftOut[0][i] + m->m_fftOut[1][i]) * 0.5;
                            }
                            else
                            {
                                x = m->m_fftOut[0][i];
                            }
                        }
                        else if (m->m_channel < m->m_wfx->nChannels)
                        {
                            x = m->m_fftOut[m->m_channel][i];
                        }

                        x               = CLAMP01(x);
                        x               = max(0, 10.0 / m->m_sensitivity * log10(x) + 1.0);
                        output[type][i] = x;
                    }

                    quasar_set_data_double_array(hData, output[type].data(), output[type].size());

                    return true;
                }
                break;
            }

        case Measure::TYPE_BAND:
            {
                if (m->m_clCapture && m->m_nBands)
                {
                    double x;

                    for (size_t i = 0; i < m->m_nBands; i++)
                    {
                        if (m->m_channel == Measure::CHANNEL_SUM)
                        {
                            if (m->m_wfx->nChannels >= 2)
                            {
                                x = (m->m_bandOut[0][i] + m->m_bandOut[1][i]) * 0.5;
                            }
                            else
                            {
                                x = m->m_bandOut[0][i];
                            }
                        }
                        else if (m->m_channel < m->m_wfx->nChannels)
                        {
                            x = m->m_bandOut[m->m_channel][i];
                        }

                        x               = CLAMP01(x);
                        x               = max(0, 10.0 / m->m_sensitivity * log10(x) + 1.0);
                        output[type][i] = x;
                    }

                    quasar_set_data_double_array(hData, output[type].data(), output[type].size());

                    return true;
                }
                break;
            }

        default:
            break;
    }

    return false;
}

quasar_settings_t* win_audio_viz_create_settings(quasar_ext_handle handle)
{
    extHandle                               = handle;

    m                                       = new Measure;

    quasar_selection_options_t* devSelect   = nullptr;
    quasar_settings_t*          settings    = nullptr;

    HRESULT                     hr          = S_OK;
    IMMDeviceCollection*        pCollection = NULL;
    IMMDevice*                  pEndpoint   = NULL;
    IPropertyStore*             pProps      = NULL;
    LPWSTR                      pwszID      = NULL;

    // Add default endpoint
    devSelect = quasar_create_selection_setting();
    quasar_add_selection_option(devSelect, "Default", "Default");

    hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (void**) &m->m_enum);
    EXIT_ON_ERROR(hr);

    hr = m->m_enum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &pCollection);
    EXIT_ON_ERROR(hr);

    UINT count;
    hr = pCollection->GetCount(&count);
    EXIT_ON_ERROR(hr);

    if (count == 0)
    {
        warn("No audio endpoints found.");
    }

    // Each loop prints the name of an endpoint device.
    for (ULONG i = 0; i < count; i++)
    {
        // Get pointer to endpoint number i.
        hr = pCollection->Item(i, &pEndpoint);
        EXIT_ON_ERROR(hr);

        // Get the endpoint ID string.
        hr = pEndpoint->GetId(&pwszID);
        EXIT_ON_ERROR(hr);

        hr = pEndpoint->OpenPropertyStore(STGM_READ, &pProps);
        EXIT_ON_ERROR(hr);

        PROPVARIANT varName;
        // Initialize container for property value.
        PropVariantInit(&varName);

        // Get the endpoint's friendly-name property.
        hr = pProps->GetValue(PKEY_Device_FriendlyName, &varName);
        EXIT_ON_ERROR(hr);

        // Print endpoint friendly name and endpoint ID.
        quasar_add_selection_option(devSelect, converter.to_bytes(varName.pwszVal).c_str(), converter.to_bytes(pwszID).c_str());

        CoTaskMemFree(pwszID);
        pwszID = NULL;
        PropVariantClear(&varName);
        SAFE_RELEASE(pProps);
        SAFE_RELEASE(pEndpoint);
    }
    SAFE_RELEASE(pCollection);

    settings = quasar_create_settings(extHandle);

    quasar_add_selection_setting(extHandle, settings, "device", "Audio Device (requires restart)", devSelect);
    devSelect = nullptr;

    // RMS options
    quasar_add_int_setting(extHandle, settings, "RMSAttack", "RMSAttack", 0, 100000, 1, 300);
    quasar_add_int_setting(extHandle, settings, "RMSDecay", "RMSDecay", 0, 100000, 1, 300);
    quasar_add_double_setting(extHandle, settings, "RMSGain", "RMSGain", 0.0, 1000.0, 0.1, 1.0);

    // Peak options
    quasar_add_int_setting(extHandle, settings, "PeakAttack", "PeakAttack", 0, 100000, 1, 50);
    quasar_add_int_setting(extHandle, settings, "PeakDecay", "PeakDecay", 0, 100000, 1, 2500);
    quasar_add_double_setting(extHandle, settings, "PeakGain", "PeakGain", 0.0, 1000.0, 0.1, 1.0);

    // FFT options
    quasar_add_int_setting(extHandle, settings, "FFTSize", "FFTSize", 0, 8192, 2, 256);
    quasar_add_int_setting(extHandle, settings, "FFTOverlap", "FFTOverlap", 0, 4096, 1, 0);
    quasar_add_int_setting(extHandle, settings, "FFTAttack", "FFTAttack", 0, 100000, 1, 300);
    quasar_add_int_setting(extHandle, settings, "FFTDecay", "FFTDecay", 0, 100000, 1, 300);
    quasar_add_int_setting(extHandle, settings, "Bands", "Number of Bands", 0, 1024, 1, 16);
    quasar_add_double_setting(extHandle, settings, "FreqMin", "Band Frequency Min (Hz)", 0.0, 20000.0, 0.1, 20.0);
    quasar_add_double_setting(extHandle, settings, "FreqMax", "Band Frequency Max (Hz)", 0.0, 20000.0, 0.1, 20000.0);
    quasar_add_double_setting(extHandle, settings, "Sensitivity", "Sensitivity", 1.0, 10000.0, 0.1, 35.0);

    return settings;

Exit:
    warn("create_settings() failed on audio enumeration: last error is {}", GetLastError());
    quasar_free(devSelect);
    quasar_free(settings);
    CoTaskMemFree(pwszID);
    SAFE_RELEASE(m->m_enum);
    SAFE_RELEASE(pCollection);
    SAFE_RELEASE(pEndpoint);
    SAFE_RELEASE(pProps);
    delete m;
    return nullptr;
}

void win_audio_viz_update_settings(quasar_settings_t* settings)
{
    bool needs_reinit = false;

    char buf[512];
    quasar_get_selection_setting(extHandle, settings, "device", buf, sizeof(buf));
    m->m_reqID  = converter.from_bytes(buf);

    int fftsize = quasar_get_int_setting(extHandle, settings, "FFTSize");
    if (fftsize < 0 || fftsize & 1)
    {
        warn("Invalid FFTSize {}: must be an even integer >= 0. (powers of 2 work best)", fftsize);
    }
    else if (fftsize != m->m_fftSize)
    {
        m->m_fftSize = fftsize;
        needs_reinit = true;
    }

    if (m->m_fftSize)
    {
        int overlap = quasar_get_int_setting(extHandle, settings, "FFTOverlap");
        if (overlap < 0 || overlap >= m->m_fftSize)
        {
            warn("Invalid FFTOverlap {}: must be an integer between 0 and FFTSize({}).", overlap, m->m_fftSize);
        }
        else
        {
            m->m_fftOverlap = overlap;
        }
    }

    int numbands = quasar_get_uint_setting(extHandle, settings, "Bands");
    if (numbands != m->m_nBands)
    {
        m->m_nBands  = numbands;
        needs_reinit = true;
    }

    m->m_freqMin    = quasar_get_double_setting(extHandle, settings, "FreqMin");
    m->m_freqMax    = quasar_get_double_setting(extHandle, settings, "FreqMax");

    m->m_envRMS[0]  = quasar_get_uint_setting(extHandle, settings, "RMSAttack");
    m->m_envRMS[1]  = quasar_get_uint_setting(extHandle, settings, "RMSDecay");
    m->m_envPeak[0] = quasar_get_uint_setting(extHandle, settings, "PeakAttack");
    m->m_envPeak[1] = quasar_get_uint_setting(extHandle, settings, "PeakDecay");
    m->m_envFFT[0]  = quasar_get_uint_setting(extHandle, settings, "FFTAttack");
    m->m_envFFT[1]  = quasar_get_uint_setting(extHandle, settings, "FFTDecay");

    // (re)parse gain constants
    m->m_gainRMS     = quasar_get_double_setting(extHandle, settings, "RMSGain");
    m->m_gainPeak    = quasar_get_double_setting(extHandle, settings, "PeakGain");
    m->m_sensitivity = quasar_get_double_setting(extHandle, settings, "Sensitivity");

    // regenerate filter constants
    if (m->m_wfx)
    {
        const double freq = m->m_wfx->nSamplesPerSec;
        m->m_kRMS[0]      = (float) exp(log10(0.01) / (freq * (double) m->m_envRMS[0] * 0.001));
        m->m_kRMS[1]      = (float) exp(log10(0.01) / (freq * (double) m->m_envRMS[1] * 0.001));
        m->m_kPeak[0]     = (float) exp(log10(0.01) / (freq * (double) m->m_envPeak[0] * 0.001));
        m->m_kPeak[1]     = (float) exp(log10(0.01) / (freq * (double) m->m_envPeak[1] * 0.001));

        if (m->m_fftSize)
        {
            m->m_kFFT[0] = (float) exp(log10(0.01) / (freq / (m->m_fftSize - m->m_fftOverlap) * (double) m->m_envFFT[0] * 0.001));
            m->m_kFFT[1] = (float) exp(log10(0.01) / (freq / (m->m_fftSize - m->m_fftOverlap) * (double) m->m_envFFT[1] * 0.001));
        }
    }

    if (startup_initialized && needs_reinit)
    {
        m->DeviceRelease();
        m->DeviceInit();
    }
}

quasar_ext_info_fields_t fields = {EXT_NAME,
    EXT_FULLNAME,
    "2.0",
    "r52",
    "Supplies desktop audio frequency data. Adapted from Rainmeter's AudioLevel plugin.",
    "https://github.com/r52/quasar"};

quasar_ext_info_t        info   = {
    QUASAR_API_VERSION,
    &fields,

    std::size(sources),
    sources,

    win_audio_viz_init,             // init
    win_audio_viz_shutdown,         // shutdown
    win_audio_viz_get_data,         // data
    win_audio_viz_create_settings,  // create setting
    win_audio_viz_update_settings   // update setting
};

quasar_ext_info_t* quasar_ext_load(void)
{
    return &info;
}

void quasar_ext_destroy(quasar_ext_info_t* info)
{
    // does nothing; info is on stack
}
