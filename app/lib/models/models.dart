class Repository {
  final String path;
  final String name;
  final bool exists;

  Repository({required this.path, required this.name, required this.exists});

  factory Repository.fromJson(Map<String, dynamic> json) {
    return Repository(
      path: json['path'] as String,
      name: json['name'] as String,
      exists: json['exists'] as bool,
    );
  }
}

class DirectoryEntry {
  final String name;
  final String path;
  final int count;

  DirectoryEntry({
    required this.name,
    required this.path,
    required this.count,
  });

  factory DirectoryEntry.fromJson(Map<String, dynamic> json) {
    return DirectoryEntry(
      name: json['name'] as String,
      path: json['path'] as String,
      count: json['count'] as int,
    );
  }
}

class FileEntry {
  final String name;
  final String path;
  final int size;
  final String modified;

  FileEntry({
    required this.name,
    required this.path,
    required this.size,
    required this.modified,
  });

  factory FileEntry.fromJson(Map<String, dynamic> json) {
    return FileEntry(
      name: json['name'] as String,
      path: json['path'] as String,
      size: json['size'] as int,
      modified: json['modified'] as String,
    );
  }
}

class BrowseResult {
  final String path;
  final List<DirectoryEntry> directories;
  final List<FileEntry> files;

  BrowseResult({
    required this.path,
    required this.directories,
    required this.files,
  });

  factory BrowseResult.fromJson(Map<String, dynamic> json) {
    return BrowseResult(
      path: json['path'] as String? ?? '',
      directories:
          (json['directories'] as List<dynamic>?)
              ?.map(
                (d) => DirectoryEntry.fromJson(d as Map<String, dynamic>),
              )
              .toList() ??
          [],
      files:
          (json['files'] as List<dynamic>?)
              ?.map((f) => FileEntry.fromJson(f as Map<String, dynamic>))
              .toList() ??
          [],
    );
  }
}

class SearchResult {
  final int count;
  final List<FileEntry> results;

  SearchResult({required this.count, required this.results});

  factory SearchResult.fromJson(Map<String, dynamic> json) {
    return SearchResult(
      count: json['count'] as int,
      results:
          (json['results'] as List<dynamic>)
              .map((r) => FileEntry.fromJson(r as Map<String, dynamic>))
              .toList(),
    );
  }
}

/// A stack found by searching the text on its pages
class TextHit {
  /// the stack, relative to the repository
  final String path;

  /// the page which best matches, from 1
  final int page;

  /// the text around the words, with each word in <b>...</b>
  final String snippet;

  TextHit({required this.path, required this.page, required this.snippet});

  String get name => path.split('/').last;

  factory TextHit.fromJson(Map<String, dynamic> json) {
    return TextHit(
      path: json['path'] as String,
      page: json['page'] as int? ?? 1,
      snippet: json['snippet'] as String? ?? '',
    );
  }
}

class TextSearchResult {
  final List<TextHit> hits;

  /// false while the server is still building its index, so some stacks
  /// may be missing
  final bool complete;

  TextSearchResult({required this.hits, this.complete = true});

  factory TextSearchResult.fromJson(Map<String, dynamic> json) {
    return TextSearchResult(
      hits: (json['results'] as List<dynamic>? ?? [])
          .map((r) => TextHit.fromJson(r as Map<String, dynamic>))
          .toList(),
      complete: json['complete'] as bool? ?? true,
    );
  }
}
