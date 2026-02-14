import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/models.dart';
import '../services/api_service.dart';
import '../widgets/file_tile.dart';
import 'viewer_screen.dart';

class SearchScreen extends StatefulWidget {
  final String? repo;
  final String? currentPath;

  const SearchScreen({super.key, this.repo, this.currentPath});

  @override
  State<SearchScreen> createState() => _SearchScreenState();
}

class _SearchScreenState extends State<SearchScreen> {
  final _searchController = TextEditingController();
  SearchResult? _result;
  bool _searching = false;
  String? _error;
  bool _searchInCurrentDir = false;

  Future<void> _search() async {
    final query = _searchController.text.trim();
    if (query.isEmpty) return;

    setState(() {
      _searching = true;
      _error = null;
    });

    final api = context.read<ApiService>();
    try {
      final result = await api.search(
        query: query,
        repo: widget.repo,
        path: _searchInCurrentDir ? widget.currentPath : null,
      );
      setState(() {
        _result = result;
        _searching = false;
      });
    } catch (e) {
      setState(() {
        _searching = false;
        _error = 'Search failed: $e';
      });
    }
  }

  void _openFile(FileEntry file) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder:
            (_) => ViewerScreen(
              filePath: file.path,
              fileName: file.name,
              repo: widget.repo,
            ),
      ),
    );
  }

  @override
  void dispose() {
    _searchController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Search')),
      body: Column(
        children: [
          Padding(
            padding: const EdgeInsets.all(16),
            child: Column(
              children: [
                TextField(
                  controller: _searchController,
                  decoration: InputDecoration(
                    hintText: 'Search documents...',
                    border: const OutlineInputBorder(),
                    prefixIcon: const Icon(Icons.search),
                    suffixIcon: IconButton(
                      icon: const Icon(Icons.clear),
                      onPressed: () {
                        _searchController.clear();
                        setState(() => _result = null);
                      },
                    ),
                  ),
                  textInputAction: TextInputAction.search,
                  onSubmitted: (_) => _search(),
                  autofocus: true,
                ),
                if (widget.currentPath != null &&
                    widget.currentPath!.isNotEmpty)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: Row(
                      children: [
                        Checkbox(
                          value: _searchInCurrentDir,
                          onChanged:
                              (v) => setState(
                                () => _searchInCurrentDir = v ?? false,
                              ),
                        ),
                        Expanded(
                          child: Text(
                            'Search in ${widget.currentPath}',
                            style: Theme.of(context).textTheme.bodySmall,
                          ),
                        ),
                      ],
                    ),
                  ),
              ],
            ),
          ),
          if (_searching) const LinearProgressIndicator(),
          if (_error != null)
            Padding(
              padding: const EdgeInsets.all(16),
              child: Text(_error!, style: const TextStyle(color: Colors.red)),
            ),
          if (_result != null)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: Text(
                '${_result!.count} result${_result!.count == 1 ? '' : 's'}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          Expanded(
            child:
                _result == null
                    ? const SizedBox()
                    : _result!.results.isEmpty
                    ? const Center(child: Text('No results found'))
                    : ListView.builder(
                      itemCount: _result!.results.length,
                      itemBuilder: (_, i) {
                        final file = _result!.results[i];
                        return FileTile(
                          file: file,
                          repo: widget.repo,
                          showFullPath: true,
                          onTap: () => _openFile(file),
                        );
                      },
                    ),
          ),
        ],
      ),
    );
  }
}
