import 'package:flutter/services.dart';
import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:paperman/services/credential_store.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Secure storage whose password cannot be read, as after a phone is
/// restored from a backup without the key to it
class _Unreadable extends FlutterSecureStorage {
  const _Unreadable();

  @override
  Future<String?> read({
    required String key,
    AppleOptions? iOptions,
    AndroidOptions? aOptions,
    LinuxOptions? lOptions,
    WebOptions? webOptions,
    AppleOptions? mOptions,
    WindowsOptions? wOptions,
  }) => Future.error(PlatformException(code: 'Exception'));
}

void main() {
  TestWidgetsFlutterBinding.ensureInitialized();

  group('CredentialStore', () {
    test('keeps the password in secure storage, not the preferences',
        () async {
      SharedPreferences.setMockInitialValues({});
      FlutterSecureStorage.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();
      final store = CredentialStore();

      await store.writePassword('pw');
      expect(prefs.getString('password'), isNull);
      expect(await const FlutterSecureStorage().read(key: 'password'), 'pw');
      expect(await store.readPassword(prefs), 'pw');
    });

    test('moves a password from the preferences into secure storage',
        () async {
      SharedPreferences.setMockInitialValues({'password': 'old'});
      FlutterSecureStorage.setMockInitialValues({});
      final prefs = await SharedPreferences.getInstance();

      expect(await CredentialStore().readPassword(prefs), 'old');
      expect(prefs.getString('password'), isNull);
      expect(await const FlutterSecureStorage().read(key: 'password'), 'old');
    });

    test('an empty password forgets the one kept', () async {
      SharedPreferences.setMockInitialValues({});
      FlutterSecureStorage.setMockInitialValues({'password': 'pw'});
      final prefs = await SharedPreferences.getInstance();
      final store = CredentialStore();

      await store.writePassword('');
      expect(await store.readPassword(prefs), '');
    });

    test('a password which cannot be read is forgotten', () async {
      SharedPreferences.setMockInitialValues({});
      FlutterSecureStorage.setMockInitialValues({'password': 'pw'});
      final prefs = await SharedPreferences.getInstance();

      expect(
        await CredentialStore(storage: const _Unreadable())
            .readPassword(prefs),
        '',
      );
      expect(await const FlutterSecureStorage().read(key: 'password'), isNull);
    });
  });
}
