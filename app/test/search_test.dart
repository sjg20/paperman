import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:provider/provider.dart';

import 'package:paperman/models/models.dart';
import 'package:paperman/screens/search_screen.dart';
import 'package:paperman/screens/viewer_screen.dart';
import 'package:paperman/services/api_service.dart';

class MockApiService extends Mock implements ApiService {}

void main() {
  setUp(() {
    TestWidgetsFlutterBinding.ensureInitialized();
    const channel = MethodChannel('plugins.flutter.io/path_provider');
    TestDefaultBinaryMessengerBinding.instance.defaultBinaryMessenger
        .setMockMethodCallHandler(channel, (MethodCall call) async {
      return Directory.systemTemp.path;
    });
  });

  group('search screen', () {
    late MockApiService api;

    setUp(() {
      api = MockApiService();
      when(() => api.isDemo).thenReturn(false);
      when(() => api.authHeader).thenReturn(null);
      when(() => api.getPageCount(
            path: any(named: 'path'),
            repo: any(named: 'repo'),
          )).thenAnswer((_) async => 5);
      when(() => api.searchText(
            repo: any(named: 'repo'),
            text: any(named: 'text'),
            path: any(named: 'path'),
          )).thenAnswer((_) async => TextSearchResult(
            hits: [
              TextHit(
                path: 'sub/bill.max',
                page: 3,
                snippet: 'the <b>invoice</b> total',
              ),
            ],
            complete: false,
          ));
    });

    Future<void> pumpSearch(WidgetTester tester, {String? repo}) async {
      // above the app, so that the viewer a result opens finds it too
      await tester.pumpWidget(
        Provider<ApiService>.value(
          value: api,
          child: MaterialApp(home: SearchScreen(repo: repo)),
        ),
      );
    }

    testWidgets('finds stacks by the text on their pages', (tester) async {
      await pumpSearch(tester, repo: 'papers');
      await tester.tap(find.text('Text on the pages'));
      await tester.pump();
      await tester.enterText(find.byType(TextField), 'invoice');
      await tester.testTextInput.receiveAction(TextInputAction.search);
      await tester.pumpAndSettle();

      verify(() => api.searchText(
            repo: 'papers',
            text: 'invoice',
            path: null,
          )).called(1);
      expect(find.text('bill.max'), findsOneWidget);
      expect(find.text('sub · page 3'), findsOneWidget);
      expect(find.textContaining('still reading'), findsOneWidget);

      // the stack opens at the page which matched
      await tester.tap(find.text('bill.max'));
      await tester.pump();
      await tester.pump();
      final viewer = tester.widget<ViewerScreen>(find.byType(ViewerScreen));
      expect(viewer.filePath, 'sub/bill.max');
      expect(viewer.initialPage, 3);
    });

    testWidgets('offers text only within a repository', (tester) async {
      await pumpSearch(tester);
      expect(find.text('Text on the pages'), findsNothing);
    });
  });

  test('a snippet shows the words found in bold', () {
    final span = SearchScreenSnippet.span('a <b>tax</b> bill', null);
    final parts = span.children!.cast<TextSpan>();
    expect(parts.map((s) => s.text), ['a ', 'tax', ' bill']);
    expect(parts[1].style?.fontWeight, FontWeight.bold);
    expect(parts[0].style, isNull);
  });
}
