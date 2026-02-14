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
