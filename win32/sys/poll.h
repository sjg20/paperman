/*
 * Minimal poll() for Windows
 *
 * The scanner code polls the SANE select file descriptor while waiting for
 * data. The Windows build has no libsane, so sane_get_select_fd() never
 * succeeds and the poll loop is never entered; this only needs to compile.
 */

#ifndef WIN32_SYS_POLL_H
#define WIN32_SYS_POLL_H

#include <windows.h>

#define POLLIN   0x0001
#define POLLNVAL 0x0020

struct pollfd
   {
   int fd;
   short events;
   short revents;
   };

static inline int poll (struct pollfd *fds, unsigned int nfds, int timeout)
   {
   for (unsigned int i = 0; i < nfds; i++)
      fds [i].revents = POLLNVAL;
   if (timeout > 0)
      Sleep (timeout);
   return (int)nfds;
   }

#endif
