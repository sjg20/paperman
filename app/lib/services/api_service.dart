import 'dart:convert';
import 'dart:typed_data';
import 'package:http/http.dart' as http;
import '../models/models.dart';
import 'demo_data.dart';

class ApiService {
  String _baseUrl;
  String? _username;
  String? _password;

  ApiService({required String baseUrl, String? username, String? password})
    : _baseUrl = baseUrl.replaceAll(RegExp(r'/+$'), ''),
      _username = username,
      _password = password;

  bool _isDemo = false;
  bool get isDemo => _isDemo;

  void enableDemo() => _isDemo = true;
  void disableDemo() => _isDemo = false;

  String get baseUrl => _baseUrl;
  String? get username => _username;
  String? get password => _password;

  void updateConfig({
    required String baseUrl,
    String? username,
    String? password,
  }) {
    _baseUrl = baseUrl.replaceAll(RegExp(r'/+$'), '');
    _username = username;
    _password = password;
  }

  String? get basicAuth => _basicAuth;

  String? get _basicAuth {
    if (_username != null && _username!.isNotEmpty) {
      final credentials = base64Encode(
        utf8.encode('$_username:${_password ?? ''}'),
      );
      return 'Basic $credentials';
    }
    return null;
  }

  Map<String, String> get _headers {
    final headers = <String, String>{'Accept': 'application/json'};
    final auth = _basicAuth;
    if (auth != null) {
      headers['Authorization'] = auth;
    }
    return headers;
  }

  Future<Map<String, dynamic>> _getJson(
    String endpoint, [
    Map<String, String>? params,
  ]) async {
    final uri = Uri.parse('$_baseUrl$endpoint').replace(
      queryParameters: params?.isNotEmpty == true ? params : null,
    );
    final response = await http.get(uri, headers: _headers);
    if (response.statusCode == 401) {
      throw ApiException(401, 'Invalid username or password');
    }
    if (response.statusCode != 200) {
      try {
        final body = jsonDecode(response.body) as Map<String, dynamic>;
        throw ApiException(
          response.statusCode,
          body['error'] as String? ?? 'Request failed',
        );
      } catch (e) {
        if (e is ApiException) rethrow;
        throw ApiException(response.statusCode, 'Request failed');
      }
    }
    return jsonDecode(response.body) as Map<String, dynamic>;
  }

  /// Check server status. Returns normally on success, throws on failure.
  Future<void> checkStatus() async {
    if (_isDemo) return;
    final uri = Uri.parse('$_baseUrl/status');
    final http.Response response;
    try {
      response = await http.get(uri, headers: _headers);
    } catch (e) {
      throw ApiException(0, 'Cannot reach server: $e');
    }
    if (response.statusCode == 401) {
      throw ApiException(401, 'Invalid username or password');
    }
    if (response.statusCode != 200) {
      throw ApiException(
        response.statusCode,
        'Server error ${response.statusCode}',
      );
    }
    final json = jsonDecode(response.body) as Map<String, dynamic>;
    if (json['status'] != 'running') {
      throw ApiException(0, 'Server not ready');
    }
  }

  Future<List<Repository>> getRepos() async {
    if (_isDemo) return DemoData.getRepos();
    final json = await _getJson('/repos');
    return (json['repositories'] as List<dynamic>)
        .map((r) => Repository.fromJson(r as Map<String, dynamic>))
        .toList();
  }

  Future<BrowseResult> browse({String? path, String? repo}) async {
    if (_isDemo) return DemoData.browse(path: path);
    final params = <String, String>{};
    if (path != null && path.isNotEmpty) params['path'] = path;
    if (repo != null) params['repo'] = repo;
    final json = await _getJson('/browse', params);
    return BrowseResult.fromJson(json);
  }

  Future<SearchResult> search({
    required String query,
    String? repo,
    String? path,
  }) async {
    if (_isDemo) return DemoData.search(query: query);
    final params = <String, String>{'q': query};
    if (repo != null) params['repo'] = repo;
    if (path != null && path.isNotEmpty) params['path'] = path;
    final json = await _getJson('/search', params);
    return SearchResult.fromJson(json);
  }

  /// Build a plain URL (no embedded credentials). Auth is handled
  /// via headers instead.
  Uri _buildUri(String endpoint, Map<String, String> params) {
    return Uri.parse('$_baseUrl$endpoint').replace(
      queryParameters: params,
    );
  }

  Uri getFileUrl({required String path, String? repo, bool asPdf = true}) {
    final params = <String, String>{'path': path};
    if (repo != null) params['repo'] = repo;
    if (asPdf) params['type'] = 'pdf';
    return _buildUri('/file', params);
  }

  Uri getThumbnailUrl({
    required String path,
    String? repo,
    int page = 1,
    String size = 'medium',
  }) {
    if (_isDemo) return Uri.parse('asset://${DemoData.thumbnailAsset(path)}');
    final params = <String, String>{
      'path': path,
      'page': page.toString(),
      'size': size,
    };
    if (repo != null) params['repo'] = repo;
    return _buildUri('/thumbnail', params);
  }

  /// Get page count for a PDF file.
  Future<int> getPageCount({required String path, String? repo}) async {
    if (_isDemo) return DemoData.getPageCount(path);
    final params = <String, String>{'path': path, 'pages': 'true'};
    if (repo != null) params['repo'] = repo;
    final json = await _getJson('/file', params);
    return json['pages'] as int;
  }

  /// Download a single page of a PDF file.
  Future<http.Response> downloadFilePage({
    required String path,
    required int page,
    String? repo,
  }) async {
    if (_isDemo) {
      final bytes = await DemoData.loadPageBytes(path);
      return http.Response.bytes(bytes, 200);
    }
    final params = <String, String>{
      'path': path,
      'page': page.toString(),
    };
    if (repo != null) params['repo'] = repo;
    final uri = Uri.parse('$_baseUrl/file').replace(queryParameters: params);
    final response = await http.get(uri, headers: _headers);
    if (response.statusCode != 200) {
      throw ApiException(response.statusCode, 'Failed to download page');
    }
    return response;
  }

  /// Download a single page with streaming progress.
  Future<Uint8List> downloadFilePageStreamed({
    required String path,
    required int page,
    String? repo,
    void Function(int received, int total)? onProgress,
  }) async {
    if (_isDemo) return DemoData.loadPageBytes(path);
    final params = <String, String>{
      'path': path,
      'page': page.toString(),
    };
    if (repo != null) params['repo'] = repo;
    final uri = Uri.parse('$_baseUrl/file').replace(queryParameters: params);

    final request = http.Request('GET', uri);
    final auth = _basicAuth;
    if (auth != null) {
      request.headers['Authorization'] = auth;
    }

    final client = http.Client();
    try {
      final streamed = await client.send(request);
      if (streamed.statusCode != 200) {
        throw ApiException(streamed.statusCode, 'Failed to download page');
      }

      final total = streamed.contentLength ?? -1;
      final chunks = <List<int>>[];
      var received = 0;

      await for (final chunk in streamed.stream) {
        chunks.add(chunk);
        received += chunk.length;
        if (onProgress != null) {
          onProgress(received, total);
        }
      }

      final bytes = Uint8List(received);
      var offset = 0;
      for (final chunk in chunks) {
        bytes.setRange(offset, offset + chunk.length, chunk);
        offset += chunk.length;
      }
      return bytes;
    } finally {
      client.close();
    }
  }

  /// Download a file and return its bytes.
  Future<http.Response> downloadFile({
    required String path,
    String? repo,
    bool asPdf = true,
  }) async {
    if (_isDemo) {
      final bytes = await DemoData.loadPageBytes(path);
      return http.Response.bytes(bytes, 200);
    }
    final params = <String, String>{'path': path};
    if (repo != null) params['repo'] = repo;
    if (asPdf) params['type'] = 'pdf';
    final uri = Uri.parse('$_baseUrl/file').replace(queryParameters: params);
    final response = await http.get(uri, headers: _headers);
    if (response.statusCode != 200) {
      throw ApiException(response.statusCode, 'Failed to download file');
    }
    return response;
  }

  /// Download a complete file with streaming progress.
  Future<Uint8List> downloadFileStreamed({
    required String path,
    String? repo,
    bool asPdf = true,
    void Function(int received, int total)? onProgress,
  }) async {
    if (_isDemo) return DemoData.loadPageBytes(path);
    final params = <String, String>{'path': path};
    if (repo != null) params['repo'] = repo;
    if (asPdf) params['type'] = 'pdf';
    final uri =
        Uri.parse('$_baseUrl/file').replace(queryParameters: params);

    final request = http.Request('GET', uri);
    final auth = _basicAuth;
    if (auth != null) {
      request.headers['Authorization'] = auth;
    }

    final client = http.Client();
    try {
      final streamed = await client.send(request);
      if (streamed.statusCode != 200) {
        throw ApiException(
            streamed.statusCode, 'Failed to download file');
      }

      final total = streamed.contentLength ?? -1;
      final chunks = <List<int>>[];
      var received = 0;

      await for (final chunk in streamed.stream) {
        chunks.add(chunk);
        received += chunk.length;
        if (onProgress != null) {
          onProgress(received, total);
        }
      }

      final bytes = Uint8List(received);
      var offset = 0;
      for (final chunk in chunks) {
        bytes.setRange(offset, offset + chunk.length, chunk);
        offset += chunk.length;
      }
      return bytes;
    } finally {
      client.close();
    }
  }
}

class ApiException implements Exception {
  final int statusCode;
  final String message;

  ApiException(this.statusCode, this.message);

  @override
  String toString() => 'ApiException($statusCode): $message';
}
