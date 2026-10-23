ezgrpc2 API
===========

Include ``ezgrpc2.h``. Every function returns an :c:type:`ezgrpc2_err` (0 on
success, negative on error) unless it returns a value.

A server is created from an :c:type:`ezgrpc2_config` and serves methods in one
of two ways:

* **Handlers**: :c:func:`ezgrpc2_server_handle` for methods with a single
  request (unary and server streaming) and :c:func:`ezgrpc2_server_handle_stream`
  for client-streaming and bidirectional methods. Run the server with
  :c:func:`ezgrpc2_server_run` and stop it with :c:func:`ezgrpc2_server_stop`.

* **Poll mode**: :c:func:`ezgrpc2_server_register_path`, then call
  :c:func:`ezgrpc2_server_poll` in your own loop and handle the
  :c:func:`ezgrpc2_server_events`.

Either way, each request is an :c:type:`ezgrpc2_call`: read its metadata, send
responses with :c:func:`ezgrpc2_call_send` and end it with
:c:func:`ezgrpc2_call_finish`. To answer from another thread, take a reference
with :c:func:`ezgrpc2_call_ref` and use :c:func:`ezgrpc2_call_send_async` and
:c:func:`ezgrpc2_call_finish_async`.

Server
------

.. autocmodule:: ezgrpc2_server.h
   :members:

Calls
-----

.. autocmodule:: ezgrpc2_call.h
   :members:

Events (poll mode)
------------------

.. autocmodule:: ezgrpc2_event.h
   :members:

Messages
--------

.. autocmodule:: ezgrpc2_message.h
   :members:

Metadata
--------

.. autocmodule:: ezgrpc2_metadata.h
   :members:

Errors
------

.. autocmodule:: ezgrpc2_error.h
   :members:

gRPC status codes
-----------------

.. autocmodule:: ezgrpc2_status.h
   :members:

Rich error details
------------------

.. autocmodule:: ezgrpc2_status_details.h
   :members:

Interceptors
------------

.. autocmodule:: ezgrpc2_interceptor.h
   :members:

Health checking
---------------

.. autocmodule:: ezgrpc2_health.h
   :members:

Server reflection
-----------------

.. autocmodule:: ezgrpc2_reflection.h
   :members:

Compression
-----------

.. autocmodule:: ezgrpc2_compression.h
   :members:

TLS
---

.. autocmodule:: ezgrpc2_tls.h
   :members:

Memory
------

.. autocmodule:: ezgrpc2_mem.h
   :members:

Protobuf-C and FlatBuffers
--------------------------

Built when the libraries are found (``PROTOBUF_C=1``, ``FLATCC=1``):

* ``ezgrpc2_protobuf_c.h``: ``EZGRPC2_PB_UNPACK()``, ``ezgrpc2_pb_pack()`` and
  ``ezgrpc2_call_send_pb()``.
* ``ezgrpc2_flatcc.h``: ``EZGRPC2_FB_UNPACK()`` (verified, zero-copy reads),
  ``ezgrpc2_fb_pack()`` and ``ezgrpc2_call_send_fb()``.

See the comments in those headers.
