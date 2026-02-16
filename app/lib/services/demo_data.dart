import 'dart:typed_data';
import 'package:flutter/services.dart' show rootBundle;
import '../models/models.dart';

/// Hardcoded demo catalogue used when the app runs without a server.
class DemoData {
  DemoData._();

  static const _repoName = 'Sample Documents';

  static const _thumbnails = {
    'Reports/Annual Report.pdf': 'assets/demo/thumb_report.jpg',
    'Reports/Summary.pdf': 'assets/demo/thumb_summary.jpg',
    'Photos/Image Document.pdf': 'assets/demo/thumb_image.jpg',
  };

  // Asset paths keyed by virtual file path
  static const _assets = {
    'Reports/Annual Report.pdf': 'assets/demo/long_report.pdf',
    'Reports/Summary.pdf': 'assets/demo/sample.pdf',
    'Photos/Image Document.pdf': 'assets/demo/image_doc.pdf',
  };

  static const _sizes = {
    'Reports/Annual Report.pdf': 204800,
    'Reports/Summary.pdf': 37888,
    'Photos/Image Document.pdf': 51200,
  };

  static const _pageCounts = {
    'Reports/Annual Report.pdf': 100,
    'Reports/Summary.pdf': 5,
    'Photos/Image Document.pdf': 2,
  };

  static List<Repository> getRepos() {
    return [
      Repository(path: '/demo', name: _repoName, exists: true),
    ];
  }

  static BrowseResult browse({String? path}) {
    if (path == null || path.isEmpty) {
      return BrowseResult(
        path: '',
        directories: [
          DirectoryEntry(name: 'Reports', path: 'Reports', count: 2),
          DirectoryEntry(name: 'Photos', path: 'Photos', count: 1),
        ],
        files: [],
      );
    }

    final files = <FileEntry>[];
    for (final entry in _assets.keys) {
      if (entry.startsWith('$path/')) {
        final name = entry.substring(path.length + 1);
        if (!name.contains('/')) {
          files.add(FileEntry(
            name: name,
            path: entry,
            size: _sizes[entry] ?? 0,
            modified: '2025-01-15T10:30:00Z',
          ));
        }
      }
    }

    return BrowseResult(path: path, directories: [], files: files);
  }

  static SearchResult search({required String query}) {
    final lower = query.toLowerCase();
    final matches = <FileEntry>[];
    for (final entry in _assets.keys) {
      final name = entry.split('/').last;
      if (name.toLowerCase().contains(lower)) {
        matches.add(FileEntry(
          name: name,
          path: entry,
          size: _sizes[entry] ?? 0,
          modified: '2025-01-15T10:30:00Z',
        ));
      }
    }
    return SearchResult(count: matches.length, results: matches);
  }

  static String thumbnailAsset(String path) =>
      _thumbnails[path] ?? 'assets/demo/thumb_summary.jpg';

  static int getPageCount(String path) {
    return _pageCounts[path] ?? 1;
  }

  static Future<Uint8List> loadPageBytes(String path) async {
    final asset = _assets[path];
    if (asset == null) {
      throw Exception('Unknown demo file: $path');
    }
    final data = await rootBundle.load(asset);
    return data.buffer.asUint8List();
  }
}
