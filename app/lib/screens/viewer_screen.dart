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
  String? _localPath;
  bool _loading = true;
  String? _error;
  int _currentPage = 0;
  int _totalPages = 0;

  @override
  void initState() {
    super.initState();
    _downloadPdf();
  }

  Future<void> _downloadPdf() async {
    setState(() {
      _loading = true;
      _error = null;
    });

    final api = context.read<ApiService>();
    try {
      final response = await api.downloadFile(
        path: widget.filePath,
        repo: widget.repo,
        asPdf: true,
      );

      final dir = await getTemporaryDirectory();
      final file = File(
        '${dir.path}/paperman_${widget.fileName.replaceAll(RegExp(r'[^\w.]'), '_')}.pdf',
      );
      await file.writeAsBytes(response.bodyBytes);

      if (!mounted) return;
      setState(() {
        _localPath = file.path;
        _loading = false;
      });
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = 'Failed to load document: $e';
      });
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
                child: Text('${_currentPage + 1} / $_totalPages'),
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
                onPressed: _downloadPdf,
                child: const Text('Retry'),
              ),
            ],
          ),
        ),
      );
    }

    if (_localPath == null) return const SizedBox();

    return PDFView(
      filePath: _localPath!,
      enableSwipe: true,
      swipeHorizontal: false,
      autoSpacing: true,
      pageFling: true,
      onRender: (pages) {
        setState(() => _totalPages = pages ?? 0);
      },
      onViewCreated: (controller) {},
      onPageChanged: (page, total) {
        setState(() {
          _currentPage = page ?? 0;
          _totalPages = total ?? 0;
        });
      },
      onError: (error) {
        setState(() => _error = 'PDF render error: $error');
      },
    );
  }
}
