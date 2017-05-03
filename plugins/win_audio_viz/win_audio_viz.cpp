// Adapted from the Rainmeter AudioLevel plugin

#include <array>
#include <limits>
#include <mutex>
#include <shared_mutex>

#define NOMINMAX
#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <windows.h>

#include <plugin_api.h>
#include <plugin_support.h>

#include <kfr/dft.hpp>
#include <kfr/dsp.hpp>
#include <kfr/math.hpp>

#define CLAMP01(x) kfr::max(0.0, kfr::min(1.0, (x)))

#define PLUGIN_NAME "Audio Visualization Data"
#define PLUGIN_CODE "win_audio_viz"

// Process once every 10 buffers (~100ms)
#define VIZ_BUFFER_LIMIT 10

#define VIZ_SENSITIVITY 50.0
#define VIZ_FFT_SIZE 256
#define VIZ_BANDS 32
#define VIZ_FREQ_MIN 20.0
#define VIZ_FREQ_MAX 20000.0
#define VIZ_MAX_CHANNELS 8

#define qlog(l, f, ...)                                                \
    {                                                                  \
        char msg[256];                                                 \
        snprintf(msg, sizeof(msg), PLUGIN_CODE ": " f, ##__VA_ARGS__); \
        quasar_log(l, msg);                                            \
    }

#define debug(f, ...) qlog(QUASAR_LOG_DEBUG, f, ##__VA_ARGS__)
#define info(f, ...) qlog(QUASAR_LOG_INFO, f, ##__VA_ARGS__)
#define warn(f, ...) qlog(QUASAR_LOG_WARNING, f, ##__VA_ARGS__)

#include "cleanup.h"

quasar_data_source_t sources[1] =
    {
      { "viz", -1, 0 }
    };

using namespace kfr;

namespace
{
    enum WaveFormat
    {
        FORMAT_INV,
        FORMAT_SI16,
        FORMAT_FL32
    };

    quasar_plugin_handle plugHandle = nullptr;

    IMMDevice* pMMDevice     = nullptr;
    HANDLE     hThread       = nullptr;
    HANDLE     hStartedEvent = nullptr;
    HANDLE     hStopEvent    = nullptr;
    HRESULT    threadResult  = S_OK;

    std::array<double, VIZ_BANDS>                               spectrumFreqs;
    std::array<std::array<double, VIZ_BANDS>, VIZ_MAX_CHANNELS> spectrum;
    std::shared_mutex                                           spectrumMutex;

    WaveFormat s_format = FORMAT_INV;

    univector<complex<double>>      fftIn[VIZ_MAX_CHANNELS];
    univector<complex<double>>      fftOut[VIZ_MAX_CHANNELS];
    univector<double, VIZ_FFT_SIZE> fftMag[VIZ_MAX_CHANNELS];
}

HRESULT LoopbackCapture(
    IMMDevice* pMMDevice,
    HANDLE     startEvent,
    HANDLE     stopEvent)
{
    HRESULT hr;

    // activate an IAudioClient
    IAudioClient* pAudioClient;
    hr = pMMDevice->Activate(
        __uuidof(IAudioClient),
        CLSCTX_ALL,
        NULL,
        (void**) &pAudioClient);
    if (FAILED(hr))
    {
        warn("IMMDevice::Activate(IAudioClient) failed: hr = 0x%08lx", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioClient(pAudioClient);

    // get the default device periodicity
    REFERENCE_TIME hnsDefaultDevicePeriod;
    hr = pAudioClient->GetDevicePeriod(&hnsDefaultDevicePeriod, NULL);
    if (FAILED(hr))
    {
        warn("IAudioClient::GetDevicePeriod failed: hr = 0x%08lx", hr);
        return hr;
    }

    // get the default device format
    WAVEFORMATEX* pwfx;
    hr = pAudioClient->GetMixFormat(&pwfx);
    if (FAILED(hr))
    {
        warn("IAudioClient::GetMixFormat failed: hr = 0x%08lx", hr);
        return hr;
    }
    CoTaskMemFreeOnExit freeMixFormat(pwfx);

    switch (pwfx->wFormatTag)
    {
        case WAVE_FORMAT_PCM:
            if (pwfx->wBitsPerSample == 16)
            {
                s_format = FORMAT_SI16;
            }
            break;

        case WAVE_FORMAT_IEEE_FLOAT:
            s_format = FORMAT_FL32;
            break;

        case WAVE_FORMAT_EXTENSIBLE:
            if (reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT)
            {
                s_format = FORMAT_FL32;
            }
            break;
    }

    if (s_format == FORMAT_INV)
    {
        warn("Unsupported audio format");
        return S_FALSE;
    }

    debug("Bits: %d, Channels: %d, Sample Rate: %lu", pwfx->wBitsPerSample, pwfx->nChannels, pwfx->nSamplesPerSec);

    // Process once every 10 buffers
    size_t samplePass = 0;

    // create a periodic waitable timer
    HANDLE hWakeUp = CreateWaitableTimer(NULL, FALSE, NULL);
    if (NULL == hWakeUp)
    {
        DWORD dwErr = GetLastError();
        warn("CreateWaitableTimer failed: last error = %lu", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CloseHandleOnExit closeWakeUp(hWakeUp);

    // call IAudioClient::Initialize
    // note that AUDCLNT_STREAMFLAGS_LOOPBACK and AUDCLNT_STREAMFLAGS_EVENTCALLBACK
    // do not work together...
    // the "data ready" event never gets set
    // so we're going to do a timer-driven loop
    hr = pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        0,
        0,
        pwfx,
        0);
    if (FAILED(hr))
    {
        warn("IAudioClient::Initialize failed: hr = 0x%08lx", hr);
        return hr;
    }

    // activate an IAudioCaptureClient
    IAudioCaptureClient* pAudioCaptureClient;
    hr = pAudioClient->GetService(
        __uuidof(IAudioCaptureClient),
        (void**) &pAudioCaptureClient);
    if (FAILED(hr))
    {
        warn("IAudioClient::GetService(IAudioCaptureClient) failed: hr = 0x%08lx", hr);
        return hr;
    }
    ReleaseOnExit releaseAudioCaptureClient(pAudioCaptureClient);

    // register with MMCSS
    DWORD  nTaskIndex = 0;
    HANDLE hTask      = AvSetMmThreadCharacteristics(L"Audio", &nTaskIndex);
    if (NULL == hTask)
    {
        DWORD dwErr = GetLastError();
        warn("AvSetMmThreadCharacteristics failed: last error = %lu", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    AvRevertMmThreadCharacteristicsOnExit unregisterMmcss(hTask);

    // set the waitable timer
    LARGE_INTEGER liFirstFire;
    liFirstFire.QuadPart   = -hnsDefaultDevicePeriod / 2;                     // negative means relative time
    LONG lTimeBetweenFires = (LONG) hnsDefaultDevicePeriod / 2 / (10 * 1000); // convert to milliseconds
    BOOL bOK               = SetWaitableTimer(
        hWakeUp,
        &liFirstFire,
        lTimeBetweenFires,
        NULL,
        NULL,
        FALSE);

    if (!bOK)
    {
        DWORD dwErr = GetLastError();
        warn("SetWaitableTimer failed: last error = %lu", dwErr);
        return HRESULT_FROM_WIN32(dwErr);
    }
    CancelWaitableTimerOnExit cancelWakeUp(hWakeUp);

    // call IAudioClient::Start
    hr = pAudioClient->Start();
    if (FAILED(hr))
    {
        warn("IAudioClient::Start failed: hr = 0x%08lx", hr);
        return hr;
    }
    AudioClientStopOnExit stopAudioClient(pAudioClient);

    SetEvent(startEvent);

    debug("Audio capture thread running");

    // loopback capture loop
    HANDLE waitArray[2] = { stopEvent, hWakeUp };
    DWORD  dwWaitResult;

    bool bDone = false;
    while (!bDone)
    {
        // get the captured data
        BYTE*  pData;
        UINT32 nNumFramesToRead;
        DWORD  dwFlags;
        bool   silentPacket = false;

        hr = pAudioCaptureClient->GetBuffer(
            &pData,
            &nNumFramesToRead,
            &dwFlags,
            NULL,
            NULL);
        if (FAILED(hr))
        {
            warn("IAudioCaptureClient::GetBuffer failed: hr = 0x%08lx", hr);
            return hr;
        }

        {
            float*       sF32   = (float*) pData;
            INT16*       sI16   = (INT16*) pData;
            const double scalar = (double) (1.0 / kfr::sqrt(VIZ_FFT_SIZE));

            for (size_t frame = 0; frame < nNumFramesToRead; frame++)
            {
                for (size_t chan = 0; chan < pwfx->nChannels; chan++)
                {
                    fftIn[chan].push_back(make_complex(s_format == FORMAT_FL32 ? (double) *sF32++ : (double) *sI16++ * 1.0 / 0x7fff, 0.0));
                }

                if (fftIn[0].size() == VIZ_FFT_SIZE)
                {
                    auto window = to_pointer(window_hann<double>(VIZ_FFT_SIZE));

                    for (size_t chan = 0; chan < pwfx->nChannels; chan++)
                    {
                        if (!(dwFlags & AUDCLNT_BUFFERFLAGS_SILENT))
                        {
                            fftIn[chan] = fftIn[chan] * window;

                            fftOut[chan] = dft(fftIn[chan]);
                        }
                        else
                        {
                            std::fill_n(fftOut[chan].begin(), VIZ_FFT_SIZE, 0.0);
                        }

                        for (size_t b = 0; b < VIZ_FFT_SIZE; b++)
                        {
                            fftMag[chan][b] = (kfr::sqr(fftOut[chan][b].real()) + kfr::sqr(fftOut[chan][b].imag())) * scalar;
                        }

                        fftIn[chan].clear();
                    }
                }
            }
        }

        {
            const double df     = (double) pwfx->nSamplesPerSec / VIZ_FFT_SIZE;
            const double scalar = 2.0f / (double) pwfx->nSamplesPerSec;

            std::unique_lock<std::shared_mutex> lock(spectrumMutex);

            for (size_t chan = 0; chan < pwfx->nChannels; chan++)
            {
                std::fill(spectrum[chan].begin(), spectrum[chan].end(), 0.0);

                size_t bin  = 0;
                size_t band = 0;
                double f0   = 0.0;

                while (bin <= (VIZ_FFT_SIZE / 2) && band < VIZ_BANDS)
                {
                    double  fLin1 = ((double) bin + 0.5) * df;
                    double  fLog1 = spectrumFreqs[band];
                    double  x     = fftMag[chan][bin];
                    double& y     = (spectrum[chan])[band];

                    if (fLin1 <= fLog1)
                    {
                        y += (fLin1 - f0) * x * scalar;
                        f0 = fLin1;
                        bin += 1;
                    }
                    else
                    {
                        y += (fLog1 - f0) * x * scalar;
                        f0 = fLog1;
                        band += 1;
                    }
                }
            }

            // scale
            double maxMag = 0.0;
            for (size_t b = 0; b < VIZ_BANDS; b++)
            {
                double x = spectrum[0][b];

                if (pwfx->nChannels >= 2)
                {
                    // combine channels
                    x = (spectrum[0][b] + spectrum[1][b]) * 0.5;
                }

                x              = CLAMP01(x);
                spectrum[0][b] = kfr::max(0.0, 10.0 / VIZ_SENSITIVITY * kfr::log10(x) + 1.0);

                if (spectrum[0][b] > maxMag)
                {
                    maxMag = spectrum[0][b];
                }
            }

            silentPacket = (maxMag == 0.0);
        }

        samplePass++;

        if (samplePass >= VIZ_BUFFER_LIMIT)
        {
            static bool sentOneZero = false;

            if (!silentPacket || !sentOneZero)
            {
                sentOneZero = silentPacket;

                // only send if not silent
                quasar_signal_data_ready(plugHandle, "viz");
            }

            samplePass = 0;
        }

        hr = pAudioCaptureClient->ReleaseBuffer(nNumFramesToRead);
        if (FAILED(hr))
        {
            warn("IAudioCaptureClient::ReleaseBuffer failed: hr = 0x%08lx", hr);
            return hr;
        }

        if (FAILED(hr))
        {
            warn("IAudioCaptureClient::GetNextPacketSize failed: hr = 0x%08lx", hr);
            return hr;
        }

        dwWaitResult = WaitForMultipleObjects(
            ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE);

        if (WAIT_OBJECT_0 == dwWaitResult)
        {
            info("Received stop event");
            bDone = true;
            continue; // exits loop
        }

        if (WAIT_OBJECT_0 + 1 != dwWaitResult)
        {
            warn("Unexpected WaitForMultipleObjects return value %lu", dwWaitResult);
            return E_UNEXPECTED;
        }
    } // capture loop

    return hr;
}

DWORD WINAPI LoopbackCaptureThreadFunction(LPVOID pContext)
{
    threadResult = CoInitialize(NULL);
    if (FAILED(threadResult))
    {
        warn("CoInitialize failed: hr = 0x%08lx", threadResult);
        return 0;
    }
    CoUninitializeOnExit cuoe;

    threadResult = LoopbackCapture(pMMDevice, hStartedEvent, hStopEvent);

    info("Audio capture thread stopped");

    return 0;
}

void win_audio_viz_cleanup()
{
    if (nullptr != pMMDevice)
    {
        pMMDevice->Release();
    }

    if (nullptr != hStartedEvent)
    {
        CloseHandleOnExit closeStart(hStartedEvent);
    }

    if (nullptr != hStopEvent)
    {
        CloseHandleOnExit closeStop(hStopEvent);
    }

    if (nullptr != hThread)
    {
        CloseHandleOnExit closeThread(hThread);
    }
}

bool win_audio_viz_init(quasar_plugin_handle handle)
{
    plugHandle = handle;

    // calculate band frequencies
    const double step = (kfr::log(VIZ_FREQ_MAX / VIZ_FREQ_MIN) / spectrumFreqs.size()) / kfr::log(2.0);
    spectrumFreqs[0]  = (float) (VIZ_FREQ_MIN * kfr::pow(2.0, step / 2.0));

    for (size_t i = 1; i < spectrumFreqs.size(); ++i)
    {
        spectrumFreqs[i] = (float) (spectrumFreqs[i - 1] * kfr::pow(2.0, step));
    }

    // Get default device
    HRESULT              hr = S_OK;
    IMMDeviceEnumerator* pMMDeviceEnumerator;

    // activate a device enumerator
    hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**) &pMMDeviceEnumerator);
    if (FAILED(hr))
    {
        warn("CoCreateInstance(IMMDeviceEnumerator) failed: hr = 0x%08lx", hr);
        return false;
    }

    ReleaseOnExit releaseMMDeviceEnumerator(pMMDeviceEnumerator);

    // get the default render endpoint
    hr = pMMDeviceEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pMMDevice);
    if (FAILED(hr))
    {
        warn("IMMDeviceEnumerator::GetDefaultAudioEndpoint failed: hr = 0x%08lx", hr);
        win_audio_viz_cleanup();
        return false;
    }

    // create a "loopback capture has started" event
    hStartedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hStartedEvent)
    {
        warn("CreateEvent failed: last error is %lu", GetLastError());
        win_audio_viz_cleanup();
        return false;
    }

    // create a "stop capturing now" event
    hStopEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (NULL == hStopEvent)
    {
        warn("CreateEvent failed: last error is %lu", GetLastError());
        win_audio_viz_cleanup();
        return false;
    }

    hThread = CreateThread(
        NULL, 0, LoopbackCaptureThreadFunction, NULL, 0, NULL);
    if (NULL == hThread)
    {
        warn("CreateThread failed: last error is %lu", GetLastError());
        win_audio_viz_cleanup();
        return false;
    }

    // wait for either capture to start or the thread to end
    HANDLE waitArray[2] = { hStartedEvent, hThread };
    DWORD  dwWaitResult;
    dwWaitResult = WaitForMultipleObjects(
        ARRAYSIZE(waitArray), waitArray, FALSE, INFINITE);

    if (WAIT_OBJECT_0 + 1 == dwWaitResult)
    {
        warn("Thread aborted before starting to loopback capture: hr = 0x%08lx", threadResult);
        win_audio_viz_cleanup();
        return false;
    }

    if (WAIT_OBJECT_0 != dwWaitResult)
    {
        warn("Unexpected WaitForMultipleObjects return value %lu", dwWaitResult);
        win_audio_viz_cleanup();
        return false;
    }

    return true;
}

bool win_audio_viz_shutdown(quasar_plugin_handle handle)
{
    {
        WaitForSingleObjectOnExit waitForThread(hThread);
        SetEventOnExit            setStopEvent(hStopEvent);
    }

    DWORD exitCode;
    if (!GetExitCodeThread(hThread, &exitCode))
    {
        warn("GetExitCodeThread failed: last error is %lu", GetLastError());
        return false;
    }

    if (0 != exitCode)
    {
        warn("Loopback capture thread exit code is %lu; expected 0", exitCode);
        return false;
    }

    if (S_OK != threadResult)
    {
        warn("Thread HRESULT is 0x%08lx", threadResult);
        return false;
    }

    win_audio_viz_cleanup();

    return true;
}

bool win_audio_viz_get_data(size_t srcUid, quasar_data_handle hData)
{
    if (srcUid == sources->uid)
    {
        std::shared_lock<std::shared_mutex> lock(spectrumMutex);

        quasar_set_data_double_array(hData, spectrum[0].data(), spectrum[0].size());

        return true;
    }

    warn("Unknown source %Iu", srcUid);

    return false;
}

quasar_plugin_info_t info =
    {
      PLUGIN_NAME,
      PLUGIN_CODE,
      "v1",
      "me",
      "Supplies desktop audio frequency data",

      _countof(sources),
      sources,

      win_audio_viz_init,     //init
      win_audio_viz_shutdown, //shutdown
      win_audio_viz_get_data, //data
      nullptr,                //create setting
      nullptr                 // update setting
    };

quasar_plugin_info_t* quasar_plugin_load(void)
{
    return &info;
}
