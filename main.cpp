/* 
 * File:   main.cpp
 * Author: philippe SIMIER Lycée Touchard Washington
 * 
 * Programme test unitaire classe SX1278
 * test les 2 méthodes continuous_receive() & send() 
 * test l'operateur << 
 *
 * Created on 7 juillet 2024, 16:14
 */

#include <cstdlib>
#include <iostream>
#include <string> 
#include <fstream>
#include <vector>
#include <cstdint>
#include <random>
#include <thread>
#include <mutex>
#include <chrono>
#include <condition_variable>
#include "SX1278.h"
#include <zlib.h>

constexpr std::size_t CHUNK_SIZE = 100;   // chunk size

std::mutex ackMutex;
std::condition_variable ackCV;
bool ackReceived = false;
bool ackSuccess = false;
uint8_t lastAckSeq = 0;
int file_count = 1;

// --- Diese Callback-Funktion wird bei ACK/NACK-Empfang aufgerufen ---
void ackCallback(char* payload, int8_t len, int rssi, float snr) {
    if (len < 2) return;

    std::unique_lock<std::mutex> lock(ackMutex);
    lastAckSeq = static_cast<uint8_t>(payload[0]);
    ackSuccess = (payload[1] == 0xAA);
    ackReceived = true;
    ackCV.notify_one();
}

using namespace std;

void callback_Tx(void);  // user callback function for when a packet is transmited.

// --- Funktion zum Senden der Telemetrie ---
bool sendTelemetry(SX1278& radio) {
    std::ifstream file("data/telemetry.csv");
    std::string lastLine;
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (!line.empty()) {
                lastLine = line;
            }
        }
        file.close();
    } else {
        lastLine = "FEHLER: Datei nicht lesbar";
    }

    std::string telemetry = lastLine;
    uint8_t telemetryType = 1;
    uint8_t telemetrySeq = 255;  // Fester Wert, nicht kollidierend

    std::vector<int8_t> telemetryPacket;
    telemetryPacket.push_back(static_cast<int8_t>(telemetryType));
    telemetryPacket.push_back(static_cast<int8_t>(telemetrySeq));
    telemetryPacket.push_back(0);  // totalChunks = 0 für Telemetrie
    telemetryPacket.insert(telemetryPacket.end(), telemetry.begin(), telemetry.end());

    uint32_t telemetryCrc = crc32(0L, Z_NULL, 0);
    telemetryCrc = crc32(telemetryCrc, reinterpret_cast<const Bytef*>(telemetryPacket.data()), telemetryPacket.size());
    for (int i = 0; i < 4; ++i) {
        telemetryPacket.push_back(static_cast<int8_t>((telemetryCrc >> (i * 8)) & 0xFF));
    }

    const int maxRetries = 50;
    for (int attempt = 1; attempt <= maxRetries; ++attempt) {
        radio.send(telemetryPacket.data(), static_cast<uint8_t>(telemetryPacket.size()));
        std::cout << "📡 Telemetrie gesendet (Seq " << static_cast<int>(telemetrySeq) << ") – Versuch " << attempt << "\n";

        {
            std::unique_lock<std::mutex> lock(ackMutex);
            ackReceived = false;
            bool timedOut = !ackCV.wait_for(lock, std::chrono::milliseconds(5000), [] { return ackReceived; });

            if (!timedOut && ackSuccess && lastAckSeq == telemetrySeq) {
                return true;  // Telemetrie erfolgreich gesendet
            }
        }

        std::cout << "→ Keine gültige ACK für Telemetrie – wiederhole\n";
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cerr << "❌ Telemetrie konnte nicht bestätigt werden. Abbruch.\n";
    return false;
}

void sendFileChunks(SX1278& radio, const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "Fehler: konnte \"" << filename << "\" nicht öffnen!\n";
        return;
    }

    file.seekg(0, std::ios::end);
    std::size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    const std::size_t totalChunks = (fileSize + CHUNK_SIZE - 1) / CHUNK_SIZE;

    uint8_t type = 0;
    uint8_t seq = 0;
    std::vector<char> rawBuf(CHUNK_SIZE);
    std::vector<int8_t> packet;

    for (std::size_t chunkIdx = 0; chunkIdx < totalChunks; ++chunkIdx) {
        file.read(rawBuf.data(), CHUNK_SIZE);
        std::streamsize bytesRead = file.gcount();
        if (bytesRead <= 0) break;

        // --- Paket aufbauen ---
        type = 0;
        packet.clear();
        packet.push_back(static_cast<int8_t>(type));  // type = 0 für Datei-Daten
        packet.push_back(static_cast<int8_t>(seq));   // Sequenznummer
        packet.push_back(static_cast<int8_t>(totalChunks));  // Anzahl aller Chunks
        packet.insert(packet.end(), rawBuf.begin(), rawBuf.begin() + bytesRead);

        // --- CRC berechnen ---
        uint32_t crc = crc32(0L, Z_NULL, 0);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(packet.data()), packet.size());
        for (int i = 0; i < 4; ++i) {
            packet.push_back(static_cast<int8_t>((crc >> (i * 8)) & 0xFF));  // little endian
        }

        // --- Senden mit Retry-Logik ---
        const int maxRetries = 50;
        bool success = false;
        for (int attempt = 1; attempt <= maxRetries; ++attempt) {
            radio.send(packet.data(), static_cast<uint8_t>(packet.size()));
            std::cout << "Chunk " << (chunkIdx + 1) << "/" << totalChunks
                      << " (Seq " << static_cast<int>(seq)
                      << ", " << bytesRead << " Bytes) gesendet – Versuch " << attempt << "\n";

            {
                std::unique_lock<std::mutex> lock(ackMutex);
                ackReceived = false;
                bool timedOut = !ackCV.wait_for(lock, std::chrono::milliseconds(5000), [] { return ackReceived; });

                if (!timedOut && ackSuccess && lastAckSeq == seq) {
                    success = true;
                    break;
                }
            }

            std::cout << "→ Keine gültige ACK – wiederhole Senden\n";
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        if (!success) {
            std::cerr << "❌ Chunk " << static_cast<int>(seq) << " konnte nicht bestätigt werden. Abbruch.\n";
            return;
        }

        ++seq;  // Nur für Datei-Daten erhöhen

        // --- Nach jedem 10. Chunk Telemetrie senden ---
        if ((chunkIdx + 1) % 10 == 0) {
            // Hier kannst du optional weiterhin Telemetrie senden,
            // oder es entfernen, wenn du nur vor dem Bild senden willst.
        }
    }

    std::cout << "✅ Dateiübertragung abgeschlossen.\n";
}

void takePhoto(const char* fn) {
    // Erzeuge den Shell-Befehl mit dem Dateinamen
    std::string cmd = "timeout 10s rpicam-still -t 1 -o - | convert - -resize 320x240 -interlace Plane -quality 100 \"";
    cmd += fn;
    cmd += "\"";

    int result = std::system(cmd.c_str());

    if (result == 0) {
        std::cout << "Bild erfolgreich aufgenommen und konvertiert: " << fn << "\n";
    } else {
        std::cerr << "Fehler beim Ausführen des Befehls.\n";
    }
}

void convertPhoto(const char* inputFile, const char* outputFile) {
    // Erstelle den cwebp-Befehl
    std::string cmd = "timeout 10s cwebp -q 40 -m 6 -mt -af \"";
    cmd += inputFile;
    cmd += "\" -o \"";
    cmd += outputFile;
    cmd += "\"";

    int result = std::system(cmd.c_str());

    if (result == 0) {
        std::cout << "Bild erfolgreich konvertiert: " << outputFile << "\n";
    } else {
        std::cerr << "Fehler bei der Bildkonvertierung mit cwebp.\n";
    }
}

int main(int argc, char** argv) {
    try {
        loRa.onRxDone(ackCallback); // Register a user callback function 
        loRa.onTxDone(callback_Tx); // Register a user callback function
        loRa.begin();               // settings the radio     
        loRa.continuous_receive();  // Puts the radio in continuous receive mode.

        std::this_thread::sleep_for(std::chrono::seconds(1));

        while(1){
            std::string inputFile = "data/" + std::to_string(file_count) + ".jpg";
            std::string outputFile = "data/" + std::to_string(file_count) + ".webp";

            takePhoto(inputFile.c_str());
            convertPhoto(inputFile.c_str(), outputFile.c_str());

            // Telemetrie senden vor Bildübertragung
            if (!sendTelemetry(loRa)) {
                std::cerr << "Telemetrie-Senden fehlgeschlagen. Wiederhole in 5 Sekunden.\n";
                std::this_thread::sleep_for(std::chrono::seconds(5));
                continue;  // Versuche nochmal, kein Fortschritt
            }

            sendFileChunks(loRa, outputFile.c_str());
            file_count++;

            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    } catch (const std::runtime_error &e) {     
        cout << "Exception caught: " << e.what() << endl;
    }
    return 0;
}

void callback_Tx(void) {
    //cout << "Tx done : " << endl;
}
