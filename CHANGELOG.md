# Changelog

## 1.2.5 (development candidate)

- Fixed disabled content selection, loss of disabled entries, non-BMP Unicode, settings validation and failed saves reported as success.
- Preserved intentionally empty libraries and user files. Default dhikr content now has one source in `assets/content.md`.
- Fixed overlapping daily schedules, restored saved pause state and prevented stale preferences from replacing the live pause state.
- Fixed miniaudio retry deadlock and made Stop control the actual playback. Null backends are reported as unavailable.
- Added Qt weekly schedules, display controls and library import/export/disable controls. Fixed persisted language and quiet hours, long text scrolling, missing-tray exit and second-instance IPC.
- Replaced fatal Xlib error paths with checked XCB replies. AppImage startup registration uses the permanent APPIMAGE path.
- Added pinned Qt deployment packaging for Linux and a macOS universal/signing/notarization pipeline that fails on deployment errors.
- Added regression, audio, UI and executable smoke tests. Updated build, privacy and remaining-work documentation without resource-use or antivirus guarantees.

Linux validation was performed locally in Ubuntu VMs. Native Windows MSVC build and 23 core tests passed; unsigned MSIX packaging passed MakeAppx validation. macOS execution, installer tests and remote GitHub workflow execution remain pending. This entry does not record a published release.
