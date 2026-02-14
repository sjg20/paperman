import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import 'services/api_service.dart';
import 'screens/connection_screen.dart';

void main() {
  runApp(const PapermanApp());
}

class PapermanApp extends StatelessWidget {
  const PapermanApp({super.key});

  @override
  Widget build(BuildContext context) {
    return Provider<ApiService>(
      create: (_) => ApiService(baseUrl: ''),
      child: MaterialApp(
        title: 'Paperman',
        theme: ThemeData(
          colorSchemeSeed: Colors.blue,
          useMaterial3: true,
        ),
        darkTheme: ThemeData(
          colorSchemeSeed: Colors.blue,
          brightness: Brightness.dark,
          useMaterial3: true,
        ),
        home: const ConnectionScreen(),
      ),
    );
  }
}
