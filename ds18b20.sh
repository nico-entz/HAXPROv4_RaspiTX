#!/bin/bash

# Pfad zum 1-Wire-Geräteordner (kann variieren, hier Raspberry Pi Standard)
W1_DEVICES_DIR="/sys/bus/w1/devices"

# Suche nach dem Ordner des DS18B20 Sensors (beginnt mit 28-)
sensor_folder=$(ls $W1_DEVICES_DIR | grep "^28-" | head -n1)

if [ -z "$sensor_folder" ]; then
  echo "error"
  exit 1
fi

sensor_path="$W1_DEVICES_DIR/$sensor_folder/w1_slave"

# Auslesen der Temperatur
if [ -f "$sensor_path" ]; then
  # Lese die Datei mit Sensordaten
  content=$(cat "$sensor_path")
  
  # Prüfe, ob CRC OK ist (erste Zeile endet mit "YES")
  crc_ok=$(echo "$content" | head -n1 | grep "YES")
  if [ -z "$crc_ok" ]; then
    echo "error"
    exit 1
  fi

  # Temperatur extrahieren (zweite Zeile, nach "t=" kommt Temperatur in Milligrad Celsius)
  temp_milli=$(echo "$content" | tail -n1 | sed -n 's/.*t=\([0-9\-]*\)$/\1/p')

  if [ -z "$temp_milli" ]; then
    echo "error"
    exit 1
  fi

  # Umrechnung in Grad Celsius
  temp_c=$(echo "scale=3; $temp_milli / 1000" | bc)

  echo "$temp_c"
else
  echo "error"
  exit 1
fi
