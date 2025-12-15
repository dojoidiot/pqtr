# POST

Distribution platform plugins for delivering processed images to clients and platforms.

## Overview

POST handles the final delivery step of the PQTR pipeline. After an image is processed through HEAD → BODY → TAIL, POST delivers it to the configured destination(s). Each platform has its own plugin that handles authentication, format requirements, and API communication.

## Supported Platforms

| Plugin | Platform | Status |
|--------|----------|--------|
| `instagram` | Instagram | Planned |
| `email` | Email (SMTP/API) | Planned |
| `sms` | SMS/MMS | Planned |
| `dropbox` | Dropbox | Planned |
| `gdrive` | Google Drive | Planned |
| `s3` | AWS S3 | Planned |
| `ftp` | FTP/SFTP | Planned |
| `webhook` | Custom webhook | Planned |

## Plugin Interface

Each plugin implements a common interface:

```cpp
struct PostPlugin {
    // Plugin metadata
    const char* name;
    const char* version;

    // Initialize with credentials/config
    bool (*init)(const char* config_json);

    // Deliver image to platform
    bool (*post)(const char* image_path, const char* metadata_json);

    // Check delivery status
    int (*status)(const char* delivery_id);

    // Cleanup
    void (*shutdown)();
};
```

## Configuration

Each plugin stores credentials and settings in `~/.pqtr/post/`:

```
~/.pqtr/post/
├── instagram.json    # Instagram API credentials
├── email.json        # SMTP/Mailgun settings
├── sms.json          # Twilio/SMS gateway config
└── ...
```

## Usage

From the pipeline:
```cpp
// After TAIL produces final image
PostConfig config;
config.plugin = "instagram";
config.image_path = "output.png";
config.caption = "Shot on Sony A7IV";
config.tags = {"photography", "portrait"};

post_deliver(&config);
```

## Building

```bash
cd POST
make        # Build all plugins
make test   # Test plugin interfaces
```

## Project Structure

```
POST/
├── src/
│   ├── main/
│   │   ├── post.cpp       # Core delivery engine
│   │   └── part/
│   │       ├── instagram/ # Instagram plugin
│   │       ├── email/     # Email plugin
│   │       ├── sms/       # SMS plugin
│   │       └── ...
│   └── test/
├── inc/
│   └── post.hpp           # Plugin interface
├── lib/
├── bin/
└── README.md
```
