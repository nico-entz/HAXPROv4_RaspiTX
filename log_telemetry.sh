#!/bin/bash

# Ziel-Datei
output_file="data/telemetry.csv"

# Header einmalig schreiben
if [ ! -f "$output_file" ]; then
  echo "Timestamp,GPS Time,Lat,Lon,Alt,Temp 1,Pressure,Temp 2,Humidity,Temp EXT" >> "$output_file"
fi

# Endlosschleife
while true; do
  timestamp=$(date "+%Y-%m-%d %H:%M:%S")

  gps=$(timeout 10 ./gps_test 2>/dev/null | tr -d '\n')
  bmp=$(timeout 10 ./bmp280 2>/dev/null | tr -d '\n')
  sht=$(timeout 10 ./sht3x 2>/dev/null | tr -d '\n')
  ds18=$(timeout 10 ./ds18b20.sh 2>/dev/null | tr -d '\n')

  echo "$timestamp,$gps,$bmp,$sht,$ds18" >> "$output_file"

  sleep 5
done