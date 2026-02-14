# Paperman Server Deployment Guide

**Deploying to:** your production server

## Prerequisites

- SSH access to the production server
- Git repository access on local machine
- Latest code committed to the `serv` branch

> **Important:** Always test the server locally before deploying to production!

## Deployment Steps

### 1. Prepare Local Repository

Ensure all changes are committed and pushed:

```sh
cd ~/paperman
git status
git add .
git commit -m "Your deployment message"
git push origin serv
```

### 2. Connect to the Server

```sh
ssh server
```

### 3. Navigate to Paperman Directory

```sh
cd ~/paperman
```

### 4. Pull Latest Changes

```sh
git fetch origin
git checkout serv
git pull origin serv
```

> **Tip:** Verify you're on the correct branch with `git branch`

### 5. Build the Server

```sh
# Clean previous build (optional but recommended)
make clean

# Build the paperman-server executable
qmake paperman-server.pro
make
```

### 6. Verify Build Success

```sh
# Check if the executable was created
ls -lh paperman-server

# Quick test (optional)
./paperman-server --help
```

### 7. Stop Existing Server

> **Warning:** This will interrupt service to users!

```sh
# Find the running server process
ps aux | grep paperman-server

# Stop it (replace PID with actual process ID)
kill PID

# Or use killall if running under your user
killall paperman-server
```

### 8. Start New Server

```sh
# Start in background with nohup
nohup ./paperman-server > server.log 2>&1 &

# Or if using systemd (if configured)
sudo systemctl restart paperman-server
```

### 9. Verify Server is Running

```sh
# Check process is running
ps aux | grep paperman-server

# Check the log file
tail -f server.log

# Test the API endpoint
curl http://localhost:8080/status
```

If you see a JSON response from the status endpoint, deployment is complete.

## Testing the Deployment

### Test Search Functionality

```sh
curl "http://localhost:8080/search?q=test"
```

### Test File Listing

```sh
curl "http://localhost:8080/list?path=/"
```

## Rollback Procedure

If something goes wrong, you can rollback to a previous version:

```sh
# View recent commits
git log --oneline -10

# Checkout previous version (replace COMMIT_HASH)
git checkout COMMIT_HASH

# Rebuild and restart
make clean
qmake paperman-server.pro
make
killall paperman-server
nohup ./paperman-server > server.log 2>&1 &
```

## Troubleshooting

### Build Fails

- Check for missing dependencies: `qmake --version`
- Ensure Qt5 development packages are installed
- Check for compilation errors in the output

### Server Won't Start

- Check if port 8080 is already in use: `netstat -tuln | grep 8080`
- Review server.log for error messages: `cat server.log`
- Verify file permissions: `chmod +x paperman-server`

### API Returns Errors

- Check if search index exists: `ls -la .paperindex`
- Verify repository path is correct
- Review server logs for specific error messages

## Post-Deployment Checklist

- Server process is running
- Status endpoint returns valid JSON
- Search functionality works
- File listing works
- No errors in server.log
- Test from external client (web browser or app)

## Monitoring

Keep an eye on the server logs after deployment:

```sh
# Follow the log file in real-time
tail -f ~/paperman/server.log

# Check for errors
grep -i error ~/paperman/server.log

# Monitor server resource usage
top | grep paperman-server
```

> **Note:** Consider setting up log rotation for server.log to prevent it
> from growing too large.
