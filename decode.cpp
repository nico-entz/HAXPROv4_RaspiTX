#include <webp/decode.h>
#include <webp/encode.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <sstream>
#include <iomanip>

void save_webp_preview(const uint8_t* rgba, int width, int height, int step) {
    uint8_t* output;
    size_t size = WebPEncodeRGBA(rgba, width, height, width * 4, 75, &output);

    if (size > 0 && output) {
        std::ostringstream fname;
        fname << "preview_" << std::setw(3) << std::setfill('0') << step << ".webp";

        std::ofstream file(fname.str(), std::ios::binary);
        file.write(reinterpret_cast<const char*>(output), size);
        file.close();

        std::cout << "Bildvorschau gespeichert: " << fname.str() << "\n";
        WebPFree(output);
    }
}

int main() {
    std::ifstream input("data/1.webp", std::ios::binary);
    if (!input) {
        std::cerr << "Datei input.webp nicht gefunden!\n";
        return 1;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(input)), {});
    size_t total_size = buffer.size();

    WebPDecoderConfig config;
    if (!WebPInitDecoderConfig(&config)) {
        std::cerr << "Decoder-Konfiguration fehlgeschlagen!\n";
        return 1;
    }

    config.output.colorspace = MODE_RGBA;
    WebPIDecoder* idec = WebPINewDecoder(&config.output);

    size_t chunk_size = 100;
    size_t offset = 0;
    int step = 0;

    while (offset < total_size) {
        size_t len = std::min(chunk_size, total_size - offset);
        VP8StatusCode status = WebPIAppend(idec, buffer.data() + offset, len);
        offset += len;

        if (status == VP8_STATUS_OK) {
            // Vollständig dekodierbar
            save_webp_preview(config.output.u.RGBA.rgba, config.output.width, config.output.height, step++);
            std::cout << "Bild vollständig dekodiert.\n";
            break;
        } else if (status == VP8_STATUS_SUSPENDED) {
            // Teilweise dekodiert, aber keine API für „wie viel“
            // Du kannst hier theoretisch die RGBA-Daten nehmen, sind aber evtl. unvollständig
            save_webp_preview(config.output.u.RGBA.rgba, config.output.width, config.output.height, step++);
            std::cout << "Bild teilweise dekodiert (suspended).\n";
        } else {
            std::cerr << "Dekodierfehler: " << status << "\n";
            break;
        }
    }

    WebPIDelete(idec);
    return 0;
}