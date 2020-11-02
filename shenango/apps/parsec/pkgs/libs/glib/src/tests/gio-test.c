/* GLIB - Library of useful routines for C programming
 * Copyright (C) 2000  Tor Lillqvist
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/* A test program for the main loop and IO channel code.
 * Just run it. Optional parameter is number of sub-processes.
 */

#undef G_DISABLE_ASSERT
#undef G_LOG_DOMAIN

#include "config.h"

#include <glib.h>

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

#ifdef G_OS_WIN32
  #include <io.h>
  #include <fcntl.h>
  #include <process.h>
  #define STRICT
  #include <windows.h>
  #define pipe(fds) _pipe(fds, 4096, _O_BINARY)
#else
  #ifdef HAVE_UNISTD_H
    #include <unistd.h>
  #endif
#endif

static int nrunning;
static GMainLoop *main_loop;

#define BUFSIZE 5000		/* Larger than the circular buffer in
				 * giowin32.c on purpose.
				 */

static int nkiddies;

static struct {
  int fd;
  int seq;
} *seqtab;

static GIOError
read_all (int         fd,
	  GIOChannel *channel,
	  char       *buffer,
	  guint       nbytes,
	  guint      *bytes_read)
{
  guint left = nbytes;
  gsize nb;
  GIOError error = G_IO_ERROR_NONE;
  char *bufp = buffer;

  /* g_io_channel_read() doesn't necessarily return all the
   * data we want at once.
   */
  *bytes_read = 0;
  while (left)
    {
      error = g_io_channel_read (channel, bufp, left, &nb);
      
      if (error != G_IO_ERROR_NONE)
	{
	  g_print ("gio-test: ...from %d: G_IO_ERROR_%s\n", fd,
		   (error == G_IO_ERROR_AGAIN ? "AGAIN" :
		    (error == G_IO_ERROR_INVAL ? "INVAL" :
		     (error == G_IO_ERROR_UNKNOWN ? "UNKNOWN" : "???"))));
	  if (error == G_IO_ERROR_AGAIN)
	    continue;
	  break;
	}
      if (nb == 0)
	return error;
      left -= nb;
      bufp += nb;
      *bytes_read += nb;
    }
  return error;
}

static void
shutdown_source (gpointer data)
{
  if (g_source_remove (*(guint *) data))
    {
      nrunning--;
      if (nrunning == 0)
	g_main_loop_quit (main_loop);
    }
}

static gboolean
recv_message (GIOChannel  *channel,
	      GIOCondition cond,
	      gpointer    data)
{
  gint fd = g_io_channel_unix_get_fd (channel);
  gboolean retval = TRUE;

#ifdef VERBOSE
  g_print ("gio-test: ...from %d:%s%s%s%s\n", fd,
	   (cond & G_IO_ERR) ? " ERR" : "",
	   (cond & G_IO_HUP) ? " HUP" : "",
	   (cond & G_IO_IN)  ? " IN"  : "",
	   (cond & G_IO_PRI) ? " PRI" : "");
#endif

  if (cond & (G_IO_ERR | G_IO_HUP))
    {
      shutdown_source (data);
      retval = FALSE;
    }

  if (cond & G_IO_IN)
    {
      char buf[BUFSIZE];
      guint nbytes;
      guint nb;
      int i, j, seq;
      GIOError error;
      
      error = read_all (fd, channel, (gchar *) &seq, sizeof (seq), &nb);
      if (error == G_IO_ERROR_NONE)
	{
	  if (nb == 0)
	    {
#ifdef VERBOSE
	      g_print ("gio-test: ...from %d: EOF\n", fd);
#endif
	      shutdown_source (data);
	      return FALSE;
	    }
	  
	  g_assert (nb == sizeof (nbytes));

	  for (i = 0; i < nkiddies; i++)
	    if (seqtab[i].fd == fd)
	      {
		if (seq != seqtab[i].seq)
		  {
		    g_print ("gio-test: ...from %d: invalid sequence number %d, expected %d\n",
			     fd, seq, seqtab[i].seq);
		    g_assert_not_reached ();
		  }
		seqtab[i].seq++;
		break;
	      }

	  error = read_all (fd, channel, (gchar *) &nbytes, sizeof (nbytes), &nb);
	}

      if (error != G_IO_ERROR_NONE)
	return FALSE;
      
      if (nb == 0)
	{
#ifdef VERBOSE
	  g_print ("gio-test: ...from %d: EOF\n", fd);
#endif
	  shutdown_source (data);
	  return FALSE;
	}
      
      g_assert (nb == sizeof (nbytes));

      if (nbytes >= BUFSIZE)
	{
	  g_print ("gio-test: ...from %d: nbytes = %d (%#x)!\n", fd, nbytes, nbytes);
	  g_assert_not_reached ();
	}
      g_assert (nbytes >= 0 && nbytes < BUFSIZE);
#ifdef VERBOSE      
      g_print ("gio-test: ...from %d: %d bytes\n", fd, nbytes);
#endif      
      if (nbytes > 0)
	{
	  error = read_all (fd, channel, buf, nbytes, &nb);

	  if (error != G_IO_ERROR_NONE)
	    return FALSE;

	  if (nb == 0)
	    {
#ifdef VERBOSE
	      g_print ("gio-test: ...from %d: EOF\n", fd);
#endif
	      shutdown_source (data);
	      return FALSE;
	    }
      
	  for (j = 0; j < nbytes; j++)
	    if (buf[j] != ' ' + ((nbytes + j) % 95))
	      {
		g_print ("gio-test: ...from %d: buf[%d] == '%c', should be '%c'\n",
			 fd, j, buf[j], 'a' + ((nbytes + j) % 32));
		g_assert_not_reached ();
	      }
#ifdef VERBOSE
	  g_print ("gio-test: ...from %d: OK\n", fd);
#endif
	}
    }
  return retval;
}

#ifdef G_OS_WIN32

static gboolean
recv_windows_message (GIOChannel  *channel,
		      GIOCondition cond,
		      gpointer    data)
{
  GIOError error;
  MSG msg;
  guint nb;
  
  while (1)
    {
      error = g_io_channel_read (channel, &msg, sizeof (MSG), &nb);
      
      if (error != G_IO_ERROR_NONE)
	{
	  g_print ("gio-test: ...reading Windows message: G_IO_ERROR_%s\n",
		   (error == G_IO_ERROR_AGAIN ? "AGAIN" :
		    (error == G_IO_ERROR_INVAL ? "INVAL" :
		     (error == G_IO_ERROR_UNKNOWN ? "UNKNOWN" : "???"))));
	  if (error == G_IO_ERROR_AGAIN)
	    continue;
	}
      break;
    }

  g_print ("gio-test: ...Windows message for %#x: %d,%d,%d\n",
	   msg.hwnd, msg.message, msg.wParam, msg.lParam);

  return TRUE;
}

LRESULT CALLBACK 
window_procedure (HWND hwnd,
		  UINT message,
		  WPARAM wparam,
		  LPARAM lparam)
{
  g_print ("gio-test: window_procedure for %#x: %d,%d,%d\n",
	   hwnd, message, wparam, lparam);
  return DefWindowProc (hwnd, message, wparam, lparam);
}

#endif

int
main (int    argc,
      char **argv)
{
  if (argc < 3)
    {
      /* Parent */
      
      GIOChannel *my_read_channel;
      gchar *cmdline;
      guint *id;
      int i;
#ifdef G_OS_WIN32
      GTimeVal start, end;
      GPollFD pollfd;
      int pollresult;
      ATOM klass;
      static WNDCLASS wcl;
      HWND hwnd;
      GIOChannel *windows_messages_channel;
#endif

      nkiddies = (argc == 1 ? 1 : atoi(argv[1]));
      seqtab = g_malloc (nkiddies * 2 * sizeof (int));

#ifdef G_OS_WIN32
      wcl.style = 0;
      wcl.lpfnWndProc = window_procedure;
      wcl.cbClsExtra = 0;
      wcl.cbWndExtra = 0;
      wcl.hInstance = GetModuleHandle (NULL);
      wcl.hIcon = NULL;
      wcl.hCursor = NULL;
      wcl.hbrBackground = NULL;
      wcl.lpszMenuName = NULL;
      wcl.lpszClassName = "gio-test";

      klass = RegisterClass (&wcl);

      if (!klass)
	{
	  g_print ("gio-test: RegisterClass failed\n");
	  exit (1);
	}

      hwnd = CreateWindow (MAKEINTATOM(klass), "gio-test", 0, 0, 0, 10, 10,
			   NULL, NULL, wcl.hInstance, NULL);
      if (!hwnd)
	{
	  g_print ("gio-test: CreateWindow failed\n");
	  exit (1);
	}

      windows_messages_channel = g_io_channel_win32_new_messages ((guint)hwnd);
      g_io_add_watch (windows_messages_channel, G_IO_IN, recv_windows_message, 0);
#endif

      for (i = 0; i < nkiddies; i++)
	{
	  int pipe_to_sub[2], pipe_from_sub[2];
	  
	  if (pipe (pipe_to_sub) == -1 ||
	      pipe (pipe_from_sub) == -1)
	    perror ("pipe"), exit (1);
	  
	  seqtab[i].fd = pipe_from_sub[0];
	  seqtab[i].seq = 0;

	  my_read_channel = g_io_channel_unix_new (pipe_from_sub[0]);
	  
	  id = g_new (guint, 1);
	  *id =
	    g_io_add_watch (my_read_channel,
			    G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP,
			    recv_message,
			    id);
	  
	  nrunning++;
	  
#ifdef G_OS_WIN32
	  cmdline = g_strdup_printf ("%d:%d:%d",
				     pipe_to_sub[0],
				     pipe_from_sub[1],
				     hwnd);
	  _spawnl (_P_NOWAIT, argv[0], argv[0], "--child", cmdline, NULL);
#else
	  cmdline = g_strdup_printf ("%s --child %d:%d &", argv[0],
				     pipe_to_sub[0], pipe_from_sub[1]);
	  
	  system (cmdline);
#endif
	  close (pipe_to_sub[0]);
	  close (pipe_from_sub [1]);

#ifdef G_OS_WIN32
	  g_get_current_time (&start);
	  g_io_channel_win32_make_pollfd (my_read_channel, G_IO_IN, &pollfd);
	  pollresult = g_io_channel_win32_poll (&pollfd, 1, 100);
	  g_get_current_time (&end);
	  if (end.tv_usec < start.tv_usec)
	    end.tv_sec--, end.tv_usec += 1000000;
	  g_print ("gio-test: had to wait %ld.%03ld s, result:%d\n",
		   end.tv_sec - start.tv_sec,
		   (end.tv_usec - start.tv_usec) / 1000,
		   pollresult);
#endif
	}
      
      main_loop = g_main_loop_new (NULL, FALSE);
      
      g_main_loop_run (main_loop);
    }
  else if (argc == 3)
    {
      /* Child */
      
      int readfd, writefd;
#ifdef G_OS_WIN32
      HWND hwnd;
#endif
      int i, j;
      char buf[BUFSIZE];
      int buflen;
      GTimeVal tv;
      int n;
  
      g_get_current_time (&tv);
      
      sscanf (argv[2], "%d:%d%n", &readfd, &writefd, &n);

#ifdef G_OS_WIN32
      sscanf (argv[2] + n, ":%d", &hwnd);
#endif
      
      srand (tv.tv_sec ^ (tv.tv_usec / 1000) ^ readfd ^ (writefd << 4));
  
      for (i = 0; i < 20 + rand() % 20; i++)
	{
	  g_usleep (100 + (rand() % 10) * 5000);
	  buflen = rand() % BUFSIZE;
	  for (j = 0; j < buflen; j++)
	    buf[j] = ' ' + ((buflen + j) % 95);
#ifdef VERBOSE
	  g_print ("gio-test: child writing %d+%d bytes to %d\n",
		   (int)(sizeof(i) + sizeof(buflen)), buflen, writefd);
#endif
	  write (writefd, &i, sizeof (i));
	  write (writefd, &buflen, sizeof (buflen));
	  write (writefd, buf, buflen);

#ifdef G_OS_WIN32
	  if (rand() % 100 < 5)
	    {
	      int msg = WM_USER + (rand() % 100);
	      WPARAM wparam = rand ();
	      LPARAM lparam = rand ();
	      g_print ("gio-test: child posting message %d,%d,%d to %#x\n",
		       msg, wparam, lparam, hwnd);
	      PostMessage (hwnd, msg, wparam, lparam);
	    }
#endif
	}
#ifdef VERBOSE
      g_print ("gio-test: child exiting, closing %d\n", writefd);
#endif
      close (writefd);
    }
  else
    g_print ("Huh?\n");
  
  return 0;
}

