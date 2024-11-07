:man_page: mongoc_ssl_opt_get_default

mongoc_ssl_opt_get_default()
============================

.. deprecated:: 1.30.0

   Use :symbol:`mongoc_tls_opt_get_default` instead.

Synopsis
--------

.. code-block:: c

  const mongoc_tls_opt_t *
  mongoc_ssl_opt_get_default (void);

Returns
-------

Returns the default TLS options for the process. This should not be modified or freed.

