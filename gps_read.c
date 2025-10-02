#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <gps.h>
#include <errno.h>
#include <math.h>
#include <time.h>

int main(int argc, char *argv[]) {
    int verbose = 0;
    if (argc > 1 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--verbose") == 0)) {
        verbose = 1;
    }

    struct gps_data_t gps_data;
    if (gps_open("localhost", "2947", &gps_data) != 0) {
        fprintf(stderr, "Fehler beim Verbinden mit gpsd: %s\n", gps_errstr(errno));
        return 1;
    }

    gps_stream(&gps_data, WATCH_ENABLE | WATCH_JSON, NULL);

    for (int i = 0; i < 10; ++i) {
        if (gps_waiting(&gps_data, 2000000)) {
            if (gps_read(&gps_data, NULL, 0) == -1) {
                if (verbose)
                    fprintf(stderr, "Fehler beim Lesen der GPS-Daten.\n");
            } else {
                if ((gps_data.fix.mode >= MODE_3D) &&
                    !isnan(gps_data.fix.latitude) &&
                    !isnan(gps_data.fix.longitude) &&
                    !isnan(gps_data.fix.altitude)) {

                    time_t raw = (time_t)gps_data.fix.time.tv_sec;
                    struct tm *ptm = gmtime(&raw);
                    char iso_time[64];
                    strftime(iso_time, sizeof(iso_time), "%Y-%m-%dT%H:%M:%SZ", ptm);

                    printf("%s,%.6f,%.6f,%.2f\n",
                        iso_time,
                        gps_data.fix.latitude,
                        gps_data.fix.longitude,
                        gps_data.fix.altitude);
                    break;
                } else {
                    if (verbose)
                        printf("GPS-Daten unvollständig (noch kein 3D-Fix oder Altitude fehlt)\n");
                }
            }
        } else {
            if (verbose)
                printf("Timeout beim Warten auf GPS-Daten\n");
        }
        sleep(1);
    }

    gps_stream(&gps_data, WATCH_DISABLE, NULL);
    gps_close(&gps_data);
    return 0;
}
