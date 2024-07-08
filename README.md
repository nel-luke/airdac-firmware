# ![AirDAC Logo](logo.png) AirDAC
_A Wireless Ditigal-to-Analogue audio Converter_

This ESP32-based device supports the uPnP 2.0 standard and allows playback of WAV and FLAC files over a local area network.

# Building
This project is built using CMake and ESP-IDF v5.3.

In your terminal, run:
```
git clone https://github.com/nel-luke/airdac-firmware.git
cd airdac-firmware
idf.py build flash monitor
```

# License
This project is licensed under [LGPL3](https://opensource.org/licenses/lgpl-3.0.html).
