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
  TextSearchResult? _textResult;
  bool _searching = false;
  String? _error;
  bool _searchInCurrentDir = false;

  /// look for words on the pages rather than in the names; this needs a
  /// repository, whose server keeps an index of its text
  bool _byText = false;

  Future<void> _search() async {
    final query = _searchController.text.trim();
    if (query.isEmpty) return;

    setState(() {
      _searching = true;
      _error = null;
    });

    final api = context.read<ApiService>();
    final path = _searchInCurrentDir ? widget.currentPath : null;
    try {
      if (_byText && widget.repo != null) {
        final result = await api.searchText(
          repo: widget.repo!,
          text: query,
          path: path,
        );
        setState(() {
          _textResult = result;
          _result = null;
          _searching = false;
        });
        return;
      }
      final result = await api.search(
        query: query,
        repo: widget.repo,
        path: path,
      );
      setState(() {
        _result = result;
        _textResult = null;
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

  // a stack found by its text opens at the page which matched
  void _openHit(TextHit hit) {
    Navigator.of(context).push(
      MaterialPageRoute(
        builder:
            (_) => ViewerScreen(
              filePath: hit.path,
              fileName: hit.name,
              repo: widget.repo,
              initialPage: hit.page,
            ),
      ),
    );
  }

  Widget _buildTextResults(BuildContext context) {
    final result = _textResult!;
    if (result.hits.isEmpty) {
      return const Center(child: Text('No pages found with those words'));
    }
    final small = Theme.of(context).textTheme.bodySmall;
    return ListView.builder(
      itemCount: result.hits.length,
      itemBuilder: (_, i) {
        final hit = result.hits[i];
        final dir = hit.path.contains('/')
            ? hit.path.substring(0, hit.path.lastIndexOf('/'))
            : '';
        return ListTile(
          leading: const Icon(Icons.article_outlined),
          title: Text(hit.name),
          subtitle: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(dir.isEmpty ? 'Page ${hit.page}'
                               : '$dir · page ${hit.page}',
                   style: small),
              if (hit.snippet.isNotEmpty)
                Text.rich(SearchScreenSnippet.span(hit.snippet, small),
                          maxLines: 3, overflow: TextOverflow.ellipsis),
            ],
          ),
          onTap: () => _openHit(hit),
        );
      },
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
                if (widget.repo != null)
                  Padding(
                    padding: const EdgeInsets.only(top: 8),
                    child: SegmentedButton<bool>(
                      segments: const [
                        ButtonSegment(value: false, label: Text('Names')),
                        ButtonSegment(
                          value: true,
                          label: Text('Text on the pages'),
                        ),
                      ],
                      selected: {_byText},
                      onSelectionChanged: (s) => setState(() {
                        _byText = s.first;
                        _result = null;
                        _textResult = null;
                      }),
                    ),
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
          if (_textResult != null)
            Padding(
              padding: const EdgeInsets.symmetric(horizontal: 16),
              child: Text(
                '${_textResult!.hits.length} '
                'stack${_textResult!.hits.length == 1 ? '' : 's'}'
                '${_textResult!.complete ? '' : ' (the server is still '
                    'reading its stacks, so some may be missing)'}',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ),
          Expanded(
            child:
                _textResult != null
                    ? _buildTextResults(context)
                    : _result == null
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

/// The server's snippet of the text around the words a search found
class SearchScreenSnippet {
  /// the snippet, with the words it found in bold
  static TextSpan span(String snippet, TextStyle? style) {
    final spans = <TextSpan>[];
    final parts = snippet.split(RegExp(r'</?b>'));
    // the parts alternate between plain text and a word found
    for (var i = 0; i < parts.length; i++) {
      if (parts[i].isEmpty) continue;
      spans.add(TextSpan(
        text: parts[i],
        style: i.isOdd ? const TextStyle(fontWeight: FontWeight.bold) : null,
      ));
    }
    return TextSpan(style: style, children: spans);
  }
}
