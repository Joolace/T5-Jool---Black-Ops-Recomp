// xwma_decode.h - decodifica xWMA (WMA v2 di XAudio2) in PCM 16 bit, con FFmpeg caricato a runtime.
//
// Black Ops tiene i suoni "caricati" (spari, coltello, versi degli zombie...) in xWMA; quelli in streaming
// (musica, ambiente) sono PCM/ADPCM. XAudio2 su Windows decodifica xWMA da solo; qui lo fa libavcodec.
// Questo file è compilato SENZA bo1_compat.h (gli header di FFmpeg non vanno d'accordo con le macro Windows),
// quindi l'interfaccia usa solo tipi standard.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

namespace bo1
{
// Restituisce i campioni interleaved a 16 bit, oppure nullptr se FFmpeg non c'è o i dati non si decodificano.
// I risultati restano in una cache (limitata in memoria) indicizzata sul contenuto del buffer.
std::shared_ptr<const std::vector<int16_t>> DecodeXwma(const uint8_t *data, uint32_t bytes, uint32_t channels,
                                                       uint32_t sampleRate, uint32_t blockAlign,
                                                       uint32_t avgBytesPerSec, const uint32_t *cumulativeBytes,
                                                       uint32_t packetCount);
} // namespace bo1
