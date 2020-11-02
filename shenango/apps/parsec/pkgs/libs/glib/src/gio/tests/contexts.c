#include <gio/gio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_FILE (SRCDIR "/Makefile.am")
char *test_file_buffer;
gsize test_file_size;
static char async_read_buffer[8192];

static void
read_data (GObject *source, GAsyncResult *result, gpointer loop)
{
  GInputStream *in = G_INPUT_STREAM (source);
  GError *error = NULL;
  gssize nread;

  nread = g_input_stream_read_finish (in, result, &error);
  g_assert_no_error (error);

  g_assert_cmpint (nread, >, 0);
  g_assert_cmpint (nread, <=, MIN(sizeof (async_read_buffer), test_file_size));
  g_assert (memcmp (async_read_buffer, test_file_buffer, nread) == 0);

  g_main_loop_quit (loop);
}

static void
opened_for_read (GObject *source, GAsyncResult *result, gpointer loop)
{
  GFile *file = G_FILE (source);
  GFileInputStream *in;
  GError *error = NULL;

  in = g_file_read_finish (file, result, &error);
  g_assert_no_error (error);

  memset (async_read_buffer, 0, sizeof (async_read_buffer));
  g_input_stream_read_async (G_INPUT_STREAM (in),
			     async_read_buffer, sizeof (async_read_buffer),
			     G_PRIORITY_DEFAULT, NULL,
			     read_data, loop);
}

/* Test 1: Async I/O started in a thread with a thread-default context
 * will stick to that thread, and will complete even if the default
 * main loop is blocked. (NB: the last part would not be true if we
 * were testing GFileMonitor!)
 */

static gboolean idle_start_test1_thread (gpointer loop);
static gpointer test1_thread (gpointer user_data);

static GCond *test1_cond;
static GMutex *test1_mutex;

static void
test_thread_independence (void)
{
  GMainLoop *loop;

  test1_cond = g_cond_new ();
  test1_mutex = g_mutex_new ();

  loop = g_main_loop_new (NULL, FALSE);
  g_idle_add (idle_start_test1_thread, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_mutex_free (test1_mutex);
  g_cond_free (test1_cond);
}

static gboolean
idle_start_test1_thread (gpointer loop)
{
  GTimeVal time;
  GThread *thread;
  gboolean io_completed;

  g_mutex_lock (test1_mutex);
  thread = g_thread_create (test1_thread, NULL, TRUE, NULL);

  g_get_current_time (&time);
  time.tv_sec += 2;
  io_completed = g_cond_timed_wait (test1_cond, test1_mutex, &time);
  g_assert (io_completed);
  g_thread_join (thread);

  g_mutex_unlock (test1_mutex);
  g_main_loop_quit (loop);
  return FALSE;
}

static gpointer
test1_thread (gpointer user_data)
{
  GMainContext *context;
  GMainLoop *loop;
  GFile *file;

  /* Wait for main thread to be waiting on test1_cond */
  g_mutex_lock (test1_mutex);
  g_mutex_unlock (test1_mutex);

  context = g_main_context_new ();
  g_assert (g_main_context_get_thread_default () == NULL);
  g_main_context_push_thread_default (context);
  g_assert (g_main_context_get_thread_default () == context);

  file = g_file_new_for_path (TEST_FILE);
  g_assert (g_file_supports_thread_contexts (file));

  loop = g_main_loop_new (context, FALSE);
  g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
		     opened_for_read, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_cond_signal (test1_cond);
  return NULL;
}

/* Test 2: If we push a thread-default context in the main thread, we
 * can run async ops in that context without running the default
 * context.
 */

static gboolean test2_fail (gpointer user_data);

static void
test_context_independence (void)
{
  GMainContext *context;
  GMainLoop *loop;
  GFile *file;
  guint default_timeout;
  GSource *thread_default_timeout;

  context = g_main_context_new ();
  g_assert (g_main_context_get_thread_default () == NULL);
  g_main_context_push_thread_default (context);
  g_assert (g_main_context_get_thread_default () == context);

  file = g_file_new_for_path (TEST_FILE);
  g_assert (g_file_supports_thread_contexts (file));

  /* Add a timeout to the main loop, to fail immediately if it gets run */
  default_timeout = g_timeout_add_full (G_PRIORITY_HIGH, 0,
					test2_fail, NULL, NULL);
  /* Add a timeout to the alternate loop, to fail if the I/O *doesn't* run */
  thread_default_timeout = g_timeout_source_new_seconds (2);
  g_source_set_callback (thread_default_timeout, test2_fail, NULL, NULL);
  g_source_attach (thread_default_timeout, context);

  loop = g_main_loop_new (context, FALSE);
  g_file_read_async (file, G_PRIORITY_DEFAULT, NULL,
		     opened_for_read, loop);
  g_main_loop_run (loop);
  g_main_loop_unref (loop);

  g_source_remove (default_timeout);
  g_source_destroy (thread_default_timeout);
  g_source_unref (thread_default_timeout);
}

static gboolean
test2_fail (gpointer user_data)
{
  g_assert_not_reached ();
  return FALSE;
}

int
main (int argc, char **argv)
{
  GError *error = NULL;

  g_thread_init (NULL);
  g_type_init ();
  g_test_init (&argc, &argv, NULL);

  g_file_get_contents (TEST_FILE, &test_file_buffer,
		       &test_file_size, &error);
  g_assert_no_error (error);

  g_test_add_func ("/gio/contexts/thread-independence", test_thread_independence);
  g_test_add_func ("/gio/contexts/context-independence", test_context_independence);

  return g_test_run();
}
