# FlexOutput Plugin for OBS Studio

A flexible output management plugin for OBS Studio that provides advanced output configuration and source mapping capabilities.

## Features

- **Flexible Output Management**: Create and manage multiple custom outputs with different configurations
- **Advanced Source Mapping**: Map specific sources to different outputs with intelligent filtering
- **Real-time Configuration**: Dynamic configuration changes without restarting OBS
- **JSON Import/Export**: Save and load configurations for easy backup and sharing
- **Comprehensive UI**: User-friendly interface for managing outputs and mappings
- **Audio/Video Processing**: Independent audio and video processing pipelines
- **Performance Monitoring**: Built-in statistics and performance tracking

## Installation

### Prerequisites

- OBS Studio 31.1.1 or later
- CMake 3.28 or later
- Qt6 (for UI components)

### Build from Source

1. Clone the repository:
```bash
git clone https://github.com/passwind/obs-flexoutput.git
cd obs-flexoutput
```

2. Configure the build:
```bash
cmake --preset macos  # For macOS
# or
cmake --preset windows-x64  # For Windows
# or
cmake --preset ubuntu-x86_64  # For Ubuntu
```

3. Build the plugin:
```bash
cmake --build --preset macos  # For macOS
# or
cmake --build --preset windows-x64  # For Windows
# or
cmake --build --preset ubuntu-x86_64  # For Ubuntu
```

4. Install the plugin to your OBS Studio plugins directory.

## Usage

### Basic Setup

1. Open OBS Studio
2. Go to **Tools** → **FlexOutput Settings**
3. Click **Add Output** to create a new output configuration
4. Configure your output settings (encoder, resolution, bitrate, etc.)
5. Set up source mappings to specify which sources go to which outputs

### Output Configuration

Each output can be configured with:

- **Name**: Unique identifier for the output
- **Encoder Settings**: Video codec, bitrate, resolution
- **Audio Settings**: Audio codec, sample rate, channels
- **Network Settings**: Streaming URL, authentication
- **Advanced Options**: Custom encoder parameters

### Source Mapping

Source mapping allows you to:

- Map specific sources to specific outputs
- Filter sources by type (e.g., only cameras, only displays)
- Set up automatic source detection and mapping
- Configure fallback sources for reliability

### Configuration Management

- **Export Configuration**: Save your current setup to a JSON file
- **Import Configuration**: Load a previously saved configuration
- **Reset to Defaults**: Restore factory settings

## API Reference

### Core Functions

#### Output Management
```c
// Create a new output
flexoutput_error_t flexoutput_create_output(const char *name, const flexoutput_output_config_t *config);

// Destroy an output
flexoutput_error_t flexoutput_destroy_output(const char *name);

// Start/stop outputs
flexoutput_error_t flexoutput_output_start(flexoutput_output_instance_t *instance);
flexoutput_error_t flexoutput_output_stop(flexoutput_output_instance_t *instance);
```

#### Configuration Management
```c
// Initialize configuration system
flexoutput_error_t flexoutput_config_init(void);

// Export/import configurations
char *flexoutput_config_export_json(void);
flexoutput_error_t flexoutput_config_import_json(const char *json_str);
```

#### Source Mapping
```c
// Add/remove mapping rules
flexoutput_error_t flexoutput_mapping_add_rule(const flexoutput_mapping_rule_t *rule);
flexoutput_error_t flexoutput_mapping_remove_rule(const char *rule_id);

// Update mappings
flexoutput_error_t flexoutput_mapping_update(void);
```

### Data Structures

#### Output Configuration
```c
typedef struct {
    char *name;
    char *encoder_id;
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;
    uint32_t bitrate;
    // ... additional fields
} flexoutput_output_config_t;
```

#### Mapping Rule
```c
typedef struct {
    char *rule_id;
    char *output_name;
    flexoutput_mapping_type_t type;
    bool enabled;
    bool auto_add_new;
    bool auto_remove_missing;
    // ... additional fields
} flexoutput_mapping_rule_t;
```

## Configuration File Format

The plugin uses JSON format for configuration files:

```json
{
    "version": "1.0.0",
    "outputs": [
        {
            "name": "Main Stream",
            "encoder": "obs_x264",
            "width": 1920,
            "height": 1080,
            "fps_num": 30,
            "fps_den": 1,
            "bitrate": 5000,
            "audio_encoder": "ffmpeg_aac",
            "audio_bitrate": 128
        }
    ],
    "mappings": [
        {
            "rule_id": "main_sources",
            "output_name": "Main Stream",
            "type": "source_list",
            "sources": ["Camera", "Desktop"],
            "enabled": true
        }
    ]
}
```

## Development

### Project Structure

```
src/
├── plugin-main.c           # Main plugin entry point
├── flexoutput-types.h      # Core data structures
├── flexoutput-config.c/h   # Configuration management
├── flexoutput-output.c/h   # Output management
├── flexoutput-source.c/h   # Source handling
├── flexoutput-mapping.c/h  # Source mapping logic
├── flexoutput-audio.c/h    # Audio processing
├── flexoutput-video.c/h    # Video processing
├── flexoutput-ui.cpp/h     # Main UI components
└── flexoutput-ui-widgets.cpp # UI widgets
```

### Building for Development

1. Enable development options:
```bash
cmake --preset macos-ci  # Includes warnings as errors and debug symbols
```

2. Run tests:
```bash
ctest --preset macos
```

3. Format code:
```bash
./build-aux/.run-format.zsh
```

### Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/amazing-feature`
3. Make your changes and ensure tests pass
4. Follow the existing code style (use `.clang-format`)
5. Commit your changes: `git commit -m 'Add amazing feature'`
6. Push to the branch: `git push origin feature/amazing-feature`
7. Open a Pull Request

## Troubleshooting

### Common Issues

**Plugin not loading**
- Ensure OBS Studio version compatibility
- Check that all dependencies are installed
- Verify plugin is in the correct directory

**Configuration not saving**
- Check file permissions in OBS config directory
- Ensure JSON format is valid
- Try resetting to default configuration

**Performance issues**
- Monitor CPU/GPU usage in OBS
- Reduce output resolution or bitrate
- Check encoder settings and hardware acceleration

**Source mapping not working**
- Verify source names match exactly
- Check mapping rule configuration
- Ensure sources are active and visible

### Debug Logging

Enable debug logging by setting the log level:

```c
// In your OBS log file, look for FlexOutput messages
FLEXOUTPUT_LOG_DEBUG("Debug message");
FLEXOUTPUT_LOG_INFO("Info message");
FLEXOUTPUT_LOG_WARNING("Warning message");
FLEXOUTPUT_LOG_ERROR("Error message");
```

## License

This project is licensed under the GNU General Public License v2.0 - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Built on the [OBS Plugin Template](https://github.com/obsproject/obs-plugintemplate)
- Uses OBS Studio's powerful plugin architecture
- Thanks to the OBS Studio community for feedback and testing

## Support

- **Issues**: Report bugs and feature requests on [GitHub Issues](https://github.com/passwind/obs-flexoutput/issues)
- **Documentation**: Check the [Wiki](https://github.com/passwind/obs-flexoutput/wiki) for detailed guides
- **Community**: Join discussions in the [OBS Studio Discord](https://discord.gg/obsproject)

## Changelog

### Version 1.0.0
- Initial release
- Basic output management functionality
- Source mapping system
- Configuration import/export
- Qt-based user interface
- Cross-platform support (Windows, macOS, Linux)
