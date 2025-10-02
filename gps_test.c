#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

// Hilfsfunktion: NMEA-Grad/Min in Dezimalgrad umwandeln
double nmea_to_decimal(const char* nmea_coord, char direction) {
    if (strlen(nmea_coord) < 6) return 0.0;
    double deg = atof(nmea_coord) / 100.0;
    int degrees = (int)deg;
    double minutes = (deg - degrees) * 100;
    double decimal = degrees + minutes / 60.0;
    if (direction == 'S' || direction == 'W') decimal = -decimal;
    return decimal;
}

// Öffnet seriellen Port mit 9600 Baud, 8N1, keine Flow-Control
int open_serial(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        perror("Fehler beim Öffnen des seriellen Ports");
        return -1;
    }
    struct termios options;
    tcgetattr(fd, &options);
    cfsetispeed(&options, B9600);
    cfsetospeed(&options, B9600);
    options.c_cflag &= ~PARENB; // kein Parity
    options.c_cflag &= ~CSTOPB; // 1 Stopbit
    options.c_cflag &= ~CSIZE;
    options.c_cflag |= CS8;     // 8 Datenbits
    options.c_cflag &= ~CRTSCTS; // keine Hardware Flow Control
    options.c_cflag |= CREAD | CLOCAL; // Enable receiver, keine Modemkontrolle
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); // raw input
    options.c_iflag &= ~(IXON | IXOFF | IXANY); // keine Software Flow Control
    options.c_oflag &= ~OPOST; // raw output
    tcsetattr(fd, TCSANOW, &options);
    return fd;
}

int main() {
    const char* device = "/dev/serial0"; // ggf. anpassen
    int fd = open_serial(device);
    if (fd < 0) return 1;

    FILE* serial = fdopen(fd, "r");
    if (!serial) {
        perror("fdopen fehlgeschlagen");
        close(fd);
        return 1;
    }

    char line[256];
    while (fgets(line, sizeof(line), serial)) {
        if (strncmp(line, "$GPGGA,", 7) == 0) {
            // GGA-Satz parsen: Format (vereinfacht)
            // $GPGGA,time,lat,N,lon,E,fix,...,alt,M,...
            char time_str[16], lat[16], lat_dir, lon[16], lon_dir, fix_quality;
            double altitude;
            int fields = sscanf(line, "$GPGGA,%15[^,],%15[^,],%c,%15[^,],%c,%c,%*d,%*f,%lf",
                                time_str, lat, &lat_dir, lon, &lon_dir, &fix_quality, &altitude);
            if (fields >= 7 && fix_quality > '0') { // fix_quality > 0 = gültiger Fix
                double latitude = nmea_to_decimal(lat, lat_dir);
                double longitude = nmea_to_decimal(lon, lon_dir);

                // Zeit hhmmss.ss in hh, mm, ss zerlegen
                int hh = 0, mm = 0, ss = 0;
                if (strlen(time_str) >= 6) {
                    sscanf(time_str, "%2d%2d%2d", &hh, &mm, &ss);
                }

                // Datum von Systemzeit holen
                time_t now = time(NULL);
                struct tm *tm_now = gmtime(&now);

                // tm_now ist UTC Zeit; ersetze hh, mm, ss durch GPS Zeit
                tm_now->tm_hour = hh;
                tm_now->tm_min = mm;
                tm_now->tm_sec = ss;

                char buf[64];
                strftime(buf, sizeof(buf), "%d.%m.%Y %H:%M:%S", tm_now);

                // Nur Ausgabe: Zeit, Latitude, Longitude, Altitude
                printf("%s,%.6f,%.6f,%.2f\n", buf, latitude, longitude, altitude);
                break;
            }
        }
    }

    fclose(serial);
    return 0;
}

