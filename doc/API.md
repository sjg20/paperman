# Paperman Search Server API Documentation

## Overview

The Paperman Search Server provides a REST API for searching, listing, and retrieving files from paper repositories. It supports on-the-fly PDF conversion from various formats.

**Base URL**: `http://localhost:8080`

**Version**: 1.0

All endpoints use the `GET` HTTP method and return JSON responses (except file downloads which return the file content).

## Authentication

Currently, no authentication is required. The server is designed for local or trusted network use.

## Common Response Format

### Success Response
```json
{
  "success": true,
  "data": "...",
  "count": 0
}
```

### Error Response
```json
{
  "success": false,
  "error": "Error message description"
}
```

## Endpoints

### 1. Server Status

Get the current server status and repository information.

**Endpoint**: `GET /status`

**Parameters**: None

**Response**:
```json
{
  "status": "running",
  "repository": "/path/to/repository"
}
```

**Example**:
```bash
curl http://localhost:8080/status
```

---

### 2. List Repositories

Get a list of all configured repositories.

**Endpoint**: `GET /repos`

**Parameters**: None

**Response**:
```json
{
  "success": true,
  "count": 2,
  "repositories": [
    {
      "path": "/home/user/papers",
      "name": "papers",
      "exists": true
    },
    {
      "path": "/home/user/archive",
      "name": "archive",
      "exists": true
    }
  ]
}
```

**Example**:
```bash
curl http://localhost:8080/repos
```

---

### 3. Search Files

Search for files matching a pattern in the repository.

**Endpoint**: `GET /search`

**Parameters**:

| Parameter   | Type    | Required | Default | Description                                    |
|-------------|---------|----------|---------|------------------------------------------------|
| `q`         | string  | Yes      | -       | Search pattern (partial filename match)        |
| `repo`      | string  | No       | First   | Repository name to search in                   |
| `path`      | string  | No       | Root    | Directory path to search in (relative to root) |
| `recursive` | boolean | No       | false   | Search subdirectories                          |

**Response**:
```json
{
  "success": true,
  "pattern": "invoice",
  "path": "/home/user/papers",
  "count": 3,
  "files": [
    {
      "name": "invoice-2023-01.pdf",
      "path": "invoices/invoice-2023-01.pdf",
      "size": 45632,
      "modified": "2023-01-15T10:30:00"
    },
    {
      "name": "invoice-2023-02.pdf",
      "path": "invoices/invoice-2023-02.pdf",
      "size": 52441,
      "modified": "2023-02-12T14:22:00"
    }
  ]
}
```

**Examples**:
```bash
# Basic search
curl "http://localhost:8080/search?q=invoice"

# Search in specific repository
curl "http://localhost:8080/search?q=invoice&repo=papers"

# Search in subdirectory
curl "http://localhost:8080/search?q=report&path=2023"

# Recursive search
curl "http://localhost:8080/search?q=contract&recursive=true"
```

**Notes**:
- Pattern matching is case-insensitive
- Searches for partial filename matches
- Only returns files with supported extensions (.max, .pdf, .jpg, .tiff)

---

### 4. List Directory Contents

List all files in a specific directory.

**Endpoint**: `GET /list`

**Parameters**:

| Parameter | Type   | Required | Default | Description                              |
|-----------|--------|----------|---------|------------------------------------------|
| `path`    | string | No       | Root    | Directory path (relative to repository)  |
| `repo`    | string | No       | First   | Repository name                          |

**Response**:
```json
{
  "success": true,
  "path": "invoices",
  "count": 5,
  "files": [
    {
      "name": "invoice-2023-01.pdf",
      "path": "invoices/invoice-2023-01.pdf",
      "size": 45632,
      "modified": "2023-01-15T10:30:00"
    },
    {
      "name": "invoice-2023-02.pdf",
      "path": "invoices/invoice-2023-02.pdf",
      "size": 52441,
      "modified": "2023-02-12T14:22:00"
    }
  ]
}
```

**Examples**:
```bash
# List root directory
curl "http://localhost:8080/list"

# List subdirectory
curl "http://localhost:8080/list?path=invoices"

# List in specific repository
curl "http://localhost:8080/list?path=2023&repo=archive"
```

---

### 5. Get File Content

Retrieve a file's content, optionally converting it to PDF.

**Endpoint**: `GET /file`

**Parameters**:

| Parameter | Type   | Required | Default    | Description                                    |
|-----------|--------|----------|------------|------------------------------------------------|
| `path`    | string | Yes      | -          | File path (relative to repository)             |
| `repo`    | string | No       | First      | Repository name                                |
| `type`    | string | No       | `original` | Output type: `original` or `pdf`               |

**Response**:
- **Success**: Binary file content with appropriate `Content-Type` header
- **Error**: JSON error response

**Content-Type Headers**:
- `.pdf` → `application/pdf`
- `.jpg`, `.jpeg` → `image/jpeg`
- `.tif`, `.tiff` → `image/tiff`
- `.max` → `application/octet-stream`
- PDF conversion → `application/pdf`

**Examples**:
```bash
# Download original file
curl "http://localhost:8080/file?path=invoice.pdf" -o invoice.pdf

# Download from specific repository
curl "http://localhost:8080/file?path=document.pdf&repo=archive" -o document.pdf

# Convert JPEG to PDF on-the-fly
curl "http://localhost:8080/file?path=scan.jpg&type=pdf" -o scan.pdf

# Convert .max file to PDF
curl "http://localhost:8080/file?path=document.max&type=pdf" -o document.pdf
```

**PDF Conversion**:
- Supports: `.max`, `.jpg`, `.jpeg`, `.tif`, `.tiff`
- Conversion timeout: 30 seconds
- Uses paperman's built-in conversion engine
- Maintains image quality and metadata

**Error Responses**:
```json
// File not found
{
  "success": false,
  "error": "File not found"
}

// Invalid path (directory traversal attempt)
{
  "success": false,
  "error": "Invalid file path"
}

// Conversion failed
{
  "success": false,
  "error": "PDF conversion failed: <error details>"
}

// Conversion timeout
{
  "success": false,
  "error": "PDF conversion timed out (30s limit)"
}
```

---

## Supported File Types

The server handles the following file types:

| Extension       | Description              | PDF Conversion | Direct View |
|-----------------|--------------------------|----------------|-------------|
| `.max`          | Paperman format          | ✅             | ❌          |
| `.pdf`          | PDF document             | N/A            | ✅          |
| `.jpg`, `.jpeg` | JPEG image               | ✅             | ✅          |
| `.tif`, `.tiff` | TIFF image               | ✅             | ✅          |

---

## Error Codes

| HTTP Code | Description                | Common Causes                           |
|-----------|----------------------------|-----------------------------------------|
| 200       | OK                         | Request successful                      |
| 400       | Bad Request                | Invalid path, missing parameters        |
| 404       | Not Found                  | File/endpoint not found                 |
| 405       | Method Not Allowed         | Non-GET request                         |
| 500       | Internal Server Error      | Conversion failed, file read error      |
| 501       | Not Implemented            | Unsupported conversion (deprecated)     |

---

## CORS

All endpoints include CORS headers:
```
Access-Control-Allow-Origin: *
```

This allows web applications from any origin to access the API.

---

## Rate Limiting

Currently, no rate limiting is implemented. The server is designed for trusted local or network use.

---

## Examples

### JavaScript/Fetch API

```javascript
// Search for files
fetch('http://localhost:8080/search?q=invoice')
  .then(response => response.json())
  .then(data => {
    console.log(`Found ${data.count} files`);
    data.files.forEach(file => {
      console.log(`- ${file.name} (${file.size} bytes)`);
    });
  });

// Download a file
fetch('http://localhost:8080/file?path=document.pdf')
  .then(response => response.blob())
  .then(blob => {
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'document.pdf';
    a.click();
  });

// Convert to PDF
fetch('http://localhost:8080/file?path=scan.jpg&type=pdf')
  .then(response => response.blob())
  .then(blob => {
    const url = URL.createObjectURL(blob);
    window.open(url, '_blank');
  });
```

### Python

```python
import requests

# Search for files
response = requests.get('http://localhost:8080/search', params={'q': 'invoice'})
data = response.json()
print(f"Found {data['count']} files")

# Download a file
response = requests.get('http://localhost:8080/file', params={'path': 'document.pdf'})
with open('document.pdf', 'wb') as f:
    f.write(response.content)

# Convert to PDF
response = requests.get('http://localhost:8080/file',
                       params={'path': 'scan.jpg', 'type': 'pdf'})
with open('scan.pdf', 'wb') as f:
    f.write(response.content)
```

### cURL

```bash
# Get server status
curl http://localhost:8080/status

# Search files
curl "http://localhost:8080/search?q=invoice" | jq

# List directory
curl "http://localhost:8080/list?path=2023" | jq

# Download file
curl "http://localhost:8080/file?path=document.pdf" -o document.pdf

# Convert to PDF
curl "http://localhost:8080/file?path=scan.jpg&type=pdf" -o scan.pdf

# Pretty print JSON response
curl -s http://localhost:8080/repos | jq .
```

---

## Security Considerations

### Path Traversal Prevention

The server prevents directory traversal attacks:
- Paths containing `..` are rejected
- Absolute paths starting with `/` are rejected
- All paths are resolved relative to the repository root

### Network Security

For production use, consider:
1. **Firewall**: Restrict access to trusted IPs
2. **Reverse Proxy**: Use nginx/apache with SSL/TLS
3. **Authentication**: Add authentication layer via reverse proxy
4. **Private Network**: Run on private network only

### File Access

- Server runs with limited user permissions
- Only configured repository paths are accessible
- No write operations are supported (read-only API)

---

## Performance

### Response Times

Typical response times on local network:

| Endpoint     | Response Time | Notes                           |
|--------------|---------------|---------------------------------|
| `/status`    | < 1ms         | Cached information              |
| `/repos`     | < 5ms         | Directory metadata              |
| `/search`    | 10-100ms      | Depends on directory size       |
| `/list`      | 5-50ms        | Depends on directory size       |
| `/file`      | 10-500ms      | Depends on file size            |
| PDF convert  | 1-30s         | Depends on file size/complexity |

### Caching

Currently, no caching is implemented. Each request reads from disk. For improved performance, consider:
- Implementing a caching layer
- Using a reverse proxy with caching
- Pre-generating PDFs for frequently accessed files

---

## Troubleshooting

### PDF Conversion Issues

**Problem**: Conversion fails with "Failed to start conversion process"
- **Solution**: Ensure `paperman` binary is in the same directory as `paperman-server`

**Problem**: Conversion times out
- **Solution**: File may be too large or complex. Try with a smaller file or increase timeout in source code.

**Problem**: Conversion returns error
- **Solution**: Check journalctl logs for detailed error messages: `sudo journalctl -u paperman-server -f`

### File Access Issues

**Problem**: "File not found" but file exists
- **Solution**: Check file path is relative to repository root, not absolute

**Problem**: "Invalid file path" error
- **Solution**: Path contains `..` or starts with `/`. Use relative paths only.

---

## Changelog

### Version 1.0 (Current)
- Initial release
- Basic search, list, and file retrieval
- Multi-repository support
- On-the-fly PDF conversion
- Binary file download support
- Security: Path traversal prevention
- CORS enabled for web applications

---

## Support

For issues, feature requests, or contributions:
- **GitHub**: https://github.com/sjg20/paperman
- **Email**: sjg@chromium.org

---

## License

GPL-2 - See LICENSE file for details
