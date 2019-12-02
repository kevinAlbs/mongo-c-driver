:man_page: mongoc_client_encryption_opts_t

mongoc_client_encryption_opts_t
===============================

Synopsis
--------

.. code-block:: c

   #include <mongoc/mongoc.h>

   typedef struct _mongoc_client_encryption_opts_t mongoc_client_encryption_opts_t;

Description
-----------

Used to set options for :symbol:`mongoc_client_encryption_new()`.

See also
--------

* :symbol:`mongoc_client_encryption_new()`

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_client_encryption_opts_new
    mongoc_client_encryption_opts_destroy
    mongoc_client_encryption_opts_set_keyvault_client
    mongoc_client_encryption_opts_set_keyvault_namespace
    mongoc_client_encryption_opts_set_kms_providers
