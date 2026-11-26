// xwma_ffmpeg.cpp - vedi xwma_decode.h.
//
// FFmpeg è caricato con dlopen (libavcodec/libavutil della stessa versione degli header usati per compilare):
// se manca, i suoni xWMA restano muti e il resto del gioco funziona uguale.
// xWMA = pacchetti WMA v2 da nBlockAlign byte, senza extradata nel formato: come il demuxer xwma di FFmpeg
// gli diamo 6 byte di extradata con i flag di codifica 0x1F (exp VLC, bit reservoir, blocchi variabili).
#include "xwma_decode.h"

#if defined(__has_include)
#if __has_include(<libavcodec/avcodec.h>)
#define BO1_HAVE_FFMPEG 1
#endif
#endif

#ifdef BO1_HAVE_FFMPEG

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>
#include <libavutil/mem.h>
}

#include <dlfcn.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <list>
#include <mutex>
#include <unordered_map>

namespace
{
#define BO1_FF_STR2(x) #x
#define BO1_FF_STR(x) BO1_FF_STR2(x)

struct FFmpeg
{
    bool tried = false;
    bool ok = false;
    decltype(&avcodec_find_decoder) find_decoder = nullptr;
    decltype(&avcodec_alloc_context3) alloc_context3 = nullptr;
    decltype(&avcodec_open2) open2 = nullptr;
    decltype(&avcodec_send_packet) send_packet = nullptr;
    decltype(&avcodec_receive_frame) receive_frame = nullptr;
    decltype(&avcodec_free_context) free_context = nullptr;
    decltype(&av_packet_alloc) packet_alloc = nullptr;
    decltype(&av_packet_free) packet_free = nullptr;
    decltype(&av_frame_alloc) frame_alloc = nullptr;
    decltype(&av_frame_free) frame_free = nullptr;
    decltype(&av_mallocz) mallocz = nullptr;
    decltype(&av_channel_layout_default) layout_default = nullptr;
    decltype(&av_log_set_level) log_set_level = nullptr;

    bool Load()
    {
        if (tried)
            return ok;
        tried = true;
#ifdef __ANDROID__
        // Android: l'APK impacchetta solo librerie "lib*.so" (FFmpeg compilato senza numero di versione)
        const char *utilName = "libavutil.so";
        const char *codecName = "libavcodec.so";
#else
        const char *utilName = "libavutil.so." BO1_FF_STR(LIBAVUTIL_VERSION_MAJOR);
        const char *codecName = "libavcodec.so." BO1_FF_STR(LIBAVCODEC_VERSION_MAJOR);
#endif
        void *util = dlopen(utilName, RTLD_NOW | RTLD_GLOBAL);
        void *codec = util ? dlopen(codecName, RTLD_NOW | RTLD_GLOBAL) : nullptr;
        if (!util || !codec)
        {
            fprintf(stderr, "BO1 audio: %s/%s non trovate (%s): suoni xWMA muti. Installa libavcodec a 32 bit.\n",
                    utilName, codecName, dlerror());
            return false;
        }
#define BO1_FF_SYM(lib, field, name)                                                                                   \
    field = reinterpret_cast<decltype(field)>(dlsym(lib, #name));                                                      \
    if (!field)                                                                                                        \
    {                                                                                                                  \
        fprintf(stderr, "BO1 audio: simbolo FFmpeg mancante: %s\n", #name);                                            \
        return false;                                                                                                  \
    }
        BO1_FF_SYM(codec, find_decoder, avcodec_find_decoder)
        BO1_FF_SYM(codec, alloc_context3, avcodec_alloc_context3)
        BO1_FF_SYM(codec, open2, avcodec_open2)
        BO1_FF_SYM(codec, send_packet, avcodec_send_packet)
        BO1_FF_SYM(codec, receive_frame, avcodec_receive_frame)
        BO1_FF_SYM(codec, free_context, avcodec_free_context)
        BO1_FF_SYM(codec, packet_alloc, av_packet_alloc)
        BO1_FF_SYM(codec, packet_free, av_packet_free)
        BO1_FF_SYM(util, frame_alloc, av_frame_alloc)
        BO1_FF_SYM(util, frame_free, av_frame_free)
        BO1_FF_SYM(util, mallocz, av_mallocz)
        BO1_FF_SYM(util, layout_default, av_channel_layout_default)
        BO1_FF_SYM(util, log_set_level, av_log_set_level)
#undef BO1_FF_SYM
        log_set_level(AV_LOG_ERROR);
        ok = true;
        fprintf(stderr, "BO1 audio: decoder xWMA attivo (%s)\n", codecName);
        return true;
    }
};

FFmpeg g_ff;
std::mutex g_lock;

// ---- cache LRU dei suoni decodificati ----
constexpr size_t kCacheLimitBytes = 96u << 20; // 96 MB di PCM

struct CacheKey
{
    const uint8_t *data;
    uint32_t bytes;
    uint64_t hash;
    bool operator==(const CacheKey &o) const { return data == o.data && bytes == o.bytes && hash == o.hash; }
};
struct CacheKeyHash
{
    size_t operator()(const CacheKey &k) const
    {
        return std::hash<const void *>()(k.data) ^ (size_t)k.hash ^ ((size_t)k.bytes << 7);
    }
};
struct CacheEntry
{
    std::shared_ptr<const std::vector<int16_t>> pcm;
    std::list<CacheKey>::iterator lru;
};
std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> g_cache;
std::list<CacheKey> g_lru; // davanti = usato più di recente
size_t g_cacheBytes = 0;

// Lo stesso indirizzo può essere riusato da un altro suono dopo un cambio mappa: nella chiave c'è anche un
// hash dell'inizio e della fine dei dati.
uint64_t HashData(const uint8_t *p, uint32_t n)
{
    uint64_t h = 1469598103934665603ull;
    uint32_t head = n < 256 ? n : 256;
    for (uint32_t i = 0; i < head; ++i)
        h = (h ^ p[i]) * 1099511628211ull;
    for (uint32_t i = n > 256 ? n - 256 : 0; i < n; ++i)
        h = (h ^ p[i]) * 1099511628211ull;
    return h;
}

int16_t ToS16(float v)
{
    if (!(v == v))
        return 0;
    v *= 32768.0f;
    if (v > 32767.0f)
        return 32767;
    if (v < -32768.0f)
        return -32768;
    return (int16_t)lrintf(v);
}

void AppendFrame(const AVFrame *frame, uint32_t channels, std::vector<int16_t> &out)
{
    const int n = frame->nb_samples;
    const size_t base = out.size();
    out.resize(base + (size_t)n * channels);
    int16_t *dst = out.data() + base;
    switch (frame->format)
    {
    case AV_SAMPLE_FMT_FLTP:
        for (uint32_t c = 0; c < channels; ++c)
        {
            const float *src = (const float *)frame->extended_data[c];
            for (int i = 0; i < n; ++i)
                dst[(size_t)i * channels + c] = ToS16(src[i]);
        }
        break;
    case AV_SAMPLE_FMT_FLT:
    {
        const float *src = (const float *)frame->extended_data[0];
        for (size_t i = 0; i < (size_t)n * channels; ++i)
            dst[i] = ToS16(src[i]);
        break;
    }
    case AV_SAMPLE_FMT_S16P:
        for (uint32_t c = 0; c < channels; ++c)
        {
            const int16_t *src = (const int16_t *)frame->extended_data[c];
            for (int i = 0; i < n; ++i)
                dst[(size_t)i * channels + c] = src[i];
        }
        break;
    case AV_SAMPLE_FMT_S16:
        memcpy(dst, frame->extended_data[0], (size_t)n * channels * sizeof(int16_t));
        break;
    default:
        memset(dst, 0, (size_t)n * channels * sizeof(int16_t));
        break;
    }
}

std::shared_ptr<std::vector<int16_t>> Decode(const uint8_t *data, uint32_t bytes, uint32_t channels,
                                             uint32_t sampleRate, uint32_t blockAlign, uint32_t avgBytesPerSec,
                                             const uint32_t *cumulativeBytes, uint32_t packetCount)
{
    // Il decoder WMA ricava dal bitrate parametri del bitstream (byte_offset_bits): serve quello vero, cioè
    // nAvgBytesPerSec del formato come lo usa XAudio2 (6000 byte/s per canale nei suoni di Black Ops).
    uint32_t packets = bytes / blockAlign;
    if (packetCount && packetCount < packets)
        packets = packetCount;
    uint64_t tableFrames = 0;
    if (cumulativeBytes && packetCount)
        tableFrames = cumulativeBytes[packetCount - 1] / (sizeof(int16_t) * channels);
    const int64_t bitRate = (int64_t)avgBytesPerSec * 8;
    int badPackets = 0;

    const AVCodec *codec = g_ff.find_decoder(AV_CODEC_ID_WMAV2);
    if (!codec)
        return nullptr;
    AVCodecContext *ctx = g_ff.alloc_context3(codec);
    if (!ctx)
        return nullptr;
    ctx->sample_rate = (int)sampleRate;
    ctx->bit_rate = bitRate;
    ctx->block_align = (int)blockAlign;
    g_ff.layout_default(&ctx->ch_layout, (int)channels);
    ctx->extradata = (uint8_t *)g_ff.mallocz(6 + AV_INPUT_BUFFER_PADDING_SIZE);
    if (ctx->extradata)
    {
        ctx->extradata_size = 6;
        ctx->extradata[4] = 31; // flag di codifica xWMA (come libavformat/xwma.c)
    }
    if (g_ff.open2(ctx, codec, nullptr) < 0)
    {
        g_ff.free_context(&ctx);
        return nullptr;
    }

    auto pcm = std::make_shared<std::vector<int16_t>>();
    AVPacket *pkt = g_ff.packet_alloc();
    AVFrame *frame = g_ff.frame_alloc();
    std::vector<uint8_t> padded(blockAlign + AV_INPUT_BUFFER_PADDING_SIZE, 0);

    auto drain = [&]() {
        while (g_ff.receive_frame(ctx, frame) == 0)
            AppendFrame(frame, channels, *pcm);
    };
    for (uint32_t i = 0; pkt && frame && i < packets; ++i)
    {
        memcpy(padded.data(), data + (size_t)i * blockAlign, blockAlign);
        pkt->data = padded.data();
        pkt->size = (int)blockAlign;
        if (g_ff.send_packet(ctx, pkt) < 0)
        {
            ++badPackets;
            continue; // pacchetto rovinato: si va avanti come farebbe XAudio2
        }
        drain();
    }
    if (pkt && frame)
    {
        g_ff.send_packet(ctx, nullptr);
        drain();
    }
    g_ff.frame_free(&frame);
    g_ff.packet_free(&pkt);
    g_ff.free_context(&ctx);

    const uint64_t decodedFrames = pcm->size() / channels;
    static int s_logged;
    if (s_logged < 12)
    {
        ++s_logged;
        fprintf(stderr,
                "BO1 audio: xWMA %u ch %u Hz blocco %u pacchetti %u (scartati %d) bitrate %lld -> %llu frame "
                "(tabella %llu)\n",
                channels, sampleRate, blockAlign, packets, badPackets, (long long)bitRate,
                (unsigned long long)decodedFrames, (unsigned long long)tableFrames);
    }
    // la tabella di XAudio2 dice quanti frame produce il suono: si adegua la lunghezza solo se è coerente
    // (differenza di qualche blocco al massimo), altrimenti la tabella è in un'altra unità e si ignora
    if (tableFrames && decodedFrames)
    {
        uint64_t diff = tableFrames > decodedFrames ? tableFrames - decodedFrames : decodedFrames - tableFrames;
        if (diff <= 8192)
            pcm->resize((size_t)tableFrames * channels, 0);
    }
    if (pcm->empty() && tableFrames)
        pcm->assign((size_t)tableFrames * channels, 0); // silenzio della durata giusta: la voce finisce in orario
    if (pcm->empty())
        return nullptr;
    return pcm;
}
} // namespace

namespace bo1
{
std::shared_ptr<const std::vector<int16_t>> DecodeXwma(const uint8_t *data, uint32_t bytes, uint32_t channels,
                                                       uint32_t sampleRate, uint32_t blockAlign,
                                                       uint32_t avgBytesPerSec, const uint32_t *cumulativeBytes,
                                                       uint32_t packetCount)
{
    if (!data || !bytes || !channels || channels > 8 || !blockAlign || !sampleRate)
        return nullptr;
    std::lock_guard<std::mutex> l(g_lock);
    if (!g_ff.Load())
        return nullptr;

    CacheKey key{ data, bytes, HashData(data, bytes) };
    auto it = g_cache.find(key);
    if (it != g_cache.end())
    {
        g_lru.splice(g_lru.begin(), g_lru, it->second.lru);
        return it->second.pcm;
    }

    std::shared_ptr<const std::vector<int16_t>> pcm =
        Decode(data, bytes, channels, sampleRate, blockAlign, avgBytesPerSec, cumulativeBytes, packetCount);
    if (!pcm)
    {
        static int s_failures;
        if (s_failures++ < 10)
            fprintf(stderr, "BO1 audio: decodifica xWMA fallita (%u byte, %u canali, %u Hz, blocco %u)\n", bytes,
                    channels, sampleRate, blockAlign);
        return nullptr;
    }

    size_t size = pcm->size() * sizeof(int16_t);
    while (!g_lru.empty() && g_cacheBytes + size > kCacheLimitBytes)
    {
        auto old = g_cache.find(g_lru.back());
        if (old != g_cache.end())
        {
            g_cacheBytes -= old->second.pcm->size() * sizeof(int16_t);
            g_cache.erase(old);
        }
        g_lru.pop_back();
    }
    g_lru.push_front(key);
    g_cache[key] = CacheEntry{ pcm, g_lru.begin() };
    g_cacheBytes += size;
    return pcm;
}
} // namespace bo1

#else // senza header di FFmpeg: xWMA muto

#include <cstdio>

namespace bo1
{
std::shared_ptr<const std::vector<int16_t>> DecodeXwma(const uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t,
                                                       uint32_t, const uint32_t *, uint32_t)
{
    static bool s_warned;
    if (!s_warned)
    {
        s_warned = true;
        fprintf(stderr, "BO1 audio: compilato senza FFmpeg: suoni xWMA muti\n");
    }
    return nullptr;
}
} // namespace bo1

#endif
