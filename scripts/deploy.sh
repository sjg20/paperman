#!/bin/bash
# Deploy paperman and paperman-server to a remote host
#
# Pushes the current dev branch, rebuilds on the remote, and restarts the
# service.

set -e

if [ -z "$1" ]; then
	echo "Usage: $0 <host>"
	echo "Example: $0 fred@my-server"
	exit 1
fi

HOST=$1
REMOTE_DIR=paperman
INSTALL_DIR=/opt/paperman
SERVICE=paperman-server.service

echo "Pushing dev to $HOST..."
git push --force "$HOST:$REMOTE_DIR" dev:master

echo "Building on $HOST..."
ssh "$HOST" "cd ~/$REMOTE_DIR \
	&& make -f GNUmakefile builddate.h \
	&& qmake paperman-server.pro -o Makefile.server \
	&& make -j -f Makefile.server \
	&& qmake paperman.pro -o Makefile \
	&& make -j -f Makefile"

echo "Installing..."
ssh "$HOST" "sudo systemctl stop $SERVICE \
	&& sudo cp ~/$REMOTE_DIR/paperman-server \
		~/$REMOTE_DIR/paperman $INSTALL_DIR/ \
	&& sudo systemctl start $SERVICE"

echo "Checking service..."
ssh "$HOST" "sudo systemctl status $SERVICE --no-pager"

echo "Done."
