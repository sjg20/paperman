import 'package:flutter/material.dart';
import 'package:package_info_plus/package_info_plus.dart';
import 'package:provider/provider.dart';
import 'package:shared_preferences/shared_preferences.dart';
import '../services/api_service.dart' show ApiException, ApiService;
import 'browse_screen.dart';

class ConnectionScreen extends StatefulWidget {
  final bool autoConnect;

  const ConnectionScreen({super.key, this.autoConnect = true});

  @override
  State<ConnectionScreen> createState() => _ConnectionScreenState();
}

class _ConnectionScreenState extends State<ConnectionScreen> {
  final _urlController = TextEditingController();
  final _localUrlController = TextEditingController();
  final _userController = TextEditingController();
  final _passController = TextEditingController();
  bool _connecting = false;
  String? _error;
  String? _version;

  static const _buildDate = String.fromEnvironment(
    'BUILD_DATE',
    defaultValue: 'unknown',
  );

  @override
  void initState() {
    super.initState();
    _loadVersion();
    _loadSaved();
  }

  Future<void> _loadVersion() async {
    final info = await PackageInfo.fromPlatform();
    if (mounted) {
      setState(() => _version = info.version);
    }
  }

  Future<void> _loadSaved() async {
    final prefs = await SharedPreferences.getInstance();
    _urlController.text = prefs.getString('server_url') ?? '';
    _localUrlController.text = prefs.getString('local_url') ?? '';
    _userController.text = prefs.getString('username') ?? '';
    _passController.text = prefs.getString('password') ?? '';
    // Auto-connect if we have a saved URL (unless returning from disconnect)
    if (widget.autoConnect && _urlController.text.isNotEmpty) {
      _connect();
    }
  }

  void _connectDemo() {
    final api = context.read<ApiService>();
    api.enableDemo();
    Navigator.of(context).pushReplacement(
      MaterialPageRoute(builder: (_) => const BrowseScreen()),
    );
  }

  Future<void> _connect() async {
    var url = _urlController.text.trim();
    if (url.isEmpty) {
      setState(() => _error = 'Please enter a server URL');
      return;
    }
    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      url = 'https://$url';
      _urlController.text = url;
    }

    setState(() {
      _connecting = true;
      _error = null;
    });

    final api = context.read<ApiService>();
    final user = _userController.text.trim();
    final pass = _passController.text;
    final localUrl = _localUrlController.text.trim();
    api.updateConfig(
      baseUrl: url,
      localBaseUrl: localUrl.isEmpty ? null : localUrl,
      username: user.isEmpty ? null : user,
      password: pass.isEmpty ? null : pass,
    );

    try {
      await api.tryLocalUrl();
      await api.checkStatus();

      // Save settings
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString('server_url', url);
      await prefs.setString('local_url', localUrl);
      await prefs.setString('username', user);
      await prefs.setString('password', pass);

      if (!mounted) return;
      Navigator.of(context).pushReplacement(
        MaterialPageRoute(builder: (_) => const BrowseScreen()),
      );
    } catch (e) {
      final message = e is ApiException ? e.message : '$e';
      setState(() {
        _connecting = false;
        _error = message;
      });
    }
  }

  @override
  void dispose() {
    _urlController.dispose();
    _localUrlController.dispose();
    _userController.dispose();
    _passController.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Paperman')),
      body: Padding(
        padding: const EdgeInsets.all(24.0),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Icon(Icons.description, size: 64, color: Colors.blue),
            const SizedBox(height: 24),
            TextField(
              controller: _urlController,
              decoration: const InputDecoration(
                labelText: 'Server URL',
                hintText: 'https://papers.example.com',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.link),
              ),
              keyboardType: TextInputType.url,
              textInputAction: TextInputAction.next,
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _localUrlController,
              decoration: const InputDecoration(
                labelText: 'Local URL (optional)',
                hintText: 'http://192.168.1.10:8080',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.wifi),
              ),
              keyboardType: TextInputType.url,
              textInputAction: TextInputAction.next,
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _userController,
              decoration: const InputDecoration(
                labelText: 'Username',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.person),
              ),
              textInputAction: TextInputAction.next,
            ),
            const SizedBox(height: 16),
            TextField(
              controller: _passController,
              decoration: const InputDecoration(
                labelText: 'Password',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.lock),
              ),
              obscureText: true,
              textInputAction: TextInputAction.done,
              onSubmitted: (_) => _connect(),
            ),
            const SizedBox(height: 24),
            if (_error != null)
              Padding(
                padding: const EdgeInsets.only(bottom: 16),
                child: Text(
                  _error!,
                  style: const TextStyle(color: Colors.red),
                  textAlign: TextAlign.center,
                ),
              ),
            ElevatedButton.icon(
              onPressed: _connecting ? null : _connect,
              icon:
                  _connecting
                      ? const SizedBox(
                        width: 20,
                        height: 20,
                        child: CircularProgressIndicator(strokeWidth: 2),
                      )
                      : const Icon(Icons.login),
              label: Text(_connecting ? 'Connecting...' : 'Connect'),
              style: ElevatedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 16),
              ),
            ),
            const SizedBox(height: 12),
            OutlinedButton.icon(
              onPressed: _connecting ? null : _connectDemo,
              icon: const Icon(Icons.play_circle_outline),
              label: const Text('Try Demo'),
              style: OutlinedButton.styleFrom(
                padding: const EdgeInsets.symmetric(vertical: 16),
              ),
            ),
            const Spacer(),
            if (_version != null)
              Text(
                'v$_version \u2014 Built $_buildDate',
                textAlign: TextAlign.center,
                style: Theme.of(context).textTheme.bodySmall?.copyWith(
                  color: Colors.grey,
                ),
              ),
          ],
        ),
      ),
    );
  }
}
