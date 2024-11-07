:man_page: mongoc_tls_opt_t

mongoc_tls_opt_t
================

Synopsis
--------

.. code-block:: c

  typedef struct {
     const char *pem_file;
     const char *pem_pwd;
     const char *ca_file;
     const char *ca_dir;
     const char *crl_file;
     bool weak_cert_validation;
     bool allow_invalid_hostname;
     void *internal;
     void *padding[6];
  } mongoc_tls_opt_t;

Description
-----------

This structure is used to set the TLS options for a :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t`.

Once a pool or client has any TLS options set, all connections use TLS, even if the TLS is not enabled with a URI option: ``tls=true`` or the deprecated ``ssl=true``.

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_tls_opt_get_default

.. seealso::

  | `Configuring TLS <configuring_tls_>`_

  | :doc:`mongoc_client_set_tls_opts`

  | :doc:`mongoc_client_pool_set_tls_opts`

