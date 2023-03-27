#include <array>
#include <complex>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <thread>
#include <unordered_map>
#include <vector>

#include <extension_api.h>
#include <extension_support.hpp>

#include <fmt/core.h>
#include <fmt/xchar.h>

#include <pulse/error.h>
#include <pulse/simple.h>

#include <kfr/base.hpp>
#include <kfr/dft.hpp>
#include <kfr/dsp.hpp>

constexpr std::string_view EXT_FULLNAME = "PulseAudio Audio Visualization Data";
constexpr std::string_view EXT_NAME     = "pulse_viz";

#define CLAMP01(x) kfr::clamp(x, 0.0, 1.0)

template<typename T>
requires std::numeric_limits<T>::is_integer
constexpr float normalizeAsFloat(T val)
{
    constexpr float imax = 1.0f / std::numeric_limits<T>::max();
    return val * imax;
}

#define qlog(l, ...)                                                      \
  {                                                                       \
    auto msg = fmt::format("{}: {}", EXT_NAME, fmt::format(__VA_ARGS__)); \
    quasar_log(l, msg.c_str());                                           \
  }

#define debug(...) qlog(QUASAR_LOG_DEBUG, __VA_ARGS__)
#define info(...)  qlog(QUASAR_LOG_INFO, __VA_ARGS__)
#define warn(...)  qlog(QUASAR_LOG_WARNING, __VA_ARGS__)

quasar_data_source_t sources[] = {
    { "fft", 16667, 0, 0},
    {"band", 16667, 0, 0},
};

namespace
{
    enum Source : size_t
    {
        FFT,
        BAND,
        NUM_SOURCES
    };

    enum Channel : uint8_t
    {
        CHANNEL_FL,
        CHANNEL_FR,
        MAX_CHANNELS
    };

    // Handles and threads
    quasar_ext_handle                  extHandle = nullptr;
    pa_simple*                         server    = nullptr;
    std::unordered_map<size_t, Source> sourceMap;
    std::jthread                       pThread;  // Processing thread
    std::shared_mutex                  mutex;

    // Audio spec
    constexpr pa_sample_spec spec          = {.format = PA_SAMPLE_S16LE, .rate = 48000, .channels = Channel::MAX_CHANNELS};
    constexpr size_t         buffer_len_ms = 10;
    constexpr size_t         buffer_size   = (size_t) (spec.rate * 2 * spec.channels * (buffer_len_ms / 1000.0));
    constexpr size_t         frame_size    = sizeof(int16_t) * spec.channels;
    constexpr size_t         num_frames    = buffer_size / frame_size;

    // DSP
    size_t                                                           fftSize{};      // size of FFT (parsed from options)
    size_t                                                           fftOverlap{};   // number of samples between FFT calculations
    size_t                                                           nBands{};       // number of frequency bands (parsed from options)
    double                                                           freqMin{};      // min freq for band measurement
    double                                                           freqMax{};      // max freq for band measurement
    double                                                           sensitivity{};  // dB range for FFT/Band return values (parsed from options)
    std::array<size_t, 2>                                            envFFT{};       // FFT attack/decay times in ms (parsed from options)

    std::array<kfr::dft_plan_real_ptr<float>, Channel::MAX_CHANNELS> fftPlan;    // FFT plans for each channel
    std::array<std::vector<float>, Channel::MAX_CHANNELS>            fftIn;      // buffer for each channel's FFT input
    std::array<std::vector<float>, Channel::MAX_CHANNELS>            fftOut;     // buffer for each channel's FFT output
    kfr::univector<float>                                            fftKWdw;    // window function coefficients
    kfr::univector<float>                                            fftTmpIn;   // temp FFT processing buffer
    kfr::univector<std::complex<float>>                              fftTmpOut;  // temp FFT processing buffer
    size_t                                                           fftBufW{};  // write index for input ring buffers
    int                                                              fftBufP{};  // decremental counter - process FFT at zero
    std::array<float, 2>                                             kFFT{};     // FFT attack/decay filter constants

    float                                                            fftScalar, bandScalar, df = 0;

    std::array<std::vector<float>, Channel::MAX_CHANNELS>            bandOut;     // buffer of band values
    std::vector<float>                                               bandFreq{};  // buffer of band max frequencies
    std::span<std::byte>                                             buffer{};
    kfr::univector<kfr::u8>                                          temp;

    std::array<std::vector<double>, Source::NUM_SOURCES>             output;  // output buffer

    bool                                                             init_buffers()
    {
        std::lock_guard lk(mutex);

        if (fftSize)
        {
            // output buffers
            output[Source::FFT].resize((fftSize / 2) + 1, 0.0);

            fftScalar = (float) (1.0 / kfr::sqrt(fftSize));

            for (auto&& iChan : std::views::iota((size_t) 0, (size_t) spec.channels))
            {
                fftPlan[iChan].reset(new kfr::dft_plan_real<float>(fftSize));
                fftIn[iChan].resize(fftSize, 0.0f);
                fftOut[iChan].resize(fftSize, 0.0f);
                temp.resize(fftPlan[iChan]->temp_size);
            }

            fftTmpIn.resize(fftSize, 0.0f);
            fftTmpOut.resize(fftSize, {0.0f, 0.0f});
            fftBufP = fftSize - fftOverlap;

            fftKWdw = kfr::window_hann(fftSize);
        }

        if (nBands)
        {
            // output buffers
            output[Source::BAND].resize(nBands, 0.0);

            bandScalar = 2.0f / (float) spec.rate;
            df         = (float) spec.rate / fftSize;

            bandFreq.resize(nBands, 0.0f);
            const double step = (kfr::log(freqMax / freqMin) / nBands) / kfr::log(2.0);
            bandFreq[0]       = (float) (freqMin * kfr::pow(2.0, step / 2.0));

            for (auto&& iBand : std::views::iota((size_t) 1, nBands))
            {
                bandFreq[iBand] = (float) (bandFreq[iBand - 1] * kfr::pow(2.0, step));
            }

            for (auto&& iChan : std::views::iota((size_t) 0, (size_t) spec.channels))
            {
                bandOut[iChan].resize(nBands, 0.0f);
            }
        }

        if (!buffer.data())
        {
            info("Initializing audio buffer size {}", buffer_size);
            buffer = std::span{new std::byte[buffer_size](), buffer_size};
        }

        return (buffer.data() != nullptr);
    }
}  // namespace

void pulse_viz_process()
{
    int error = 0;

    if (pa_simple_read(server, buffer.data(), buffer.size(), &error) == 0)
    {
        std::lock_guard lk(mutex);
        if (fftSize)
        {
            int16_t* sI16 = (int16_t*) buffer.data();

            for ([[maybe_unused]] auto&& _ : std::views::iota((size_t) 0, num_frames))
            {
                // fill ring buffers (demux streams)
                for (auto&& chan : std::views::iota((size_t) 0, (size_t) spec.channels))
                {
                    (fftIn[chan])[fftBufW] = normalizeAsFloat(*sI16++);
                }

                fftBufW = (fftBufW + 1) % fftSize;

                // if overlap limit reached, process FFTs for each channel
                if (!--fftBufP)
                {
                    for (auto&& iChan : std::views::iota((size_t) 0, (size_t) spec.channels))
                    {
                        // copy from the ring buffer to temp space
                        std::memcpy(fftTmpIn.data(), fftIn[iChan].data() + fftBufW, (fftSize - fftBufW) * sizeof(float));
                        if (fftSize - fftBufW < fftTmpIn.size())
                        {
                            std::memcpy(fftTmpIn.data() + (fftSize - fftBufW), fftIn[iChan].data(), fftBufW * sizeof(float));
                        }

                        // apply the windowing function
                        fftTmpIn = fftTmpIn * fftKWdw;

                        fftPlan[iChan]->execute(fftTmpOut, fftTmpIn, temp);

                        auto scaled = kfr::cabssqr(fftTmpOut) * fftScalar;

                        // filter the bin levels as with peak measurements
                        for (auto&& bin : std::views::iota((size_t) 0, fftSize))
                        {
                            float x0             = (fftOut[iChan])[bin];
                            float x1             = kfr::get_element(scaled, {bin});
                            x0                   = x1 + kFFT[(x1 < x0)] * (x0 - x1);
                            (fftOut[iChan])[bin] = x0;
                        }
                    }

                    fftBufP = fftSize - fftOverlap;
                }
            }

            // integrate FFT results into log-scale frequency bands
            if (nBands)
            {
                for (auto&& channel : std::views::iota((size_t) 0, (size_t) spec.channels))
                {
                    std::fill(bandOut[channel].begin(), bandOut[channel].end(), 0.0f);
                    size_t iBin  = 0;
                    size_t iBand = 0;
                    float  f0    = 0.0f;

                    while (iBin <= (fftSize / 2) and iBand < nBands)
                    {
                        float  fLin1 = ((float) iBin + 0.5f) * df;
                        float  fLog1 = bandFreq[iBand];
                        float  x     = (fftOut[channel])[iBin];
                        float& y     = (bandOut[channel])[iBand];

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

        pa_simple_flush(server, nullptr);
    }
    else
    {
        warn("pa_simple_read error: {}", pa_strerror(error));
    }
}

bool pulse_viz_init(quasar_ext_handle handle)
{
    extHandle                 = handle;

    sourceMap[sources[0].uid] = Source::FFT;
    sourceMap[sources[1].uid] = Source::BAND;

    int error                 = 0;
    server                    = pa_simple_new(nullptr,  // Use the default server.
        "Quasar pulse_viz",          // Our application's name.
        PA_STREAM_RECORD,
        nullptr,                     // Use the default device.
        "Audio Visualization Data",  // Description of our stream.
        &spec,                       // Our sample format.
        nullptr,                     // Use default channel map
        nullptr,                     // Use default buffering attributes.
        &error                       // Error code.
    );

    if (server == nullptr)
    {
        warn("pa_simple_new error: {}", pa_strerror(error));
        return false;
    }

    info("Connected to PulseAudio server");

    if (!init_buffers())
    {
        warn("Error initializing buffers");
        return false;
    }

    pThread = std::jthread{[&](std::stop_token token) {
        while (!token.stop_requested())
        {
            pulse_viz_process();
        }
    }};

    return (server != nullptr);
}

bool pulse_viz_shutdown(quasar_ext_handle handle)
{
    if (pThread.joinable())
    {
        pThread.request_stop();
        pThread.join();
    }

    if (server)
    {
        pa_simple_free(server);
        server = nullptr;
    }

    for (auto&& iChan : std::views::iota((size_t) 0, (size_t) spec.channels))
    {
        if (fftPlan[iChan])
            fftPlan[iChan].reset();
    }

    if (buffer.data())
    {
        delete[] buffer.data();
        buffer = std::span<std::byte>();
    }

    return true;
}

bool pulse_viz_get_data(size_t srcUid, quasar_data_handle hData, char* args)
{
    std::shared_lock lk(mutex);

    size_t           type = sourceMap[srcUid];

    switch (type)
    {
        case Source::FFT:
            {
                if (fftSize)
                {
                    double x;

                    for (auto&& i : std::views::iota((size_t) 0, (fftSize / 2) + 1))
                    {
                        if (spec.channels >= 2)
                        {
                            x = (fftOut[0][i] + fftOut[1][i]) * 0.5;
                        }
                        else
                        {
                            x = fftOut[0][i];
                        }

                        x                      = CLAMP01(x);
                        x                      = kfr::max(0.0, sensitivity * kfr::log10(x) + 1.0);
                        output[Source::FFT][i] = x;
                    }

                    quasar_set_data_double_vector(hData, output[Source::FFT]);

                    return true;
                }
                break;
            }

        case Source::BAND:
            {
                if (nBands)
                {
                    double x;

                    for (auto&& i : std::views::iota((size_t) 0, nBands))
                    {
                        if (spec.channels >= 2)
                        {
                            x = (bandOut[0][i] + bandOut[1][i]) * 0.5;
                        }
                        else
                        {
                            x = bandOut[0][i];
                        }

                        x                       = CLAMP01(x);
                        x                       = kfr::max(0.0, sensitivity * kfr::log10(x) + 1.0);
                        output[Source::BAND][i] = x;
                    }

                    quasar_set_data_double_vector(hData, output[Source::BAND]);

                    return true;
                }
                break;
            }

        default:
            break;
    }

    return true;
}

quasar_settings_t* pulse_viz_create_settings(quasar_ext_handle handle)
{
    extHandle                   = handle;

    quasar_settings_t* settings = quasar_create_settings(extHandle);

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
}

void pulse_viz_update_settings(quasar_settings_t* settings)
{
    std::unique_lock lk(mutex);
    bool             needs_reinit = false;

    size_t           fft          = quasar_get_uint_setting(extHandle, settings, "FFTSize");
    if (fft < 0 or fft & 1)
    {
        warn("Invalid FFTSize {}: must be an even integer >= 0. (powers of 2 work best)", fft);
    }
    else if (fft != fftSize)
    {
        fftSize      = fft;
        needs_reinit = true;
    }

    if (fftSize)
    {
        size_t overlap = quasar_get_uint_setting(extHandle, settings, "FFTOverlap");
        if (overlap < 0 or overlap >= fftSize)
        {
            warn("Invalid FFTOverlap {}: must be an integer between 0 and FFTSize({}).", overlap, fftSize);
        }
        else
        {
            fftOverlap = overlap;
        }
    }

    size_t numbands = quasar_get_uint_setting(extHandle, settings, "Bands");
    if (numbands != nBands)
    {
        nBands       = numbands;
        needs_reinit = true;
    }

    double fMin = quasar_get_double_setting(extHandle, settings, "FreqMin");
    double fMax = quasar_get_double_setting(extHandle, settings, "FreqMax");

    if (fMin != freqMin or fMax != freqMax)
    {
        freqMin      = fMin;
        freqMax      = fMax;
        needs_reinit = true;
    }

    envFFT[0] = quasar_get_uint_setting(extHandle, settings, "FFTAttack");
    envFFT[1] = quasar_get_uint_setting(extHandle, settings, "FFTDecay");

    // (re)parse gain constants
    sensitivity = 10.0 / std::max(1.0, quasar_get_double_setting(extHandle, settings, "Sensitivity"));

    // regenerate filter constants

    const double freq = (double) spec.rate;

    if (fftSize)
    {
        kFFT[0] = (float) kfr::exp(kfr::log10(0.01) / (freq / (fftSize - fftOverlap) * (double) envFFT[0] * 0.001));
        kFFT[1] = (float) kfr::exp(kfr::log10(0.01) / (freq / (fftSize - fftOverlap) * (double) envFFT[1] * 0.001));
    }

    lk.unlock();

    if (needs_reinit)
    {
        init_buffers();
    }
}

quasar_ext_info_fields_t fields = {.version = "3.0",
    .author                                 = "r52",
    .description                            = "Provides desktop audio frequency data for PulseAudio based environments.",
    .url                                    = "https://github.com/r52/quasar"};

quasar_ext_info_t        info   = {
    QUASAR_API_VERSION,
    &fields,

    std::size(sources),
    sources,

    pulse_viz_init,             // init
    pulse_viz_shutdown,         // shutdown
    pulse_viz_get_data,         // data
    pulse_viz_create_settings,  // create setting
    pulse_viz_update_settings   // update setting
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
