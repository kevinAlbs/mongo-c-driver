:man_page: mongoc_client_set_tls_opts

mongoc_client_set_tls_opts()
============================

Synopsis
--------

.. code-block:: c

  #ifdef MONGOC_ENABLE_TLS
  void
  mongoc_client_set_tls_opts (mongoc_client_t *client,
                              const mongoc_tls_opt_t *opts);
  #endif

Sets the TLS options to use when connecting to TLS enabled MongoDB servers.

The ``mongoc_tls_opt_t`` struct is copied by the client along with the strings
it points to (``pem_file``, ``pem_pwd``, ``ca_file``, ``ca_dir``, and
``crl_file``) so they don't have to remain valid after the call to
``mongoc_client_set_tls_opts``.

A call to ``mongoc_client_set_tls_opts`` overrides all TLS options set through
the connection string with which the ``mongoc_client_t`` was constructed.

It is a programming error to call this function on a client from a
:symbol:`mongoc_client_pool_t`. Instead, call
:symbol:`mongoc_client_pool_set_tls_opts` on the pool before popping any
clients.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``opts``: A :symbol:`mongoc_tls_opt_t`.

Availability
------------

This feature requires that the MongoDB C driver was compiled with ``-DENABLE_TLS``.

