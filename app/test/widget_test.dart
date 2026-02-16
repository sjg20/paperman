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
}
