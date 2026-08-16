# Azkar Reminder

Azkar Reminder is a lightweight, beautifully designed Windows desktop application built with C++ and Direct2D. It helps you remember Allah during your workday by displaying unobtrusive, customizable dhikr notifications on a regular schedule.

![Azkar Reminder](assets/images/logo.png)

## Features
- **Unobtrusive Notifications**: Beautiful, smooth sliding popup notifications that don't steal window focus.
- **Customizable Schedule**: Set your own intervals, quiet hours, and start/end times.
- **Audio Alerts**: Configurable audio notifications using XAudio2 that bypass Windows background muting for reliable playback.
- **Embedded Dhikr Library**: Ships with a default set of authentic Adhkar, with support for custom JSON content libraries.
- **Rich User Interface**: Fluid animations and modern design using native Direct2D/DirectWrite rendering.
- **Lightweight**: Minimal memory footprint, entirely written in C++ using Win32 APIs with no heavy runtimes like Electron.
- **History & Analytics**: Local SQLite database to keep track of your dhikr streaks and history locally on your machine.

## Installation
The official user distribution for Azkar Reminder is the standard Windows Installer. Download the latest setup executable from the [Releases](../../releases) page:

1. Download `AzkarReminder-Setup-v1.1.0.exe`
2. Run the installer wizard and choose your preferences (e.g. desktop shortcut).
3. The app will install to `C:\Program Files\Azkar Reminder` and register in the Windows Start Menu and Installed Apps.
4. Launch **Azkar Reminder** from the Start Menu or desktop shortcut.

### System Requirements
- **OS**: Windows 10 (Version 1809 or newer) or Windows 11.
- **Architecture**: x64

## Building from Source
If you wish to compile the application and build the installer yourself, please refer to the detailed instructions in [docs/BUILD.md](docs/BUILD.md).

## Privacy
Azkar Reminder is 100% offline. It does not transmit any of your personal data, dhikr history, or preferences over the internet. All data is saved in your local `%LOCALAPPDATA%` directory.

## License
This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Third-Party Libraries
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for information regarding the open-source libraries used in this project.
