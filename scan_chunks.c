/*
 * scan_chunks.c - SANE chunk-timing diagnostic
 *
 * Performs a single SANE scan and prints, for every sane_read() return,
 * the wall-clock time, byte count, and instantaneous rate. Useful for
 * answering: how does the backend pace data delivery during a scan?
 *
 * Build:
 *   gcc -O2 -Wall scan_chunks.c -o scan_chunks -lsane -ldl
 *
 * Usage:
 *   ./scan_chunks [--device NAME] [--resolution DPI] [--mode MODE]
 *                 [--source SOURCE] [--buffermode on|off]
 *                 [--buffer BYTES] [--bsize BYTES] [--duplex]
 *
 * --buffer is the maxlen passed to sane_read(); --bsize sets the
 * backend-side runtime "buffer-size" SANE option (patched fujitsu
 * backend only). --duplex switches to the ADF Duplex source and
 * exercises sane_read_dup() if libsane provides it (loaded via dlsym
 * so the binary still runs against an unpatched libsane).
 */

#include <sane/sane.h>
#include <sane/saneopts.h>

#include <dlfcn.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Optional SANE extension exposed by the patched local libsane. */
typedef SANE_Status (*sane_read_dup_fn)(SANE_Handle, SANE_Byte *, SANE_Byte *,
                                        SANE_Int, SANE_Int *, SANE_Int *);

static int64_t now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int find_option(SANE_Handle h, const char *name)
{
   const SANE_Option_Descriptor *opt;
   int i;

   for (i = 1; (opt = sane_get_option_descriptor(h, i)) != NULL; i++) {
      if (opt->name && strcmp(opt->name, name) == 0)
         return i;
   }
   return -1;
}

static void set_int_opt(SANE_Handle h, const char *name, int value)
{
   int idx = find_option(h, name);
   const SANE_Option_Descriptor *opt;
   SANE_Status st;
   SANE_Int info = 0;

   if (idx < 0) {
      printf("# (option %s not present)\n", name);
      return;
   }
   opt = sane_get_option_descriptor(h, idx);
   if (opt->type == SANE_TYPE_FIXED) {
      SANE_Fixed v = SANE_FIX(value);
      st = sane_control_option(h, idx, SANE_ACTION_SET_VALUE, &v, &info);
   } else {
      SANE_Int v = value;
      st = sane_control_option(h, idx, SANE_ACTION_SET_VALUE, &v, &info);
   }
   if (st != SANE_STATUS_GOOD)
      fprintf(stderr, "warning: set %s=%d: %s\n", name, value,
              sane_strstatus(st));
   else
      printf("# set %s = %d\n", name, value);
}

static void set_str_opt(SANE_Handle h, const char *name, const char *value)
{
   int idx = find_option(h, name);
   const SANE_Option_Descriptor *opt;
   SANE_Status st;
   SANE_Int info = 0;
   char *buf;

   if (idx < 0) {
      printf("# (option %s not present)\n", name);
      return;
   }
   opt = sane_get_option_descriptor(h, idx);
   buf = calloc(1, opt->size);
   if (!buf) {
      fprintf(stderr, "oom\n");
      exit(1);
   }
   strncpy(buf, value, opt->size - 1);
   st = sane_control_option(h, idx, SANE_ACTION_SET_VALUE, buf, &info);
   if (st != SANE_STATUS_GOOD)
      fprintf(stderr, "warning: set %s=%s: %s\n", name, value,
              sane_strstatus(st));
   else
      printf("# set %s = %s\n", name, value);
   free(buf);
}

static void usage(const char *argv0)
{
   fprintf(stderr,
      "Usage: %s [--device NAME] [--resolution DPI] [--mode MODE]\n"
      "          [--source SOURCE] [--buffermode on|off]\n"
      "          [--buffer BYTES] [--bsize BYTES] [--duplex]\n",
      argv0);
}

/* Run the standard sane_read loop for one side. */
static int simplex_loop(SANE_Handle h, SANE_Byte *buf, int read_buf)
{
   int64_t t0 = now_ms();
   int64_t t_prev = t0;
   long total = 0;
   int chunks = 0;

   printf("\n%-5s %10s %10s %12s %12s\n",
          "chunk", "t_ms", "bytes", "cum_bytes", "B/s_inst");
   for (;;) {
      SANE_Int got = 0;
      int64_t t_now;
      double dt;
      long inst;
      SANE_Status st = sane_read(h, buf, read_buf, &got);
      t_now = now_ms();
      if (st == SANE_STATUS_GOOD) {
         total += got;
         chunks++;
         dt = (t_now - t_prev) / 1000.0;
         inst = dt > 0 ? (long)(got / dt) : 0;
         printf("%5d %10ld %10d %12ld %12ld\n", chunks, (long)(t_now - t0),
                got, total, inst);
         fflush(stdout);
         t_prev = t_now;
      } else if (st == SANE_STATUS_EOF) {
         printf("# EOF after %ld ms\n", (long)(t_now - t0));
         break;
      } else {
         fprintf(stderr, "sane_read: %s\n", sane_strstatus(st));
         return -1;
      }
   }
   {
      double secs = (now_ms() - t0) / 1000.0;
      printf("\n# summary: chunks=%d total=%ld bytes time=%.3fs avg=%ld B/s\n",
             chunks, total, secs,
             secs > 0 ? (long)(total / secs) : 0L);
   }
   return 0;
}

/* Run the sane_read_dup loop for both sides simultaneously. */
static int duplex_loop(SANE_Handle h, sane_read_dup_fn read_dup,
                       SANE_Byte *fbuf, SANE_Byte *bbuf, int read_buf)
{
   int64_t t0 = now_ms();
   long ftot = 0, btot = 0;
   int calls = 0, fcount = 0, bcount = 0;
   long ffirst_ms = -1, bfirst_ms = -1;
   long flast_ms = 0, blast_ms = 0;

   printf("\n%-5s %10s %10s %10s %12s %12s\n",
          "call", "t_ms", "front_B", "back_B", "front_cum", "back_cum");
   for (;;) {
      SANE_Int fgot = 0, bgot = 0;
      int64_t t_now;
      SANE_Status st = read_dup(h, fbuf, bbuf, read_buf, &fgot, &bgot);
      t_now = now_ms();
      calls++;
      if (st == SANE_STATUS_GOOD) {
         if (fgot) {
            if (ffirst_ms < 0) ffirst_ms = t_now - t0;
            flast_ms = t_now - t0;
            ftot += fgot;
            fcount++;
         }
         if (bgot) {
            if (bfirst_ms < 0) bfirst_ms = t_now - t0;
            blast_ms = t_now - t0;
            btot += bgot;
            bcount++;
         }
         if (fgot || bgot)
            printf("%5d %10ld %10d %10d %12ld %12ld\n", calls,
                   (long)(t_now - t0), fgot, bgot, ftot, btot);
         fflush(stdout);
      } else if (st == SANE_STATUS_EOF) {
         printf("# EOF after %ld ms\n", (long)(t_now - t0));
         break;
      } else {
         fprintf(stderr, "sane_read_dup: %s\n", sane_strstatus(st));
         return -1;
      }
   }
   {
      double secs = (now_ms() - t0) / 1000.0;
      printf("\n# summary: calls=%d front=%ld bytes (%d chunks, t %ld..%ld ms)\n",
             calls, ftot, fcount, ffirst_ms, flast_ms);
      printf("#                    back=%ld bytes (%d chunks, t %ld..%ld ms)\n",
             btot, bcount, bfirst_ms, blast_ms);
      printf("# wall time=%.3fs avg=%ld B/s\n",
             secs, secs > 0 ? (long)((ftot + btot) / secs) : 0L);
   }
   return 0;
}

int main(int argc, char **argv)
{
   const char *device = NULL;
   const char *mode = NULL;
   const char *source = NULL;
   const char *buffermode = NULL;
   int resolution = 200;
   int read_buf = 32768;
   int bsize = 0;
   int duplex = 0;
   SANE_Int version = 0;
   SANE_Parameters par;
   SANE_Handle h;
   SANE_Status st;
   SANE_Byte *buf, *buf_back = NULL;
   int64_t t_open, t_started;
   sane_read_dup_fn read_dup = NULL;
   int c;

   static struct option opts[] = {
      {"device",     required_argument, 0, 'd'},
      {"resolution", required_argument, 0, 'r'},
      {"mode",       required_argument, 0, 'm'},
      {"source",     required_argument, 0, 's'},
      {"buffermode", required_argument, 0, 'b'},
      {"buffer",     required_argument, 0, 'B'},
      {"bsize",      required_argument, 0, 'S'},
      {"duplex",     no_argument,       0, 'D'},
      {"help",       no_argument,       0, 'h'},
      {0, 0, 0, 0}
   };

   while ((c = getopt_long(argc, argv, "d:r:m:s:b:B:S:Dh", opts, NULL)) != -1) {
      switch (c) {
      case 'd': device = optarg; break;
      case 'r': resolution = atoi(optarg); break;
      case 'm': mode = optarg; break;
      case 's': source = optarg; break;
      case 'b': buffermode = optarg; break;
      case 'B': read_buf = atoi(optarg); break;
      case 'S': bsize = atoi(optarg); break;
      case 'D': duplex = 1; break;
      case 'h': usage(argv[0]); return 0;
      default:  usage(argv[0]); return 1;
      }
   }
   if (duplex && !source) source = "ADF Duplex";
   if (read_buf <= 0) {
      fprintf(stderr, "--buffer must be positive\n");
      return 1;
   }

   st = sane_init(&version, NULL);
   if (st != SANE_STATUS_GOOD) {
      fprintf(stderr, "sane_init: %s\n", sane_strstatus(st));
      return 1;
   }
   printf("# SANE version 0x%x\n", version);

   if (!device) {
      const SANE_Device **list = NULL;
      st = sane_get_devices(&list, SANE_TRUE);
      if (st != SANE_STATUS_GOOD || !list || !list[0]) {
         fprintf(stderr, "no devices found: %s\n",
                 st == SANE_STATUS_GOOD ? "(empty list)" : sane_strstatus(st));
         sane_exit();
         return 1;
      }
      device = strdup(list[0]->name);
      printf("# device (auto): %s [%s %s, %s]\n", list[0]->name,
             list[0]->vendor, list[0]->model, list[0]->type);
   } else {
      printf("# device: %s\n", device);
   }

   t_open = now_ms();
   st = sane_open(device, &h);
   printf("# sane_open: %s in %ld ms\n", sane_strstatus(st),
          (long)(now_ms() - t_open));
   if (st != SANE_STATUS_GOOD) {
      sane_exit();
      return 1;
   }

   if (mode)       set_str_opt(h, SANE_NAME_SCAN_MODE, mode);
   if (source)     set_str_opt(h, SANE_NAME_SCAN_SOURCE, source);
   if (resolution) set_int_opt(h, SANE_NAME_SCAN_RESOLUTION, resolution);
   if (buffermode) set_str_opt(h, "buffermode", buffermode);
   if (bsize)      set_int_opt(h, "buffer-size", bsize);

   if (duplex) {
      /* RTLD_DEFAULT: look up sane_read_dup in already-loaded libsane. */
      read_dup = (sane_read_dup_fn) dlsym(RTLD_DEFAULT, "sane_read_dup");
      if (!read_dup) {
         fprintf(stderr,
                 "warning: libsane has no sane_read_dup; falling back to "
                 "sequential simplex reads of the front side only.\n");
      } else {
         printf("# duplex via sane_read_dup\n");
      }
   }

   st = sane_get_parameters(h, &par);
   if (st == SANE_STATUS_GOOD) {
      printf("# params: format=%d depth=%d ppl=%d lines=%d bpl=%d total=%ld\n",
             par.format, par.depth, par.pixels_per_line, par.lines,
             par.bytes_per_line,
             par.lines > 0 ? (long)par.lines * par.bytes_per_line : -1L);
   } else {
      printf("# sane_get_parameters: %s\n", sane_strstatus(st));
   }
   printf("# read_buf = %d bytes\n", read_buf);

   buf = malloc(read_buf);
   if (duplex && read_dup) buf_back = malloc(read_buf);
   if (!buf || (duplex && read_dup && !buf_back)) {
      fprintf(stderr, "oom\n");
      sane_close(h);
      sane_exit();
      return 1;
   }

   t_open = now_ms();
   st = sane_start(h);
   t_started = now_ms();
   printf("# sane_start: %s in %ld ms\n", sane_strstatus(st),
          (long)(t_started - t_open));
   if (st != SANE_STATUS_GOOD) {
      free(buf); free(buf_back);
      sane_close(h);
      sane_exit();
      return 1;
   }

   if (duplex && read_dup)
      duplex_loop(h, read_dup, buf, buf_back, read_buf);
   else
      simplex_loop(h, buf, read_buf);

   sane_cancel(h);
   sane_close(h);
   sane_exit();
   free(buf);
   free(buf_back);
   return 0;
}
