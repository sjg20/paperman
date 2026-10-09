import 'dart:convert';
import 'dart:io';

import 'package:flutter_test/flutter_test.dart';

import 'package:paperman/services/api_service.dart';

/* The test binding makes every HTTP request fail, so that widget tests
   cannot reach the network; these tests talk to a server of their own */
class RealHttp extends HttpOverrides {}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('logging in', () {
    late HttpServer server;
    final seen = <String>[];

    setUp(() async {
      seen.clear();
      server = await HttpServer.bind(InternetAddress.loopbackIPv4, 0);
      server.listen((req) async {
        final auth = req.headers.value('authorization') ?? '';
        seen.add('${req.method} ${req.uri.path} $auth');
        if (req.uri.path == '/v1/auth/login') {
          final body = jsonDecode(await utf8.decodeStream(req));
          final ok = body['user'] == 'sam' && body['password'] == 'pw';
          req.response.statusCode = ok ? 200 : 401;
          req.response.write(jsonEncode(ok ? {'token': 't0k'} : {}));
        } else if (auth == 'Bearer t0k') {
          req.response.write(jsonEncode({
            'repositories': [
              {'name': 'papers', 'path': '/srv/papers', 'exists': true},
            ],
          }));
        } else {
          req.response.statusCode = 401;
          req.response.write('{"success":false}');
        }
        await req.response.close();
      });
    });

    tearDown(() => server.close(force: true));

    test('logs in to a server with accounts and keeps the token', () {
      return HttpOverrides.runWithHttpOverrides(() async {
        final api = ApiService(
          baseUrl: 'http://127.0.0.1:${server.port}',
          username: 'sam',
          password: 'pw',
        );
        final repos = await api.getRepos();
        expect(repos.map((r) => r.name), ['papers']);
        expect(api.authHeader, 'Bearer t0k');

        // the token is used from then on, with no second login
        await api.getRepos();
        expect(seen.where((s) => s.startsWith('POST')).length, 1);
      }, RealHttp());
    });

    test('a wrong password is reported', () {
      return HttpOverrides.runWithHttpOverrides(() async {
        final api = ApiService(
          baseUrl: 'http://127.0.0.1:${server.port}',
          username: 'sam',
          password: 'wrong',
        );
        await expectLater(
          api.getRepos(),
          throwsA(isA<ApiException>()
              .having((e) => e.statusCode, 'status', 401)),
        );
        expect(api.authHeader, startsWith('Basic '));
      }, RealHttp());
    });
  });
}
