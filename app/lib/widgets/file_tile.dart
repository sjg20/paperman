import 'package:cached_network_image/cached_network_image.dart';
import 'package:flutter/material.dart';
import 'package:provider/provider.dart';
import '../models/models.dart';
import '../services/api_service.dart';

class FileTile extends StatelessWidget {
  final FileEntry file;
  final String? repo;
  final bool showFullPath;
  final VoidCallback onTap;

  const FileTile({
    super.key,
    required this.file,
    this.repo,
    this.showFullPath = false,
    required this.onTap,
  });

  String _formatSize(int bytes) {
    if (bytes < 1024) return '$bytes B';
    if (bytes < 1024 * 1024) return '${(bytes / 1024).toStringAsFixed(1)} KB';
    return '${(bytes / (1024 * 1024)).toStringAsFixed(1)} MB';
  }

  @override
  Widget build(BuildContext context) {
    final api = context.read<ApiService>();
    final thumbUrl = api.getThumbnailUrl(
      path: file.path,
      repo: repo,
      size: 'small',
    );

    return ListTile(
      leading: SizedBox(
        width: 48,
        height: 48,
        child: CachedNetworkImage(
          imageUrl: thumbUrl.toString(),
          httpHeaders: api.basicAuth != null
              ? {'Authorization': api.basicAuth!}
              : {},
          fit: BoxFit.cover,
          placeholder:
              (_, __) =>
                  const Icon(Icons.insert_drive_file, color: Colors.grey),
          errorWidget:
              (_, __, ___) =>
                  const Icon(Icons.insert_drive_file, color: Colors.grey),
        ),
      ),
      title: Text(
        showFullPath ? file.path : file.name,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
      ),
      subtitle: Text(_formatSize(file.size)),
      onTap: onTap,
    );
  }
}
