import 'dart:convert';
import 'dart:io';

import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:mocktail/mocktail.dart';
import 'package:provider/provider.dart';

import 'package:paperman/models/models.dart';
import 'package:paperman/screens/browse_screen.dart';
import 'package:paperman/services/api_service.dart';

/* The test binding makes every HTTP request fail, so that widget tests
   cannot reach the network; the upload tests talk to a server of their own */
class RealHttp extends HttpOverrides {}

class MockApiService extends Mock implements ApiService {}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  test('a scan is named for the day, as the desktop names one', () {
    expect(defaultScanName(DateTime(2026, 10, 9)), '09-Oct-26');
  });

  group('uploading a scan', () {
    late HttpServer server;
    final seen = <String>[];
    var bodies = <List<int>>[];

    setUp(() async {
      seen.clear();
      bodies = [];
      server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      server.listen((req) async {
        final auth = req.headers.value('authorization') ?? '';
        seen.add('${req.method} ${req.uri.path} $auth');
        if (req.uri.path == '/v1/auth/login') {
          req.response.write(jsonEncode({'token': 't0k'}));
        } else if (auth != 'Bearer t0k') {
          req.response.statusCode = 401;
          req.response.write('{"success":false}');
        } else {
          bodies.add(await req.fold<List<int>>([], (a, b) => a..addAll(b)));
          // the server gives one already there a new name
          req.response.write(jsonEncode({
            'success': true,
            'name': 'scan_1.pdf',
            'etag': 'e',
          }));
        }
        await req.response.close();
      });
    });

    tearDown(() => server.close(force: true));

    test('goes into the folder, logging in, and comes back named', () {
      return HttpOverrides.runWithHttpOverrides(() async {
        final api = ApiService(
          baseUrl: 'http://127.0.0.1:${server.port}',
          username: 'sam',
          password: 'pw',
        );
        final name = await api.uploadStack(
          repo: 'my papers',
          dir: 'bills/2026',
          name: 'scan.pdf',
          bytes: [1, 2, 3],
        );

        expect(name, 'scan_1.pdf');
        expect(bodies, [
          [1, 2, 3],
        ]);
        /* the first, with the name and password as for a proxy, is
           refused for want of a login, then sent again with the token */
        expect(seen, [
          'POST /v1/repos/my%20papers/stacks/bills/2026/scan.pdf/upload '
              'Basic ${base64Encode(utf8.encode('sam:pw'))}',
          'POST /v1/auth/login ',
          'POST /v1/repos/my%20papers/stacks/bills/2026/scan.pdf/upload '
              'Bearer t0k',
        ]);
      }, RealHttp());
    });

    test('says why one is refused', () {
      return HttpOverrides.runWithHttpOverrides(() async {
        final api = ApiService(baseUrl: 'http://127.0.0.1:${server.port}');

        expect(
          () => api.uploadStack(
            repo: 'papers',
            dir: '',
            name: 'scan.pdf',
            bytes: [1],
          ),
          throwsA(isA<ApiException>()),
        );
      }, RealHttp());
    });
  });

  group('the scan button', () {
    late MockApiService api;
    late Directory tmp;

    setUp(() {
      api = MockApiService();
      tmp = Directory.systemTemp.createTempSync('scan_test');
      when(() => api.isDemo).thenReturn(false);
      when(() => api.isUsingLocalUrl).thenReturn(false);
      when(() => api.getRepos()).thenAnswer(
        (_) async => [Repository(name: 'papers', path: '/p', exists: true)],
      );
      when(() => api.browse(
            path: any(named: 'path'),
            repo: any(named: 'repo'),
          )).thenAnswer((_) async => BrowseResult(
            path: '',
            directories: [],
            files: [],
          ));
    });

    tearDown(() => tmp.deleteSync(recursive: true));

    /* Let the upload read the scan from disk, which the test's clock
       does not wait for, drawing the screen meanwhile */
    Future<void> settle(WidgetTester tester) async {
      for (var i = 0; i < 20; i++) {
        await tester.runAsync(
          () => Future<void>.delayed(const Duration(milliseconds: 10)),
        );
        await tester.pump(const Duration(milliseconds: 50));
      }
    }

    Future<void> showFolder(WidgetTester tester, ScanPages scan) async {
      await tester.pumpWidget(
        MaterialApp(
          home: Provider<ApiService>.value(
            value: api,
            child: BrowseScreen(scanPages: scan),
          ),
        ),
      );
      await tester.pumpAndSettle();
    }

    testWidgets('scans, names and uploads into the folder shown',
        (tester) async {
      final pdf = File('${tmp.path}/scan.pdf')..writeAsBytesSync([4, 5, 6]);
      when(() => api.uploadStack(
            repo: any(named: 'repo'),
            dir: any(named: 'dir'),
            name: any(named: 'name'),
            bytes: any(named: 'bytes'),
          )).thenAnswer((_) async => 'receipt.pdf');

      await showFolder(tester, () async => pdf.path);
      await tester.tap(find.byType(FloatingActionButton));
      await tester.pumpAndSettle();

      // named for the day unless changed
      expect(find.text(defaultScanName()), findsOneWidget);
      await tester.enterText(find.byType(TextField), 'receipt');
      await tester.tap(find.text('Upload'));
      await settle(tester);

      final upload = verify(() => api.uploadStack(
            repo: 'papers',
            dir: '',
            name: 'receipt.pdf',
            bytes: captureAny(named: 'bytes'),
          ));
      upload.called(1);
      expect(upload.captured.single, [4, 5, 6]);
      expect(find.text('Scanned into receipt.pdf'), findsOneWidget);

      // the scan is dropped once it is up, and the folder listed again
      expect(pdf.existsSync(), isFalse);
      verify(() => api.browse(path: null, repo: 'papers')).called(2);
    });

    testWidgets('nothing is uploaded if the scan is given up',
        (tester) async {
      await showFolder(tester, () async => null);
      await tester.tap(find.byType(FloatingActionButton));
      await tester.pumpAndSettle();

      expect(find.byType(AlertDialog), findsNothing);
      verifyNever(() => api.uploadStack(
            repo: any(named: 'repo'),
            dir: any(named: 'dir'),
            name: any(named: 'name'),
            bytes: any(named: 'bytes'),
          ));
    });

    testWidgets('a failed upload can be tried again', (tester) async {
      final pdf = File('${tmp.path}/scan.pdf')..writeAsBytesSync([7]);
      var tries = 0;
      when(() => api.uploadStack(
            repo: any(named: 'repo'),
            dir: any(named: 'dir'),
            name: any(named: 'name'),
            bytes: any(named: 'bytes'),
          )).thenAnswer((_) async {
        if (++tries == 1) throw ApiException(0, 'no connection');
        return 'scan.pdf';
      });

      await showFolder(tester, () async => pdf.path);
      await tester.tap(find.byType(FloatingActionButton));
      await tester.pumpAndSettle();
      await tester.tap(find.text('Upload'));
      await settle(tester);

      // kept for another go, which works
      expect(find.text('Retry'), findsOneWidget);
      expect(pdf.existsSync(), isTrue);
      await tester.tap(find.text('Retry'));
      await settle(tester);
      expect(tries, 2);
      expect(pdf.existsSync(), isFalse);
    });
  });
}
