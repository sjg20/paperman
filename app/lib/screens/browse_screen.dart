import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';
import '../models/models.dart';
import '../services/api_service.dart';
import '../widgets/directory_tile.dart';
import '../widgets/file_tile.dart';
import 'connection_screen.dart';
import 'search_screen.dart';
import 'viewer_screen.dart';

class BrowseScreen extends StatefulWidget {
  const BrowseScreen({super.key});

  @override
  State<BrowseScreen> createState() => _BrowseScreenState();
}

class _BrowseScreenState extends State<BrowseScreen> {
  static const _buildDate = String.fromEnvironment(
    'BUILD_DATE',
    defaultValue: 'unknown',
  );

  List<Repository>? _repos;
  String? _selectedRepo;
  String _currentPath = '';
  BrowseResult? _browseResult;
  bool _loading = true;
  String? _error;

  @override
  void initState() {
    super.initState();
    _loadRepos();
  }

  Future<void> _loadRepos() async {
    final api = context.read<ApiService>();
    try {
      final repos = await api.getRepos();
      setState(() {
        _repos = repos;
        if (repos.isNotEmpty) {
          _selectedRepo = repos.first.name;
        }
      });
      _browse();
    } catch (e) {
      setState(() {
        _loading = false;
        _error = 'Failed to load repositories: $e';
      });
    }
  }

  Future<void> _browse([String? path]) async {
    setState(() {
      _loading = true;
      _error = null;
      if (path != null) _currentPath = path;
    });

    final api = context.read<ApiService>();
    try {
      final result = await api.browse(
        path: _currentPath.isEmpty ? null : _currentPath,
        repo: _selectedRepo,
      );
      setState(() {
        _browseResult = result;
        _loading = false;
      });
    } catch (e) {
      setState(() {
        _loading = false;
        _error = 'Failed to browse: $e';
      });
    }
  }

  List<String> get _breadcrumbs {
    if (_currentPath.isEmpty) return ['Root'];
    return ['Root', ..._currentPath.split('/')];
  }

  void _navigateToDirectory(String path) {
    _browse(path);
  }

  void _navigateUp() {
    if (_currentPath.isEmpty) return;
    final parts = _currentPath.split('/');
    parts.removeLast();
    _browse(parts.join('/'));
  }

  void _openFile(FileEntry file) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder:
            (_) => ViewerScreen(
              filePath: file.path,
              fileName: file.name,
              repo: _selectedRepo,
            ),
      ),
    );
  }

  void _disconnect() {
    context.read<ApiService>().disableDemo();
    Navigator.of(context).pushReplacement(
      MaterialPageRoute(
        builder: (_) => const ConnectionScreen(autoConnect: false),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(_selectedRepo ?? 'Paperman'),
        actions: [
          IconButton(
            icon: const Icon(Icons.search),
            onPressed: () {
              Navigator.of(context).push(
                MaterialPageRoute(
                  builder:
                      (_) => SearchScreen(
                        repo: _selectedRepo,
                        currentPath: _currentPath,
                      ),
                ),
              );
            },
          ),
          if (_repos != null && _repos!.length > 1)
            PopupMenuButton<String>(
              icon: const Icon(Icons.folder_special),
              onSelected: (repo) {
                setState(() => _selectedRepo = repo);
                _browse('');
              },
              itemBuilder:
                  (_) =>
                      _repos!
                          .map(
                            (r) => PopupMenuItem(
                              value: r.name,
                              child: Text(r.name),
                            ),
                          )
                          .toList(),
            ),
          IconButton(
            icon: const Icon(Icons.info_outline),
            onPressed: () async {
              final info = await PackageInfo.fromPlatform();
              if (!context.mounted) return;
              showAboutDialog(
                context: context,
                applicationName: 'Paperman',
                applicationVersion: info.version,
                applicationIcon: const Icon(
                  Icons.description,
                  size: 48,
                  color: Colors.blue,
                ),
                children: [
                  const Text('Paper scanning and access'),
                  const SizedBox(height: 8),
                  GestureDetector(
                    onTap: () => launchUrl(
                      Uri.parse('https://paperman.readthedocs.io'),
                      mode: LaunchMode.externalApplication,
                    ),
                    child: Text(
                      'https://paperman.readthedocs.io',
                      style: TextStyle(
                        color: Theme.of(context).colorScheme.primary,
                        decoration: TextDecoration.underline,
                      ),
                    ),
                  ),
                  const SizedBox(height: 8),
                  Text('Built: $_buildDate'),
                ],
              );
            },
          ),
          IconButton(
            icon: const Icon(Icons.logout),
            onPressed: _disconnect,
          ),
        ],
      ),
      body: Column(
        children: [
          // Breadcrumbs
          SizedBox(
            height: 48,
            child: ListView.separated(
              scrollDirection: Axis.horizontal,
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
              itemCount: _breadcrumbs.length,
              separatorBuilder: (_, _) => const Icon(Icons.chevron_right, size: 18),
              itemBuilder: (_, i) {
                return GestureDetector(
                  onTap: () {
                    if (i == 0) {
                      _browse('');
                    } else {
                      final path = _breadcrumbs
                          .sublist(1, i + 1)
                          .join('/');
                      _browse(path);
                    }
                  },
                  child: Text(
                    _breadcrumbs[i],
                    style: TextStyle(
                      color:
                          i == _breadcrumbs.length - 1
                              ? Theme.of(context).colorScheme.primary
                              : null,
                      fontWeight:
                          i == _breadcrumbs.length - 1
                              ? FontWeight.bold
                              : null,
                    ),
                  ),
                );
              },
            ),
          ),
          const Divider(height: 1),
          // Content
          Expanded(child: _buildContent()),
        ],
      ),
    );
  }

  Widget _buildContent() {
    if (_loading) {
      return const Center(child: CircularProgressIndicator());
    }

    if (_error != null) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              Text(_error!, style: const TextStyle(color: Colors.red)),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: () => _browse(),
                child: const Text('Retry'),
              ),
            ],
          ),
        ),
      );
    }

    final result = _browseResult;
    if (result == null) return const SizedBox();

    final dirs = result.directories;
    final files = result.files;

    if (dirs.isEmpty && files.isEmpty) {
      return const Center(child: Text('Empty directory'));
    }

    return RefreshIndicator(
      onRefresh: () => _browse(),
      child: ListView(
        children: [
          if (_currentPath.isNotEmpty)
            ListTile(
              leading: const Icon(Icons.arrow_upward),
              title: const Text('..'),
              onTap: _navigateUp,
            ),
          ...dirs.map(
            (d) => DirectoryTile(
              directory: d,
              onTap: () => _navigateToDirectory(d.path),
            ),
          ),
          ...files.map(
            (f) => FileTile(
              file: f,
              repo: _selectedRepo,
              onTap: () => _openFile(f),
            ),
          ),
        ],
      ),
    );
  }
}
