import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_pdfview/flutter_pdfview.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';

class ViewerScreen extends StatefulWidget {
  final String filePath;
  final String fileName;
  final String? repo;

  const ViewerScreen({
    super.key,
    required this.filePath,
    required this.fileName,
    this.repo,
  });

  @override
  State<ViewerScreen> createState() => _ViewerScreenState();
}

class _ViewerScreenState extends State<ViewerScreen> {
  int _totalPages = 0;
  int _currentPage = 1;
  final Map<int, String> _pageFiles = {};
  final Set<int> _fetching = {};
  bool _loading = true;
  String? _error;
  late String _safeName;
  late Directory _cacheDir;
  late PageController _pageController;

  @override
  void initState() {
    super.initState();
    _pageController = PageController();
    _init();
  }

  @override
  void dispose() {
    _pageController.dispose();
    super.dispose();
  }

  Future<void> _init() async {
    _safeName = widget.fileName.replaceAll(RegExp(r'[^\w.]'), '_');
    _cacheDir = await getTemporaryDirectory();

    final api = context.read<ApiService>();

    try {
      final pageCount = await api.getPageCount(
        path: widget.filePath,
        repo: widget.repo,
      );

      if (!mounted) return;
      setState(() {
        _totalPages = pageCount;
        _loading = false;
      });

      // Fetch page 1 immediately
      _fetchPage(1);
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = 'Failed to load document: $e';
      });
    }
  }

  Future<void> _fetchPage(int page) async {
    if (_pageFiles.containsKey(page) || _fetching.contains(page)) return;
    if (page < 1 || page > _totalPages) return;

    _fetching.add(page);

    final api = context.read<ApiService>();

    try {
      final response = await api.downloadFilePage(
        path: widget.filePath,
        page: page,
        repo: widget.repo,
      );

      final file = File(
        '${_cacheDir.path}/paperman_${_safeName}_p$page.pdf',
      );
      await file.writeAsBytes(response.bodyBytes);

      if (!mounted) return;
      setState(() {
        _pageFiles[page] = file.path;
        _fetching.remove(page);
      });
    } catch (_) {
      _fetching.remove(page);
    }
  }

  void _prefetchAround(int page) {
    // Pre-fetch a window around the current page
    for (int p = page - 1; p <= page + 4; p++) {
      if (p >= 1 && p <= _totalPages) {
        _fetchPage(p);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.fileName),
        actions: [
          if (_totalPages > 0)
            Center(
              child: Padding(
                padding: const EdgeInsets.only(right: 16),
                child: Text('$_currentPage / $_totalPages'),
              ),
            ),
        ],
      ),
      body: _buildBody(),
    );
  }

  Widget _buildBody() {
    if (_loading) {
      return const Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(),
            SizedBox(height: 16),
            Text('Loading document...'),
          ],
        ),
      );
    }

    if (_error != null) {
      return Center(
        child: Padding(
          padding: const EdgeInsets.all(24),
          child: Column(
            mainAxisSize: MainAxisSize.min,
            children: [
              const Icon(Icons.error_outline, size: 48, color: Colors.red),
              const SizedBox(height: 16),
              Text(_error!, textAlign: TextAlign.center),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: () {
                  setState(() {
                    _loading = true;
                    _error = null;
                  });
                  _init();
                },
                child: const Text('Retry'),
              ),
            ],
          ),
        ),
      );
    }

    if (_totalPages == 0) return const SizedBox();

    return PageView.builder(
      controller: _pageController,
      itemCount: _totalPages,
      onPageChanged: (index) {
        final page = index + 1;
        setState(() => _currentPage = page);
        _prefetchAround(page);
      },
      itemBuilder: (context, index) {
        final page = index + 1;
        final path = _pageFiles[page];

        if (path != null) {
          return PDFView(
            key: ValueKey('page_$page'),
            filePath: path,
            enableSwipe: false,
            pageFling: false,
            onError: (error) {
              setState(() => _error = 'PDF render error: $error');
            },
          );
        }

        // Page not yet fetched — trigger fetch and show spinner
        _fetchPage(page);
        return const Center(child: CircularProgressIndicator());
      },
    );
  }
}
