# Sourced by the parts which compile with the toolchain found on PATH.
#
# KDE's build snap, which the kde-neon-6 extension puts first on PATH,
# has a gcc and ld of its own on amd64 which cannot link anything, as
# they lack gcc's start files and libraries (crtbeginS.o, libgcc). Put
# the system's toolchain ahead of them, leaving the rest of the build
# snap, such as its qmake6, where it is
tools=$CRAFT_PART_BUILD/system-tools
mkdir -p "$tools"
for tool in gcc g++ cc c++ cpp ld as ar ranlib nm strip objcopy; do
	if [ -x /usr/bin/$tool ]; then
		ln -sf /usr/bin/$tool "$tools/$tool"
	fi
done
export PATH="$tools:$PATH"
