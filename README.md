negotiation_test
================

  This test creates a simple pipeline and forces caps renegotiation once the
  GST_MESSAGE_STREAM_START is received on bus. It terminates correctly if the
  appsink receives a buffer with the renegotiated format.

  This program runs a test continuously until it fails or it is executed a
  number of times selected by the used with -n option (1000000 by default).

  If option -q is given, the pipeline is this:

```
    --------------      -------      ---------
   | audiotestsrc | -> | queue | -> | appsink |
    --------------      -------      ---------
```

  This test frequently fails  (1 on 15?), because audiotestsrc receive a
  not negotiated error while pushing a buffer or because no buffer with
  the new format is received.

   If -q option is not present the pipeline is this:

```
    --------------      ---------
   | audiotestsrc | -> | appsink |
    --------------      ---------
```

  This second pipeline works propertly and renegotiates correctly.
