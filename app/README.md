# Paperman Mobile App

Flutter app for browsing and viewing documents on a paperman server.

## Features

- **Browse** directories with thumbnails, breadcrumb navigation and
  pull-to-refresh
- **Search** documents across the whole repository or within the current
  directory
- **View** documents as PDF (server converts .max, .jpg, .tiff on the fly)
- **Multiple repositories** with a switcher in the toolbar
- **HTTP Basic Auth** for servers behind nginx authentication
- **Dark mode** follows the system theme
- Credentials and server URL saved locally for auto-reconnect

## Building

Requires Flutter SDK (tested with 3.41.x) and the Android SDK.

```sh
cd app
flutter pub get
flutter build apk --debug
```

The APK is written to `build/app/outputs/flutter-apk/app-debug.apk`.

## Project structure

```
lib/
  main.dart                  Entry point, Provider setup, theme
  models/models.dart         Data classes (Repository, FileEntry, etc.)
  services/api_service.dart  REST client for all paperman endpoints
  screens/
    connection_screen.dart   Server URL + credentials input
    browse_screen.dart       Directory listing with breadcrumbs
    search_screen.dart       Full-text search
    viewer_screen.dart       PDF viewer with page navigation
  widgets/
    file_tile.dart           Thumbnail + filename list item
    directory_tile.dart      Folder list item
```

## Server API

The app talks to the following paperman server endpoints:

| Endpoint     | Purpose                              |
|--------------|--------------------------------------|
| `/status`    | Health check                         |
| `/repos`     | List repositories                    |
| `/browse`    | List directories and files           |
| `/search`    | Search by filename                   |
| `/file`      | Download or convert a file to PDF    |
| `/thumbnail` | Get a JPEG thumbnail for a file page |
