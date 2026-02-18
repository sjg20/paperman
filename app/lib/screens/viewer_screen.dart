import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:pdfrx/pdfrx.dart';
import 'package:path_provider/path_provider.dart';
import 'package:provider/provider.dart';
import '../services/api_service.dart';
import '../services/demo_data.dart';

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

  /// Return the range of 0-based indices that overlap [viewportTop]
  /// to [viewportBottom] in child coordinates, clamped to [0, count).
  @visibleForTesting
  static (int first, int last) visibleRange({
    required double viewportTop,
    required double viewportBottom,
    required double extent,
    required int pageCount,
  }) {
    final first = (viewportTop / extent).floor().clamp(0, pageCount - 1);
    final last = (viewportBottom / extent).ceil().clamp(0, pageCount - 1);
    return (first, last);
  }

  @override
  State<ViewerScreen> createState() => _ViewerScreenState();
}

class _ViewerScreenState extends State<ViewerScreen> {
  static const _defaultAspectRatio = 1.414; // A4 portrait
  static const _pageGap = 4.0;
  static const _prefetchBefore = 2;
  static const _prefetchAfter = 5;
  static const _evictDistance = 10;
  static const _overviewColumns = 4;
  static const _overviewThreshold = 0.75;

  int _totalPages = 0;
  int _currentPage = 1;
  bool _loading = true;
  String? _error;
  bool _showOverview = false;

  final Map<int, PdfDocument> _documents = {};
  final Map<int, String> _diskPaths = {};
  final Map<int, Size> _pageSizes = {};
  final Set<int> _fetching = {};
  final Map<int, double> _progress = {};
  final Map<int, int> _progressTotal = {};
  Timer? _scrollDebounce;
  Timer? _gestureTimer;
  bool _isGesturing = false;
  double _settledDpi = 72.0;
  int? _scrubTarget; // page the user wants to reach via the scroller

  /// In demo mode the full multi-page PDF is kept open here.
  PdfDocument? _demoDoc;

  late String _safeName;
  late Directory _cacheDir;
  final _transformController = TransformationController();
  final _overviewScrollController = ScrollController();

  @override
  void initState() {
    super.initState();
    _transformController.addListener(_onTransformChanged);
    _init();
  }

  @override
  void dispose() {
    _scrollDebounce?.cancel();
    _gestureTimer?.cancel();
    _transformController.removeListener(_onTransformChanged);
    _transformController.dispose();
    _overviewScrollController.dispose();
    _demoDoc?.dispose();
    for (final doc in _documents.values) {
      doc.dispose();
    }
    super.dispose();
  }

  Future<void> _init() async {
    _safeName = widget.fileName.replaceAll(RegExp(r'[^\w.]'), '_');
    final api = context.read<ApiService>();
    _cacheDir = await getTemporaryDirectory();

    try {
      if (api.isDemo) {
        await _initDemo();
      } else {
        await _initRemote(api);
      }
    } catch (e) {
      if (!mounted) return;
      setState(() {
        _loading = false;
        _error = 'Failed to load document: $e';
      });
    }
  }

  Future<void> _initDemo() async {
    final bytes = await DemoData.loadPageBytes(widget.filePath);
    final filePath = '${_cacheDir.path}/paperman_${_safeName}_full.pdf';
    await File(filePath).writeAsBytes(bytes);
    final doc = await PdfDocument.openFile(filePath);

    for (int i = 0; i < doc.pages.length; i++) {
      final p = doc.pages[i];
      _pageSizes[i + 1] = Size(p.width, p.height);
    }

    if (!mounted) {
      doc.dispose();
      return;
    }
    setState(() {
      _demoDoc = doc;
      _totalPages = doc.pages.length;
      _loading = false;
    });
  }

  Future<void> _initRemote(ApiService api) async {
    final pageCount = await api.getPageCount(
      path: widget.filePath,
      repo: widget.repo,
    );

    if (!mounted) return;
    setState(() {
      _totalPages = pageCount;
      _loading = false;
    });

    _prefetchAround(1);
  }

  Future<void> _fetchPage(int page) async {
    if (_documents.containsKey(page) || _fetching.contains(page)) return;
    if (page < 1 || page > _totalPages) return;

    _fetching.add(page);

    try {
      final path = _diskPaths[page];
      PdfDocument doc;

      if (path != null && File(path).existsSync()) {
        // Reopen from disk cache
        doc = await PdfDocument.openFile(path);
      } else {
        // Download from server
        final api = context.read<ApiService>();
        final bytes = await api.downloadFilePageStreamed(
          path: widget.filePath,
          page: page,
          repo: widget.repo,
          onProgress: (received, total) {
            if (!mounted) return;
            setState(() {
              _progress[page] = total > 0 ? received / total : 0;
              if (total > 0) _progressTotal[page] = total;
            });
          },
        );

        final filePath =
            '${_cacheDir.path}/paperman_${_safeName}_p$page.pdf';
        await File(filePath).writeAsBytes(bytes);
        _diskPaths[page] = filePath;

        doc = await PdfDocument.openFile(filePath);
      }

      // Read actual page dimensions
      if (doc.pages.isNotEmpty) {
        final pdfPage = doc.pages[0];
        _pageSizes[page] = Size(pdfPage.width, pdfPage.height);
      }

      if (!mounted) {
        doc.dispose();
        return;
      }
      final pendingScrub = _scrubTarget == page;
      if (pendingScrub) _scrubTarget = null;

      setState(() {
        _documents[page] = doc;
        _fetching.remove(page);
        _progress.remove(page);
        _progressTotal.remove(page);
      });

      if (pendingScrub) {
        WidgetsBinding.instance.addPostFrameCallback((_) {
          if (!mounted) return;
          final viewWidth = MediaQuery.of(context).size.width;
          final extent = _itemExtent(viewWidth);
          _jumpToPage(page, extent);
        });
      }
    } catch (_) {
      _fetching.remove(page);
      _progress.remove(page);
      _progressTotal.remove(page);
    }
  }

  void _prefetchAround(int page) {
    for (int p = page - _prefetchBefore; p <= page + _prefetchAfter; p++) {
      if (p >= 1 && p <= _totalPages) {
        _fetchPage(p);
      }
    }
    _evictDistantPages(page);
  }

  void _evictDistantPages(int page) {
    final toEvict = <int>[];
    for (final p in _documents.keys) {
      if ((p - page).abs() > _evictDistance) {
        toEvict.add(p);
      }
    }
    for (final p in toEvict) {
      _documents[p]!.dispose();
      _documents.remove(p);
    }
  }

  double _itemExtent(double viewWidth) =>
      viewWidth * _defaultAspectRatio + _pageGap;

  /// Derive the current page from the transform and trigger prefetching.
  void _onTransformChanged() {
    if (_totalPages == 0) return;

    if (!_showOverview) {
      final scale = _transformController.value.getMaxScaleOnAxis();
      if (scale < _overviewThreshold) {
        _enterOverview();
        return;
      }
    }

    _scrollDebounce?.cancel();
    _scrollDebounce = Timer(const Duration(milliseconds: 200), () {
      if (!mounted) return;
      final viewWidth = MediaQuery.of(context).size.width;
      final extent = _itemExtent(viewWidth);
      final page = _pageFromTransform(extent);

      if (page != _currentPage) {
        setState(() => _currentPage = page);
      }
      _prefetchAround(page);
    });
  }

  /// Return the 1-based page number visible at the top of the viewport.
  int _pageFromTransform(double extent) {
    final matrix = _transformController.value;
    final scale = matrix.getMaxScaleOnAxis();
    final ty = matrix.getTranslation().y;
    final childTopY = -ty / scale;
    final row = (childTopY / extent).floor().clamp(0, _totalPages - 1);
    return (row + 1).clamp(1, _totalPages);
  }

  /// Set the transform to show the given page at the top of the viewport,
  /// preserving the current zoom level.
  void _jumpToPage(int page, double itemExtent) {
    final scale = _transformController.value.getMaxScaleOnAxis();
    _transformController.value = Matrix4.identity()
      ..translateByDouble(0.0, -scale * (page - 1) * itemExtent, 0.0, 1.0)
      ..scaleByDouble(scale, scale, 1.0, 1.0);
  }

  void _enterOverview() {
    setState(() => _showOverview = true);
    _transformController.value = Matrix4.identity();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final viewWidth = MediaQuery.of(context).size.width;
      final cellWidth = viewWidth / _overviewColumns;
      final extent = cellWidth * _defaultAspectRatio + _pageGap;
      final row = (_currentPage - 1) ~/ _overviewColumns;
      _overviewScrollController.jumpTo(row * extent);
    });
  }

  void _exitOverview(int page) {
    setState(() {
      _showOverview = false;
      _currentPage = page;
    });
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!mounted) return;
      final viewWidth = MediaQuery.of(context).size.width;
      final extent = _itemExtent(viewWidth);
      _jumpToPage(page, extent);
    });
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

    if (_showOverview) return _buildOverview();

    return LayoutBuilder(
      builder: (context, constraints) {
        final viewWidth = constraints.maxWidth;

        return Stack(
          children: [
            InteractiveViewer.builder(
              transformationController: _transformController,
              interactionEndFrictionCoefficient: 0.01,
              boundaryMargin: EdgeInsets.symmetric(
                vertical: constraints.maxHeight / 2,
                horizontal: constraints.maxWidth / 2,
              ),
              panAxis: PanAxis.aligned,
              minScale: 0.5,
              maxScale: 5.0,
              onInteractionStart: (_) {
                _gestureTimer?.cancel();
                _isGesturing = true;
              },
              onInteractionEnd: (_) {
                _gestureTimer?.cancel();
                _gestureTimer =
                    Timer(const Duration(milliseconds: 200), () {
                  if (!mounted) return;
                  setState(() => _isGesturing = false);
                });
              },
              builder: (context, viewport) {
                final extent = _itemExtent(viewWidth);
                final totalHeight = extent * _totalPages;

                final top = viewport.point0.y.clamp(0.0, totalHeight);
                final bottom =
                    viewport.point2.y.clamp(0.0, totalHeight);
                var (first, last) = ViewerScreen.visibleRange(
                  viewportTop: top,
                  viewportBottom: bottom,
                  extent: extent,
                  pageCount: _totalPages,
                );
                // Keep one extra page on each side so pages that are
                // about to scroll in are already rendered, avoiding a
                // white flash.
                if (first > 0) first--;
                if (last < _totalPages - 1) last++;

                final children = <Widget>[];
                for (int i = first; i <= last; i++) {
                  final page = i + 1;
                  children.add(
                    Positioned(
                      key: ValueKey<int>(page),
                      top: i * extent,
                      left: 0,
                      width: viewWidth,
                      height: extent - _pageGap,
                      child: _buildPage(page, viewWidth, extent - _pageGap),
                    ),
                  );
                }

                return SizedBox(
                  width: viewWidth,
                  height: totalHeight,
                  child: Stack(children: children),
                );
              },
            ),
            if (_scrubTarget != null)
              const Center(child: CircularProgressIndicator()),
            if (_totalPages > 1)
              _buildPageSlider(constraints.maxHeight),
          ],
        );
      },
    );
  }

  Widget _buildOverview() {
    return LayoutBuilder(
      builder: (context, constraints) {
        final viewWidth = constraints.maxWidth;
        final cellWidth = viewWidth / _overviewColumns;
        final cellHeight = cellWidth * _defaultAspectRatio;
        final extent = cellHeight + _pageGap;
        final totalRows =
            (_totalPages + _overviewColumns - 1) ~/ _overviewColumns;

        return ListView.builder(
          controller: _overviewScrollController,
          itemCount: totalRows,
          itemExtent: extent,
          itemBuilder: (context, row) {
            final children = <Widget>[];
            for (int col = 0; col < _overviewColumns; col++) {
              final page = row * _overviewColumns + col + 1;
              if (page > _totalPages) {
                children.add(SizedBox(width: cellWidth));
                continue;
              }
              _fetchPage(page);
              children.add(
                SizedBox(
                  key: ValueKey<int>(page),
                  width: cellWidth,
                  height: cellHeight,
                  child: GestureDetector(
                    onTap: () => _exitOverview(page),
                    child: Padding(
                      padding: const EdgeInsets.all(_pageGap / 2),
                      child: _buildPage(
                        page,
                        cellWidth - _pageGap,
                        cellHeight - _pageGap,
                      ),
                    ),
                  ),
                ),
              );
            }
            return Row(children: children);
          },
        );
      },
    );
  }

  void _scrubToPage(double dy, double sliderHeight) {
    final fraction = (dy / sliderHeight).clamp(0.0, 1.0);
    final page = (fraction * (_totalPages - 1)).round() + 1;

    // Update the thumb and appbar immediately
    if (page != _currentPage) {
      setState(() => _currentPage = page);
    }

    if (_documents.containsKey(page) || _demoDoc != null) {
      // Page is ready — scroll now
      _scrubTarget = null;
      final viewWidth = MediaQuery.of(context).size.width;
      final extent = _itemExtent(viewWidth);
      _jumpToPage(page, extent);
    } else {
      // Page not yet loaded — start fetching but don't scroll yet.
      // Don't call _prefetchAround() here: that would evict pages
      // around the current viewport while it is still visible.
      // Prefetching and eviction happen naturally via
      // _onTransformChanged once the deferred jump fires.
      _scrubTarget = page;
      _fetchPage(page);
    }
  }

  Widget _buildPageSlider(double height) {
    const thumbHeight = 32.0;
    final fraction = _totalPages > 1
        ? (_currentPage - 1) / (_totalPages - 1)
        : 0.0;
    final trackSpace = height - thumbHeight;
    final color = Theme.of(context).colorScheme.primary;

    return Positioned(
      right: 4,
      top: 0,
      bottom: 0,
      width: 28,
      child: GestureDetector(
        behavior: HitTestBehavior.opaque,
        onVerticalDragStart: (d) =>
            _scrubToPage(d.localPosition.dy, height),
        onVerticalDragUpdate: (d) =>
            _scrubToPage(d.localPosition.dy, height),
        onTapDown: (d) =>
            _scrubToPage(d.localPosition.dy, height),
        child: Stack(
          children: [
            // Track
            Positioned.fill(
              child: Center(
                child: Container(
                  width: 4,
                  decoration: BoxDecoration(
                    color: color.withValues(alpha: 0.2),
                    borderRadius: BorderRadius.circular(2),
                  ),
                ),
              ),
            ),
            // Thumb
            Positioned(
              top: fraction * trackSpace,
              left: 0,
              right: 0,
              height: thumbHeight,
              child: Container(
                decoration: BoxDecoration(
                  color: color,
                  borderRadius: BorderRadius.circular(4),
                ),
                alignment: Alignment.center,
                child: Text(
                  '$_currentPage',
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 10,
                    fontWeight: FontWeight.bold,
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  /// Return the DPI cap for PdfPageView.  When settled this returns the
  /// pdfrx default (300) so PdfPageView renders at full quality.  During
  /// an active gesture the DPI is frozen at the pre-gesture value so
  /// PdfPageView does not re-render (which would flash white).
  static const _defaultMaximumDpi = 300.0;
  double _effectiveDpi() {
    if (_isGesturing) return _settledDpi;
    return _settledDpi = _defaultMaximumDpi;
  }

  static String _formatBytes(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(0)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  Widget _buildPage(int page, double width, double height) {
    final dpi = _effectiveDpi();

    if (_demoDoc != null) {
      return SizedBox(
        width: width,
        height: height,
        child: PdfPageView(
          document: _demoDoc!,
          pageNumber: page,
          alignment: Alignment.center,
          maximumDpi: dpi,
        ),
      );
    }

    final doc = _documents[page];

    if (doc != null) {
      return SizedBox(
        width: width,
        height: height,
        child: PdfPageView(
          document: doc,
          pageNumber: 1,
          alignment: Alignment.center,
          maximumDpi: dpi,
        ),
      );
    }

    // Page not yet loaded — show progress placeholder
    final fraction = _progress[page];
    final total = _progressTotal[page];
    return SizedBox(
      width: width,
      height: height,
      child: Center(
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            CircularProgressIndicator(value: fraction),
            if (fraction != null) ...[
              const SizedBox(height: 12),
              Text(
                total != null
                    ? '${(fraction * 100).round()}% of ${_formatBytes(total)}'
                    : '${(fraction * 100).round()}%',
                style: Theme.of(context).textTheme.bodySmall,
              ),
            ],
          ],
        ),
      ),
    );
  }
}
