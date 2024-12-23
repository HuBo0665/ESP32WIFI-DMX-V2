ESP32_ArtNetNode/
├── platformio.ini
├── src/
│   ├── main.cpp
│   ├── ConfigManager.cpp
│   ├── config.h
│   ├── ConfigManager.h
│   ├── GlobalConfig.h
│   ├── dmx/
│   │   ├── ESP32DMX.h
│   │   └── ESP32DMX.cpp
│   ├── artnet/
│   │   ├── ArtnetNode.h
│   │   └── ArtnetNode.cpp
│   ├── rdm/
│   │   ├── RDMHandler.h
│   │   └── RDMHandler.cpp
│   ├── pixels/
│   │   ├── PixelDriver.h
│   │   └── PixelDriver.cpp
│   └── web/
│       ├── WebServer.h
│       └── WebServer.cpp
└── data/
    └── web/
        ├── index.html
        ├── style.css
        └── script.js