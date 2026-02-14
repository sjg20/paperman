import 'package:flutter/material.dart';
import '../models/models.dart';

class DirectoryTile extends StatelessWidget {
  final DirectoryEntry directory;
  final VoidCallback onTap;

  const DirectoryTile({
    super.key,
    required this.directory,
    required this.onTap,
  });

  @override
  Widget build(BuildContext context) {
    return ListTile(
      leading: const Icon(Icons.folder, color: Colors.amber),
      title: Text(directory.name),
      trailing: Text(
        '${directory.count}',
        style: Theme.of(context).textTheme.bodySmall,
      ),
      onTap: onTap,
    );
  }
}
