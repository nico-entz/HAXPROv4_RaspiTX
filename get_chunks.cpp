#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <string>

struct Chunk {
    std::string id;       // 4 Zeichen
    uint32_t size;        // Größe der Daten
    std::vector<uint8_t> data;
};

bool readChunk(std::ifstream& file, Chunk& chunk) {
    char id[4];
    if (!file.read(id, 4)) return false; // EOF

    chunk.id = std::string(id, 4);

    uint32_t size;
    if (!file.read(reinterpret_cast<char*>(&size), 4)) return false;

    // RIFF-Chunksize ist little-endian, ggf. anpassen je nach Plattform
    // Hier angenommen little-endian, sonst Byte-Swap nötig

    chunk.size = size;

    chunk.data.resize(size);
    if (!file.read(reinterpret_cast<char*>(chunk.data.data()), size)) return false;

    // RIFF Chunks sind 2-Byte aligned, falls Größe ungerade ist, 1 Byte Padding
    if (size % 2 == 1) file.seekg(1, std::ios::cur);

    return true;
}

int main() {
    std::ifstream file("data/1.webp", std::ios::binary);

    if (!file.is_open()) {
        std::cerr << "Kann Datei nicht öffnen\n";
        return 1;
    }

    // RIFF Header lesen
    char riffHeader[4];
    file.read(riffHeader, 4);
    if (std::string(riffHeader, 4) != "RIFF") {
        std::cerr << "Keine RIFF-Datei\n";
        return 1;
    }

    uint32_t riffSize;
    file.read(reinterpret_cast<char*>(&riffSize), 4);

    char webpHeader[4];
    file.read(webpHeader, 4);
    if (std::string(webpHeader, 4) != "WEBP") {
        std::cerr << "Keine WebP-Datei\n";
        return 1;
    }

    std::cout << "WebP RIFF Größe: " << riffSize << "\n";

    // Chunks auslesen
    int chunkCount = 0;
    while (true) {
        Chunk chunk;
        if (!readChunk(file, chunk)) break;

        std::cout << "Chunk " << chunkCount++
                  << ": ID = " << chunk.id
                  << ", Größe = " << chunk.size << "\n";

        // Beispiel: Chunk-Daten als separate Datei speichern
        std::string chunkFileName = "chunk_" + std::to_string(chunkCount) + "_" + chunk.id + ".bin";
        std::ofstream chunkFile(chunkFileName, std::ios::binary);
        chunkFile.write(reinterpret_cast<char*>(chunk.data.data()), chunk.size);
        chunkFile.close();
    }

    file.close();

    return 0;
}