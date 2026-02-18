import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:provider/provider.dart';

import 'package:paperman/screens/viewer_screen.dart';
import 'package:paperman/services/api_service.dart';

class MockApiService extends Mock implements ApiService {}

void main() {
  late MockApiService api;

  setUp(() {
    TestWidgetsFlutterBinding.ensureInitialized();

    // Safety net for environments where path_provider_linux is not registered
    const channel = MethodChannel('plugins.flutter.io/path_provider');
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
      return Directory.systemTemp.path;
    });

    api = MockApiService();
    when(() => api.isDemo).thenReturn(false);
  });

  Future<void> pumpViewer(
    WidgetTester tester, {
    String filePath = '/test.pdf',
    String fileName = 'test.pdf',
  }) async {
    await tester.pumpWidget(
      MaterialApp(
        home: Provider<ApiService>.value(
          value: api,
          child: ViewerScreen(filePath: filePath, fileName: fileName),
        ),
      ),
    );
  }

  void stubPageCount(int count) {
    when(() => api.getPageCount(
          path: any(named: 'path'),
          repo: any(named: 'repo'),
        )).thenAnswer((_) async => count);
    when(() => api.downloadFilePageStreamed(
          path: any(named: 'path'),
          page: any(named: 'page'),
          repo: any(named: 'repo'),
          onProgress: any(named: 'onProgress'),
        )).thenThrow(Exception('no rendering in tests'));
  }

  /// Pump enough frames for the async _init() to complete.
  /// Cannot use pumpAndSettle because indeterminate progress indicators
  /// on unloaded pages animate indefinitely.
  Future<void> settle(WidgetTester tester) async {
    for (int i = 0; i < 5; i++) {
      await tester.pump();
    }
  }

  testWidgets('shows loading indicator initially', (tester) async {
    stubPageCount(5);
    await pumpViewer(tester);

    expect(find.byType(CircularProgressIndicator), findsOneWidget);
    expect(find.text('Loading document...'), findsOneWidget);
  });

  testWidgets('shows error state on failure', (tester) async {
    when(() => api.getPageCount(
          path: any(named: 'path'),
          repo: any(named: 'repo'),
        )).thenThrow(Exception('server error'));

    await pumpViewer(tester);
    await tester.pumpAndSettle();

    expect(find.byIcon(Icons.error_outline), findsOneWidget);
    expect(find.text('Retry'), findsOneWidget);
  });

  testWidgets('shows file name in app bar', (tester) async {
    stubPageCount(5);
    await pumpViewer(tester, fileName: 'report.pdf');

    expect(find.text('report.pdf'), findsOneWidget);
  });

  testWidgets('shows page counter after loading', (tester) async {
    stubPageCount(20);
    await pumpViewer(tester);
    await settle(tester);

    expect(find.text('1 / 20'), findsOneWidget);
  });

  testWidgets('page slider visible for multi-page documents', (tester) async {
    stubPageCount(20);
    await pumpViewer(tester);
    await settle(tester);

    final slider = find.byWidgetPredicate(
      (w) => w is Positioned && w.width == 28,
    );
    expect(slider, findsOneWidget);
  });

  testWidgets('no page slider for single-page document', (tester) async {
    stubPageCount(1);
    await pumpViewer(tester);
    await settle(tester);

    final slider = find.byWidgetPredicate(
      (w) => w is Positioned && w.width == 28,
    );
    expect(slider, findsNothing);
  });

  testWidgets('slider drag updates page counter', (tester) async {
    stubPageCount(50);
    await pumpViewer(tester);
    await settle(tester);

    expect(find.text('1 / 50'), findsOneWidget);

    // Find the slider's GestureDetector (the only opaque one with drag)
    final slider = find.byWidgetPredicate(
      (w) =>
          w is GestureDetector &&
          w.behavior == HitTestBehavior.opaque &&
          w.onVerticalDragStart != null,
    );
    expect(slider, findsOneWidget);

    await tester.drag(slider, const Offset(0, 200));
    await tester.pump();

    // Page counter should no longer show page 1
    expect(find.text('1 / 50'), findsNothing);
  });

  testWidgets('overview mode when zoomed out below threshold', (tester) async {
    stubPageCount(10);
    await pumpViewer(tester);
    await settle(tester);

    // Default zoom (scale 1.0): normal InteractiveViewer mode.
    expect(find.byType(InteractiveViewer), findsOneWidget);
    expect(find.byType(ListView), findsNothing);

    // Zoom out below the 0.75 threshold to trigger overview mode.
    final viewer = tester.widget<InteractiveViewer>(
      find.byType(InteractiveViewer),
    );
    viewer.transformationController!.value =
        Matrix4.identity()..scale(0.6);
    await tester.pump();

    // Overview replaces the InteractiveViewer with a ListView.
    expect(find.byType(InteractiveViewer), findsNothing);
    expect(find.byType(ListView), findsOneWidget);

    // First row should have pages 1–6 (6-across grid).
    expect(find.byKey(const ValueKey<int>(1)), findsOneWidget);
    expect(find.byKey(const ValueKey<int>(2)), findsOneWidget);
    expect(find.byKey(const ValueKey<int>(3)), findsOneWidget);
    expect(find.byKey(const ValueKey<int>(4)), findsOneWidget);
    expect(find.byKey(const ValueKey<int>(5)), findsOneWidget);
    expect(find.byKey(const ValueKey<int>(6)), findsOneWidget);

    // Tap page 3 → return to normal viewer at that page.
    await tester.tap(find.byKey(const ValueKey<int>(3)));
    await tester.pump();

    expect(find.byType(InteractiveViewer), findsOneWidget);
    expect(find.byType(ListView), findsNothing);
    expect(find.text('3 / 10'), findsOneWidget);
  });

  testWidgets('scrolling down shows correct page', (tester) async {
    stubPageCount(50);
    await pumpViewer(tester);
    await settle(tester);

    expect(find.text('1 / 50'), findsOneWidget);

    // The default test surface is 800x600.  itemExtent = 800*1.414+4 = 1135.2
    // Each tester.drag() loses ~20 px to touch-slop before the gesture
    // recogniser activates, so add that back to get an effective pan of
    // exactly one quarter-page per drag.
    const extent = 800.0 * 1.414 + 4.0; // 1135.2
    const slop = 20.0; // kDragSlopDefault
    final quarterDrag = Offset(0, -(extent / 4 + slop));

    final viewer = find.byType(InteractiveViewer);
    expect(viewer, findsOneWidget);

    // Scroll three pages in quarter-page increments, verifying the page
    // counter and rendered pages after each full page.
    for (int page = 1; page <= 3; page++) {
      for (int q = 0; q < 4; q++) {
        await tester.drag(viewer, quarterDrag);
        await tester.pump();
      }
      // Let the 200 ms scroll-debounce fire.
      await tester.pump(const Duration(milliseconds: 300));

      final expected = page + 1;
      expect(find.text('$expected / 50'), findsOneWidget);

      // Each page widget has ValueKey<int>(pageNumber).  Verify the
      // builder laid out the expected pages, not the old ones.
      // The builder keeps one extra page on each side of the visible
      // range as a render buffer, so page 1 lingers for one extra
      // page of scrolling before being removed.
      expect(find.byKey(ValueKey<int>(expected)), findsOneWidget,
          reason: 'page $expected should be visible');
      expect(find.byKey(ValueKey<int>(expected + 1)), findsOneWidget,
          reason: 'page ${expected + 1} should be visible');
    }

    // After scrolling 3 full pages, page 1 should be well outside the
    // buffer zone (current page 4, buffer keeps page 3 at most).
    expect(find.byKey(const ValueKey<int>(1)), findsNothing,
        reason: 'page 1 should have scrolled off');
  });
}
