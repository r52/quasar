// Adapted from the Rainmeter AudioLevel plugin
// https://github.com/rainmeter/rainmeter/blob/master/Plugins/PluginAudioLevel/

/* Copyright (C) 2014 Rainmeter Project Developers
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>. */

#ifdef TRACY_ENABLE
#  include <tracy/Tracy.hpp>
#endif

#include <algorithm>
#include <array>
#include <cassert>
#include <complex>
#include <limits>
#include <memory>
#include <numeric>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <extension_api.h>
#include <extension_support.hpp>

#include <fmt/core.h>
#include <fmt/xchar.h>

#include "convert.h"

//- These WinAPI defs must be in a certain order
#include <windows.h>

#include <propkey.h>

#include <audioclient.h>
#include <avrt.h>
#include <Functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
//- WinAPI

#include <kfr/base.hpp>
#include <kfr/dft.hpp>
#include <kfr/dsp.hpp>

#define WINDOWS_BUG_WORKAROUND 1

constexpr std::string_view EXT_FULLNAME = "Audio Visualization Data";
constexpr std::string_view EXT_NAME     = "win_audio_viz";

#define CLAMP01(x) kfr::clamp(x, 0.0, 1.0)

template<typename T>
requires std::numeric_limits<T>::is_integer
constexpr float normalizeAsFloat(T val)
{
    constexpr float imax = 1.0f / std::numeric_limits<T>::max();
    return val * imax;
}

#define qlog(l, ...)                                                          \
    {                                                                         \
        auto msg = fmt::format("{}: {}", EXT_NAME, fmt::format(__VA_ARGS__)); \
        quasar_log(l, msg.c_str());                                           \
    }

#define debug(...) qlog(QUASAR_LOG_DEBUG, __VA_ARGS__)
#define info(...)  qlog(QUASAR_LOG_INFO, __VA_ARGS__)
#define warn(...)  qlog(QUASAR_LOG_WARNING, __VA_ARGS__)

#define EXIT_ON_ERROR(hres) \
    if (FAILED(hres))       \
    {                       \
        goto Exit;          \
    }

template<typename T>
struct IFaceDeleter
{
    constexpr void operator() (T* arg) const requires std::is_base_of_v<IUnknown, T> { arg->Release(); }
};

template<>
struct IFaceDeleter<WCHAR>
{
    constexpr void operator() (WCHAR* arg) const { CoTaskMemFree(arg); }
};

template<>
struct IFaceDeleter<WAVEFORMATEX>
{
    constexpr void operator() (WAVEFORMATEX* arg) const { CoTaskMemFree(arg); }
};

template<typename T, typename D = IFaceDeleter<T>>
struct WInterface
{
    constexpr WInterface(D&& d = D()) : _data(nullptr), _deleter(std::forward<decltype(d)>(d)) {}

    constexpr WInterface(T* ptr, D&& d = D()) : _data(ptr), _deleter(std::forward<decltype(d)>(d)) {}

    constexpr ~WInterface() { release(); }

    constexpr void release()
    {
        if (_data)
        {
            _deleter(_data);
            _data = nullptr;
        }
    }

    constexpr void reset(T* ptr = nullptr)
    {
        release();
        _data = ptr;
    }

    constexpr T** operator& () { return &_data; }

    constexpr T*  get() const { return _data; }

    constexpr T*  operator->() const { return _data; }

    constexpr operator bool () const { return (_data != nullptr); }

private:
    T* _data = nullptr;
    D  _deleter;
};

struct Measure
{
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

    Format                          m_format;       // format specifier (detected in init)
    std::array<size_t, 2>           m_envRMS;       // RMS attack/decay times in ms (parsed from options)
    std::array<size_t, 2>           m_envPeak;      // peak attack/decay times in ms (parsed from options)
    std::array<size_t, 2>           m_envFFT;       // FFT attack/decay times in ms (parsed from options)
    size_t                          m_fftSize;      // size of FFT (parsed from options)
    size_t                          m_fftOverlap;   // number of samples between FFT calculations
    size_t                          m_nBands;       // number of frequency bands (parsed from options)
    double                          m_gainRMS;      // RMS gain (parsed from options)
    double                          m_gainPeak;     // peak gain (parsed from options)
    double                          m_freqMin;      // min freq for band measurement
    double                          m_freqMax;      // max freq for band measurement
    double                          m_sensitivity;  // dB range for FFT/Band return values (parsed from options)

    WInterface<IMMDeviceEnumerator> m_enum;       // audio endpoint enumerator
    WInterface<IMMDevice>           m_dev;        // audio endpoint device
    WInterface<WAVEFORMATEX>        m_wfx;        // audio format info
    WInterface<IAudioClient>        m_clAudio;    // audio client instance
    WInterface<IAudioCaptureClient> m_clCapture;  // capture client instance
#if (WINDOWS_BUG_WORKAROUND)
    WInterface<IAudioClient>       m_clBugAudio;   // audio client for dummy silent channel
    WInterface<IAudioRenderClient> m_clBugRender;  // render client for dummy silent channel
#endif
    std::wstring                                            m_reqID;       // requested device ID (parsed from options)
    std::wstring                                            m_devName;     // device friendly name (detected in init)
    std::array<float, 2>                                    m_kRMS;        // RMS attack/decay filter constants
    std::array<float, 2>                                    m_kPeak;       // peak attack/decay filter constants
    std::array<float, 2>                                    m_kFFT;        // FFT attack/decay filter constants
    std::array<double, MAX_CHANNELS>                        m_rms;         // current RMS levels
    std::array<double, MAX_CHANNELS>                        m_peak;        // current peak levels
    std::array<kfr::dft_plan_real_ptr<float>, MAX_CHANNELS> m_fftPlan;     // FFT plans for each channel
    std::array<std::vector<float>, MAX_CHANNELS>            m_fftIn;       // buffer for each channel's FFT input
    std::array<std::vector<float>, MAX_CHANNELS>            m_fftOut;      // buffer for each channel's FFT output
    kfr::univector<float>                                   m_fftKWdw;     // window function coefficients
    kfr::univector<float>                                   m_fftTmpIn;    // temp FFT processing buffer
    kfr::univector<std::complex<float>>                     m_fftTmpOutP;  // temp FFT processing buffer
    size_t                                                  m_fftBufW;     // write index for input ring buffers
    int                                                     m_fftBufP;     // decremental counter - process FFT at zero
    std::vector<float>                                      m_bandFreq;    // buffer of band max frequencies
    std::array<std::vector<float>, MAX_CHANNELS>            m_bandOut;     // buffer of band values
    std::span<std::byte>                                    m_buffer;      // temp processing buffer

    Measure() :
        m_format(FMT_INVALID),
        m_fftSize(0),
        m_fftOverlap(0),
        m_nBands(0),
        m_gainRMS(1.0),
        m_gainPeak(1.0),
        m_freqMin(20.0),
        m_freqMax(20000.0),
        m_sensitivity(35.0),
        m_enum(),
        m_dev(),
        m_wfx(),
        m_clAudio(),
        m_clCapture(),
#if (WINDOWS_BUG_WORKAROUND)
        m_clBugAudio(),
        m_clBugRender(),
#endif
        m_reqID{},
        m_devName{},
        m_fftKWdw{},
        m_fftTmpIn{},
        m_fftTmpOutP{},
        m_fftBufW(0),
        m_fftBufP(0),
        m_bandFreq{},
        m_buffer{}
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

        for (auto&& iChan : std::views::iota((size_t) 0, (size_t) Measure::MAX_CHANNELS))
        {
            m_rms[iChan]  = 0.0;
            m_peak[iChan] = 0.0;
        }
    }

    ~Measure() { DeviceRelease(); }

    HRESULT DeviceInit();
    void    DeviceRelease();
};

const CLSID          CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID            IID_IMMDeviceEnumerator  = __uuidof(IMMDeviceEnumerator);
const IID            IID_IAudioClient         = __uuidof(IAudioClient);
const IID            IID_IAudioCaptureClient  = __uuidof(IAudioCaptureClient);
const IID            IID_IAudioRenderClient   = __uuidof(IAudioRenderClient);

quasar_data_source_t sources[]                = {
    {       "rms",                 16667,    0, 0},
    {      "peak",                 16667,    0, 0},
    {       "fft",                 16667,    0, 0},
    {      "band",                 16667,    0, 0},
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
    dbj::char_range_to_string                                 string_conv{};
    dbj::wchar_range_to_string                                to_wstring{};

    std::unique_ptr<Measure>                                  m;
    bool                                                      startup_initialized = false;

    std::unordered_map<size_t, Measure::Type>                 m_typemap;
    quasar_ext_handle                                         extHandle = nullptr;
    std::array<std::vector<double>, Measure::Type::NUM_TYPES> output;

    float                                                     fftScalar, bandScalar, df = 0;

    bool                                                      last_data_is_not_zero = true;
    kfr::univector<kfr::u8>                                   temp;
    std::shared_mutex                                         mutex;
}  // namespace

HRESULT Measure::DeviceInit()
{
    std::unique_lock           lk(mutex);
    HRESULT                    hr;
    WInterface<IPropertyStore> props;
    size_t                     bufsize = 0;

    // get the device handle
    assert(m_enum && !m_dev);

    // if a specific ID was requested, search for that one, otherwise get the default
    if (!m_reqID.empty() and m_reqID != L"Default")
    {
        hr = m_enum->GetDevice(m_reqID.c_str(), &m_dev);
        if (hr != S_OK)
        {
            warn("Audio output device '{}' not found (error 0x{}).", string_conv(m_reqID), hr);
        }
    }
    else
    {
        hr = m_enum->GetDefaultAudioEndpoint(eRender, eConsole, &m_dev);
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

    info("Initializing audio device {}", string_conv(m_devName));

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
            if (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(m_wfx.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
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

        fftScalar = (float) (1.0 / kfr::sqrt(m->m_fftSize));

        for (auto&& iChan : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
        {
            m_fftPlan[iChan].reset(new kfr::dft_plan_real<float>(m_fftSize));
            m_fftIn[iChan].resize(m_fftSize, 0.0f);
            m_fftOut[iChan].resize(m_fftSize, 0.0f);
            temp.resize(m_fftPlan[iChan]->temp_size);
        }

        m_fftTmpIn.resize(m_fftSize, 0.0f);
        m_fftTmpOutP.resize(m_fftSize, {0.0f, 0.0f});
        m_fftBufP = m_fftSize - m_fftOverlap;

        m_fftKWdw = kfr::window_hann(m_fftSize);
    }

    // calculate band frequencies and allocate band output buffers
    if (m_nBands)
    {
        // output buffers
        output[Measure::TYPE_BANDFREQ].resize(m_nBands, 0.0);
        output[Measure::TYPE_BAND].resize(m_nBands, 0.0);

        bandScalar = 2.0f / (float) m->m_wfx->nSamplesPerSec;
        df         = (float) m->m_wfx->nSamplesPerSec / m->m_fftSize;

        m_bandFreq.resize(m_nBands, 0.0f);
        const double step = (kfr::log(m_freqMax / m_freqMin) / m_nBands) / kfr::log(2.0);
        m_bandFreq[0]     = (float) (m_freqMin * kfr::pow(2.0, step / 2.0));

        for (auto&& iBand : std::views::iota((size_t) 1, m->m_nBands))
        {
            m_bandFreq[iBand] = (float) (m_bandFreq[iBand - 1] * kfr::pow(2.0, step));
        }

        for (auto&& iChan : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
        {
            m_bandOut[iChan].resize(m_nBands, 0.0f);
        }
    }

#if (WINDOWS_BUG_WORKAROUND)
    // ---------------------------------------------------------------------------------------
    // Windows bug workaround: create a silent render client before initializing loopback mode
    // see:
    // http://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/c7ba0a04-46ce-43ff-ad15-ce8932c00171/loopback-recording-causes-digital-stuttering?forum=windowspro-audiodevelopment
    hr = m_clBugAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 0, 0, m_wfx.get(), NULL);
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
    // ---------------------------------------------------------------------------------------
#endif

    // initialize the audio client
    hr = m_clAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, m_wfx.get(), NULL);
    if (hr != S_OK)
    {
        // Compatibility with the Nahimic audio driver
        // https://github.com/rainmeter/rainmeter/commit/0a3dfa35357270512ec4a3c722674b67bff541d6
        // https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/bd8cd9f2-974f-4a9f-8e9c-e83001819942/iaudioclient-initialize-failure

        // initialization failed, try to use stereo waveformat
        m_wfx->nChannels       = 2;
        m_wfx->nBlockAlign     = (2 * m_wfx->wBitsPerSample) / 8;
        m_wfx->nAvgBytesPerSec = m_wfx->nSamplesPerSec * m_wfx->nBlockAlign;

        hr                     = m_clAudio->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 0, 0, m_wfx.get(), NULL);
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

    UINT32 nMaxFrames;
    hr = m_clAudio->GetBufferSize(&nMaxFrames);
    if (hr != S_OK)
    {
        warn("Failed to determine max buffer size.");
    }
    EXIT_ON_ERROR(hr);

    bufsize  = nMaxFrames * m_wfx->nBlockAlign * sizeof(uint8_t);
    m_buffer = std::span{new std::byte[bufsize](), bufsize};

    return S_OK;

Exit:
    lk.unlock();
    DeviceRelease();
    return hr;
}

void Measure::DeviceRelease()
{
    std::unique_lock lk(mutex);
#if (WINDOWS_BUG_WORKAROUND)
    if (m_clBugAudio)
    {
        m_clBugAudio->Stop();
    }
    m_clBugRender.reset();
    m_clBugAudio.reset();
#endif

    if (m_clAudio)
    {
        m_clAudio->Stop();
    }

    m_clCapture.reset();

    if (m_wfx)
    {
        m_wfx.reset();
    }

    m_clAudio.reset();
    m_dev.reset();

    for (auto&& iChan : std::views::iota((size_t) 0, (size_t) Measure::MAX_CHANNELS))
    {
        if (m_fftPlan[iChan])
            m_fftPlan[iChan].reset();

        m_rms[iChan]  = 0.0;
        m_peak[iChan] = 0.0;
    }

    if (m_buffer.data())
    {
        delete[] m_buffer.data();
        m_buffer = std::span<std::byte>();
    }

    m_format = FMT_INVALID;
}

bool win_audio_viz_init(quasar_ext_handle handle)
{
    assert(m.get());

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

    m->DeviceInit();
    startup_initialized = true;

    return true;
}

bool win_audio_viz_shutdown(quasar_ext_handle handle)
{
    m->DeviceRelease();

    m.reset();

    return true;
}

bool win_audio_viz_get_data(size_t srcUid, quasar_data_handle hData, char* args)
{
    std::unique_lock lk(mutex);

    size_t           type = m_typemap[srcUid];

    switch (type)
    {
        case Measure::TYPE_FFTFREQ:
            {
                if (m->m_clCapture and m->m_fftSize)
                {
                    for (auto&& i : std::views::iota((size_t) 0, (m->m_fftSize / 2) + 1))
                    {
                        output[Measure::TYPE_FFTFREQ][i] = (double) i * m->m_wfx->nSamplesPerSec / m->m_fftSize;
                    }

                    quasar_set_data_double_vector(hData, output[Measure::TYPE_FFTFREQ]);
                    return true;
                }
                break;
            }

        case Measure::TYPE_BANDFREQ:
            {
                if (m->m_clCapture and m->m_nBands)
                {
                    for (auto&& i : std::views::iota((size_t) 0, m->m_nBands))
                    {
                        output[Measure::TYPE_BANDFREQ][i] = m->m_bandFreq[i];
                    }

                    quasar_set_data_double_vector(hData, output[Measure::TYPE_BANDFREQ]);
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
                    if (m->m_dev->GetState(&state) == S_OK and state == DEVICE_STATE_ACTIVE)
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

                    quasar_set_data_string_hpp(hData, ws);
                    return true;
                }
                break;
            }

        case Measure::TYPE_DEV_NAME:
            {
                quasar_set_data_string_hpp(hData, string_conv(m->m_devName));
                return true;
            }

        case Measure::TYPE_DEV_ID:
            {
                if (m->m_dev)
                {
                    WInterface<WCHAR> pwszID;
                    if (m->m_dev->GetId(&pwszID) == S_OK)
                    {
                        quasar_set_data_string_hpp(hData, string_conv(pwszID.get()));
                        return true;
                    }
                }
                break;
            }

        case Measure::TYPE_DEV_LIST:
            {
                if (m->m_enum)
                {
                    WInterface<IMMDeviceCollection> collection;
                    if (m->m_enum->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE | DEVICE_STATE_UNPLUGGED, &collection) == S_OK)
                    {
                        std::vector<std::string> list;
                        UINT                     nDevices;

                        collection->GetCount(&nDevices);

                        for (ULONG iDevice = 0; iDevice < nDevices; ++iDevice)
                        {
                            WInterface<IMMDevice>      device;
                            WInterface<IPropertyStore> props;
                            if (collection->Item(iDevice, &device) == S_OK and device->OpenPropertyStore(STGM_READ, &props) == S_OK)
                            {
                                WInterface<WCHAR> id;
                                PROPVARIANT       varName;
                                PropVariantInit(&varName);

                                if (device->GetId(&id) == S_OK and props->GetValue(PKEY_Device_FriendlyName, &varName) == S_OK)
                                {
                                    auto device = fmt::format(L"{}: {}", id.get(), varName.pwszVal);
                                    list.push_back(string_conv(device).c_str());
                                }

                                PropVariantClear(&varName);
                            }
                        }

                        // set data
                        if (!list.empty())
                        {
                            quasar_set_data_string_vector(hData, list);
                        }
                    }

                    return true;
                }
                break;
            }

        default:
            break;
    }

    // query the buffer
    if (m->m_clCapture)
    {
#ifdef TRACY_ENABLE
        ZoneScopedS(30);
#endif

        BYTE*   buffer;
        UINT32  nFrames;
        DWORD   flags;
        HRESULT hr;

        while ((hr = m->m_clCapture->GetBuffer(&buffer, &nFrames, &flags, NULL, NULL)) == S_OK)
        {
            std::memcpy(m->m_buffer.data(), buffer, nFrames * m->m_wfx->nBlockAlign);

            // release the buffer
            m->m_clCapture->ReleaseBuffer(nFrames);

            if (type == Measure::TYPE_RMS or type == Measure::TYPE_PEAK)
            {
                // measure RMS and peak levels
                float rms[Measure::MAX_CHANNELS];
                float peak[Measure::MAX_CHANNELS];

                for (auto&& iChan : std::views::iota((size_t) 0, (size_t) Measure::MAX_CHANNELS))
                {
                    rms[iChan]  = (float) m->m_rms[iChan];
                    peak[iChan] = (float) m->m_peak[iChan];
                }

                float* sF32 = (float*) m->m_buffer.data();
                INT16* sI16 = (INT16*) m->m_buffer.data();

                for ([[maybe_unused]] auto&& _ : std::views::iota((size_t) 0, (size_t) nFrames))
                {
                    for (auto&& iChan : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                    {
                        float x     = m->m_format == Measure::FMT_PCM_F32 ? *sF32++ : normalizeAsFloat(*sI16++);
                        float sqrX  = kfr::sqr(x);
                        float absX  = kfr::abs(x);
                        rms[iChan]  = sqrX + m->m_kRMS[(sqrX < rms[iChan])] * (rms[iChan] - sqrX);
                        peak[iChan] = absX + m->m_kPeak[(absX < peak[iChan])] * (peak[iChan] - absX);

                        if (m->m_wfx->nChannels == 1)
                        {
                            rms[1]  = rms[0];
                            peak[1] = peak[0];
                        }
                    }
                }

                for (auto&& iChan : std::views::iota((size_t) 0, (size_t) Measure::MAX_CHANNELS))
                {
                    m->m_rms[iChan]  = rms[iChan];
                    m->m_peak[iChan] = peak[iChan];
                }
            }

            // process FFTs (optional)
            if (m->m_fftSize)
            {
                float* sF32 = (float*) m->m_buffer.data();
                INT16* sI16 = (INT16*) m->m_buffer.data();

                for ([[maybe_unused]] auto&& _ : std::views::iota((size_t) 0, (size_t) nFrames))
                {
                    // fill ring buffers (demux streams)
                    for (auto&& iChan : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                    {
                        (m->m_fftIn[iChan])[m->m_fftBufW] = m->m_format == Measure::FMT_PCM_F32 ? *sF32++ : normalizeAsFloat(*sI16++);
                    }

                    m->m_fftBufW = (m->m_fftBufW + 1) % m->m_fftSize;

                    // if overlap limit reached, process FFTs for each channel
                    if (!--m->m_fftBufP)
                    {
                        for (auto&& iChan : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                        {
                            if (!(flags & AUDCLNT_BUFFERFLAGS_SILENT))
                            {
                                // copy from the ring buffer to temp space
                                std::memcpy(m->m_fftTmpIn.data(), m->m_fftIn[iChan].data() + m->m_fftBufW, (m->m_fftSize - m->m_fftBufW) * sizeof(float));
                                if (m->m_fftSize - m->m_fftBufW < m->m_fftTmpIn.size())
                                {
                                    std::memcpy(m->m_fftTmpIn.data() + (m->m_fftSize - m->m_fftBufW), m->m_fftIn[iChan].data(), m->m_fftBufW * sizeof(float));
                                }

                                // apply the windowing function
                                m->m_fftTmpIn = m->m_fftTmpIn * m->m_fftKWdw;

                                m->m_fftPlan[iChan]->execute(m->m_fftTmpOutP, m->m_fftTmpIn, temp);
                            }
                            else
                            {
                                std::fill(m->m_fftTmpOutP.begin(), m->m_fftTmpOutP.end(), std::complex{0.0f, 0.0f});
                            }

                            auto scaled = kfr::cabssqr(m->m_fftTmpOutP) * fftScalar;

                            // filter the bin levels as with peak measurements
                            for (auto&& bin : std::views::iota((size_t) 0, m->m_fftSize))
                            {
                                float x0                  = (m->m_fftOut[iChan])[bin];
                                float x1                  = kfr::get_element(scaled, {bin});
                                x0                        = x1 + m->m_kFFT[(x1 < x0)] * (x0 - x1);
                                (m->m_fftOut[iChan])[bin] = x0;
                            }
                        }

                        m->m_fftBufP = m->m_fftSize - m->m_fftOverlap;
                    }
                }

                // integrate FFT results into log-scale frequency bands
                if (m->m_nBands)
                {
                    for (auto&& channel : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                    {
                        std::fill(m->m_bandOut[channel].begin(), m->m_bandOut[channel].end(), 0.0f);
                        size_t iBin  = 0;
                        size_t iBand = 0;
                        float  f0    = 0.0f;

                        while (iBin <= (m->m_fftSize / 2) and iBand < m->m_nBands)
                        {
                            float  fLin1 = ((float) iBin + 0.5f) * df;
                            float  fLog1 = m->m_bandFreq[iBand];
                            float  x     = (m->m_fftOut[channel])[iBin];
                            float& y     = (m->m_bandOut[channel])[iBand];

                            if (fLin1 <= fLog1)
                            {
                                y += (fLin1 - f0) * x * bandScalar;
                                f0 = fLin1;
                                iBin += 1;
                            }
                            else
                            {
                                y += (fLog1 - f0) * x * bandScalar;
                                f0 = fLog1;
                                iBand += 1;
                            }
                        }
                    }
                }
            }
        }
        // detect device disconnection
        switch (hr)
        {
            case AUDCLNT_E_BUFFER_ERROR:
            case AUDCLNT_E_DEVICE_INVALIDATED:
            case AUDCLNT_E_SERVICE_NOT_RUNNING:
                lk.unlock();
                m->DeviceRelease();
                break;
        }
    }
    // Windows bug: sometimes when shutting down a playback application, it doesn't zero
    // out the buffer.  Detect this by checking the time since the last successful fill
    // and resetting the volumes if past the threshold.
    else
    {
        if (type == Measure::TYPE_RMS or type == Measure::TYPE_PEAK)
        {
            for (auto&& iChan : std::views::iota((size_t) 0, (size_t) Measure::MAX_CHANNELS))
            {
                m->m_rms[iChan]  = 0.0;
                m->m_peak[iChan] = 0.0;
            }
        }

        // poll for new devices
        if (m->m_enum and !m->m_dev)
        {
            lk.unlock();
            m->DeviceInit();
        }

        return true;
    }

    switch (type)
    {
        case Measure::TYPE_RMS:
            {
                for (auto&& i : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                {
                    output[Measure::TYPE_RMS][i] = CLAMP01(kfr::sqrt(m->m_rms[i]) * m->m_gainRMS);
                }

                quasar_set_data_double_vector(hData, output[Measure::TYPE_RMS]);

                return true;
            }

        case Measure::TYPE_PEAK:
            {
                for (auto&& i : std::views::iota((size_t) 0, (size_t) m->m_wfx->nChannels))
                {
                    output[Measure::TYPE_PEAK][i] = CLAMP01(m->m_peak[i] * m->m_gainPeak);
                }

                quasar_set_data_double_vector(hData, output[Measure::TYPE_PEAK]);

                return true;
            }

        case Measure::TYPE_FFT:
            {
                if (m->m_clCapture and m->m_fftSize)
                {
                    double x;

                    for (auto&& i : std::views::iota((size_t) 0, (m->m_fftSize / 2) + 1))
                    {
                        if (m->m_wfx->nChannels >= 2)
                        {
                            x = (m->m_fftOut[0][i] + m->m_fftOut[1][i]) * 0.5;
                        }
                        else
                        {
                            x = m->m_fftOut[0][i];
                        }

                        x                            = CLAMP01(x);
                        x                            = kfr::max(0.0, m->m_sensitivity * kfr::log10(x) + 1.0);
                        output[Measure::TYPE_FFT][i] = x;
                    }

                    double acc = std::reduce(output[Measure::TYPE_FFT].begin(), output[Measure::TYPE_FFT].end(), 0.0);

                    if (acc > 0.0 or (acc == 0.0 and last_data_is_not_zero))
                    {
                        quasar_set_data_double_vector(hData, output[Measure::TYPE_FFT]);
                        last_data_is_not_zero = (acc > 0.0);
                    }
                    else
                    {
                        quasar_set_data_null(hData);
                    }

                    return true;
                }
                break;
            }

        case Measure::TYPE_BAND:
            {
                if (m->m_clCapture and m->m_nBands)
                {
                    double x;

                    for (auto&& i : std::views::iota((size_t) 0, m->m_nBands))
                    {
                        if (m->m_wfx->nChannels >= 2)
                        {
                            x = (m->m_bandOut[0][i] + m->m_bandOut[1][i]) * 0.5;
                        }
                        else
                        {
                            x = m->m_bandOut[0][i];
                        }

                        x                             = CLAMP01(x);
                        x                             = kfr::max(0.0, m->m_sensitivity * kfr::log10(x) + 1.0);
                        output[Measure::TYPE_BAND][i] = x;
                    }

                    double acc = std::reduce(output[Measure::TYPE_BAND].begin(), output[Measure::TYPE_BAND].end(), 0.0);

                    if (acc > 0.0 or (acc == 0.0 and last_data_is_not_zero))
                    {
                        quasar_set_data_double_vector(hData, output[Measure::TYPE_BAND]);
                        last_data_is_not_zero = (acc > 0.0);
                    }
                    else
                    {
                        quasar_set_data_null(hData);
                    }

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
    extHandle = handle;

    m.reset(new Measure());

    quasar_selection_options_t*     devSelect = nullptr;
    quasar_settings_t*              settings  = nullptr;

    HRESULT                         hr        = S_OK;
    WInterface<IMMDeviceCollection> pCollection;

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
        WInterface<IMMDevice>      pEndpoint;
        WInterface<IPropertyStore> pProps;
        WInterface<WCHAR>          pwszID;

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
        quasar_add_selection_option(devSelect, string_conv(varName.pwszVal).c_str(), string_conv(pwszID.get()).c_str());

        PropVariantClear(&varName);
    }

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
    quasar_add_int_setting(extHandle, settings, "FFTSize", "FFTSize (requires power of 2)", 0, 8192, 2, 256);
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
    quasar_free_selection_setting(devSelect);
    m.reset();
    return nullptr;
}

void win_audio_viz_update_settings(quasar_settings_t* settings)
{
    bool needs_reinit = false;

    auto dev          = quasar_get_selection_setting_hpp(extHandle, settings, "device");
    m->m_reqID        = to_wstring(dev.data());

    size_t fftsize    = quasar_get_uint_setting(extHandle, settings, "FFTSize");
    if (fftsize < 0 or fftsize & 1)
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
        size_t overlap = quasar_get_uint_setting(extHandle, settings, "FFTOverlap");
        if (overlap < 0 or overlap >= m->m_fftSize)
        {
            warn("Invalid FFTOverlap {}: must be an integer between 0 and FFTSize({}).", overlap, m->m_fftSize);
        }
        else
        {
            m->m_fftOverlap = overlap;
        }
    }

    size_t numbands = quasar_get_uint_setting(extHandle, settings, "Bands");
    if (numbands != m->m_nBands)
    {
        m->m_nBands  = numbands;
        needs_reinit = true;
    }

    double freqMin = quasar_get_double_setting(extHandle, settings, "FreqMin");
    double freqMax = quasar_get_double_setting(extHandle, settings, "FreqMax");

    if (freqMin != m->m_freqMin or freqMax != m->m_freqMax)
    {
        m->m_freqMin = freqMin;
        m->m_freqMax = freqMax;
        needs_reinit = true;
    }

    m->m_envRMS[0]  = quasar_get_uint_setting(extHandle, settings, "RMSAttack");
    m->m_envRMS[1]  = quasar_get_uint_setting(extHandle, settings, "RMSDecay");
    m->m_envPeak[0] = quasar_get_uint_setting(extHandle, settings, "PeakAttack");
    m->m_envPeak[1] = quasar_get_uint_setting(extHandle, settings, "PeakDecay");
    m->m_envFFT[0]  = quasar_get_uint_setting(extHandle, settings, "FFTAttack");
    m->m_envFFT[1]  = quasar_get_uint_setting(extHandle, settings, "FFTDecay");

    // (re)parse gain constants
    m->m_gainRMS     = quasar_get_double_setting(extHandle, settings, "RMSGain");
    m->m_gainPeak    = quasar_get_double_setting(extHandle, settings, "PeakGain");
    m->m_sensitivity = 10.0 / std::max(1.0, quasar_get_double_setting(extHandle, settings, "Sensitivity"));

    // regenerate filter constants
    if (m->m_wfx)
    {
        const double freq = m->m_wfx->nSamplesPerSec;
        m->m_kRMS[0]      = (float) kfr::exp(kfr::log10(0.01) / (freq * (double) m->m_envRMS[0] * 0.001));
        m->m_kRMS[1]      = (float) kfr::exp(kfr::log10(0.01) / (freq * (double) m->m_envRMS[1] * 0.001));
        m->m_kPeak[0]     = (float) kfr::exp(kfr::log10(0.01) / (freq * (double) m->m_envPeak[0] * 0.001));
        m->m_kPeak[1]     = (float) kfr::exp(kfr::log10(0.01) / (freq * (double) m->m_envPeak[1] * 0.001));

        if (m->m_fftSize)
        {
            m->m_kFFT[0] = (float) kfr::exp(kfr::log10(0.01) / (freq / (m->m_fftSize - m->m_fftOverlap) * (double) m->m_envFFT[0] * 0.001));
            m->m_kFFT[1] = (float) kfr::exp(kfr::log10(0.01) / (freq / (m->m_fftSize - m->m_fftOverlap) * (double) m->m_envFFT[1] * 0.001));
        }
    }

    if (startup_initialized and needs_reinit)
    {
        m->DeviceRelease();
        m->DeviceInit();
    }
}

quasar_ext_info_fields_t fields = {.version = "3.0",
    .author                                 = "r52",
    .description                            = "Supplies desktop audio frequency data. Adapted from Rainmeter's AudioLevel plugin.",
    .url                                    = "https://github.com/r52/quasar"};

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
    quasar_strcpy(fields.name, sizeof(fields.name), EXT_NAME.data(), EXT_NAME.size());
    quasar_strcpy(fields.fullname, sizeof(fields.fullname), EXT_FULLNAME.data(), EXT_FULLNAME.size());
    return &info;
}

void quasar_ext_destroy(quasar_ext_info_t* info)
{
    // does nothing; info is on stack
}
