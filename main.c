#include <gst/gst.h>
#include <stdlib.h>

#define GST_CAT_DEFAULT negotiation_test
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);
#define GST_DEFAULT_NAME "negotiation_test"

#define PROCESSING_DATA "processing-data"

static GMainLoop *loop;
static guint error;

static gboolean
timeout_check (gpointer pipeline)
{
  gchar *timeout_file =
      g_strdup_printf ("timeout-%s", GST_OBJECT_NAME (pipeline));

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipeline),
      GST_DEBUG_GRAPH_SHOW_ALL, timeout_file);
  g_free (timeout_file);

  GST_ERROR ("Test timeout on pipeline %s", GST_OBJECT_NAME (pipeline));
  g_atomic_int_set (&error, 1);
  g_main_loop_quit (loop);

  return FALSE;
}

static GstPadProbeReturn
sink_pad_blocked (GstPad * pad, GstPadProbeInfo * info, gpointer sink)
{
  GstCaps *caps;

  if (g_object_get_data (G_OBJECT (pad), PROCESSING_DATA)) {
    GST_DEBUG ("Already processing");
    return GST_PAD_PROBE_PASS;
  }

  GST_DEBUG ("Pad blocked");
  g_object_set_data (G_OBJECT (pad), PROCESSING_DATA, GINT_TO_POINTER (TRUE));

  caps = gst_caps_from_string ("audio/x-raw,rate=40000");
  g_object_set (sink, "caps", caps, NULL);
  gst_caps_unref (caps);

  /* Force reconfiguration */
  gst_pad_push_event (pad, gst_event_new_reconfigure());
  GST_DEBUG ("Pad blocked");

  return GST_PAD_PROBE_REMOVE;
}

static void
bus_message (GstBus * bus, GstMessage * msg, gpointer pipe)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_ERROR:{
      gchar *error_file = g_strdup_printf ("error-%s", GST_OBJECT_NAME (pipe));

      GST_ERROR ("Error: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, error_file);
      g_free (error_file);

      GST_ERROR ("Error received on bus in pipeline: %s", GST_OBJECT_NAME (pipe));
      g_atomic_int_set (&error, 1);
      g_main_loop_quit (loop);
      break;
    }
    case GST_MESSAGE_WARNING:{
      gchar *warn_file = g_strdup_printf ("warning-%s", GST_OBJECT_NAME (pipe));

      GST_WARNING ("Warning: %" GST_PTR_FORMAT, msg);
      GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (pipe),
          GST_DEBUG_GRAPH_SHOW_ALL, warn_file);
      g_free (warn_file);
      break;
    }
    case GST_MESSAGE_EOS:
      GST_INFO ("Received eos event");
      g_main_loop_quit (loop);
      break;
    case GST_MESSAGE_STREAM_START:{
      GstElement *sink;
      GstPad *sink_pad;

      GST_INFO ("Stream start");
      sink = gst_bin_get_by_name (GST_BIN (pipe), "sink");
      sink_pad = gst_element_get_static_pad (sink, "sink");

      gst_pad_add_probe (sink_pad, GST_PAD_PROBE_TYPE_BLOCK, sink_pad_blocked, g_object_ref (sink), g_object_unref);

      g_object_unref (sink_pad);
      g_object_unref (sink);
      break;
    }
    default:
      break;
  }
}

static GstFlowReturn
new_sample (GstElement * appsink, gpointer target_caps)
{
  GstSample *sample;

  g_signal_emit_by_name (appsink, "pull-sample", &sample);

  GST_DEBUG ("Caps %" GST_PTR_FORMAT, gst_sample_get_caps (sample));
  if (gst_caps_is_always_compatible (gst_sample_get_caps (sample),
          GST_CAPS_CAST (target_caps))) {
    gst_sample_unref (sample);
    return GST_FLOW_EOS;
  } else {
    gst_sample_unref (sample);
    return GST_FLOW_OK;
  }
}

static void
execute_test (int count)
{
  guint timeout_id;
  gchar *name = g_strdup_printf ("negotiation_test_%d", count);
  GstElement *pipeline = gst_pipeline_new (name);
  GstElement *audiotestsrc = gst_element_factory_make ("audiotestsrc", NULL);
//   GstElement *queue = gst_element_factory_make ("queue", NULL);
  GstElement *sink = gst_element_factory_make ("appsink", "sink");
  GstCaps *caps = gst_caps_from_string ("audio/x-raw,rate=3000");

  GstBus *bus = gst_pipeline_get_bus (GST_PIPELINE (pipeline));

  g_free (name);

  if (bus == NULL) {
    GST_ERROR ("Bus is NULL");
    g_atomic_int_set (&error, 1);
    return;
  }

  gst_bus_add_signal_watch (bus);
  g_signal_connect (bus, "message", G_CALLBACK (bus_message), pipeline);

  g_object_set (G_OBJECT (sink), "emit-signals", TRUE, "sync", FALSE, NULL);
  g_signal_connect_data (G_OBJECT (sink), "new-sample", G_CALLBACK (new_sample),
      gst_caps_from_string ("audio/x-raw,rate=40000"),
      (GClosureNotify) gst_caps_unref, 0);

  g_object_set (sink, "caps", caps, NULL);
  gst_caps_unref (caps);

  gst_bin_add_many (GST_BIN (pipeline), audiotestsrc, /*queue, */sink, NULL);
  gst_element_link_many (audiotestsrc, /*queue, */sink, NULL);

  gst_element_set_state (pipeline, GST_STATE_PLAYING);

  timeout_id = g_timeout_add_seconds (5, timeout_check, pipeline);

  g_main_loop_run (loop);

  if (!g_source_remove (timeout_id)) {
    GST_ERROR ("Error removing source");
    g_atomic_int_set (&error, 1);
    return;
  }

  gst_element_set_state (pipeline, GST_STATE_NULL);
  gst_bus_remove_signal_watch (bus);
  g_object_unref (bus);
  g_object_unref (pipeline);
}

int
main(int argc, char ** argv)
{
  int count = 1;

  error = 0;

  gst_init (&argc, &argv);

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, GST_DEFAULT_NAME, 0,
      GST_DEFAULT_NAME);

  loop = g_main_loop_new (NULL, TRUE);

  while (count > 0 && !g_atomic_int_get (&error)) {
    GST_INFO ("Executing %d times", count);
    execute_test (count++);
  }

  g_main_loop_unref (loop);

  if (g_atomic_int_get (&error)) {
    GST_ERROR ("Test terminated with error");
  } else {
    GST_INFO ("Test terminated correctly");
  }

  return error;
}