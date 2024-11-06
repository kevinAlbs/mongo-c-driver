:man_page: mongoc_ssl_opt_t

mongoc_ssl_opt_t
================

.. deprecated:: 1.30.0

   Use :symbol:`mongoc_tls_opt_t` instead.

Synopsis
--------

.. code-block:: c

  typedef struct mongoc_tls_opt_t mongoc_ssl_opt_t;

Description
-----------

This structure is used to set the TLS options for a :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t`.

Beginning in version 1.2.0, once a pool or client has any TLS options set, all connections use TLS, even if the TLS is not enabled with a URI option: ``tls=true`` or the deprecated ``ssl=true``. Before, TLS options were ignored unless enabled in the URI.

As of 1.4.0, the :symbol:`mongoc_client_pool_set_ssl_opts` and :symbol:`mongoc_client_set_ssl_opts` will not only shallow copy the struct, but will also copy the ``const char*``. It is therefore no longer needed to make sure the values remain valid after setting them.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_ssl_opt_get_default

.. seealso::

  | `Configuring TLS <configuring_tls_>`_

  | :doc:`mongoc_client_set_ssl_opts`

  | :doc:`mongoc_client_pool_set_ssl_opts`

