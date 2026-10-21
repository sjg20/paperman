import 'package:flutter_secure_storage/flutter_secure_storage.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// Keeps the password for the server in the phone's secure storage: the
/// Android Keystore, or the iPhone's Keychain.
///
/// It is never kept among the plain preferences, where the server's
/// address and the user name are, as an older version of the app kept it;
/// one found there is moved into secure storage.
class CredentialStore {
  CredentialStore({FlutterSecureStorage? storage})
    : _storage = storage ?? const FlutterSecureStorage();

  final FlutterSecureStorage _storage;

  static const _key = 'password';

  /// The saved password, or an empty string if there is none.
  ///
  /// A password kept in [prefs] by an older version is moved into secure
  /// storage first. One which cannot be read, as after a phone is restored
  /// from a backup, which carries the stored password but not the key to
  /// it, is forgotten, so that the user is asked for it again.
  Future<String> readPassword(SharedPreferences prefs) async {
    final old = prefs.getString(_key);

    if (old != null) {
      if (old.isNotEmpty) await _storage.write(key: _key, value: old);
      await prefs.remove(_key);
    }
    try {
      return await _storage.read(key: _key) ?? '';
    } catch (_) {
      await _storage.delete(key: _key);
      return '';
    }
  }

  /// Keep [password], or forget the one kept if it is empty.
  ///
  /// Where there is no secure storage to be had, as on a Mac without the
  /// entitlement for the Keychain, the password is not kept, and the user
  /// gives it again next time, rather than connecting failing.
  Future<void> writePassword(String password) async {
    try {
      if (password.isEmpty) {
        await _storage.delete(key: _key);
      } else {
        await _storage.write(key: _key, value: password);
      }
    } catch (_) {}
  }
}
