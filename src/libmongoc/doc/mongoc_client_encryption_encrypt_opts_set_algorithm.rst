:man_page: mongoc_client_encryption_encrypt_opts_set_algorithm

mongoc_client_encryption_encrypt_opts_set_algorithm()
=====================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_encrypt_opts_set_algorithm (
      mongoc_client_encryption_encrypt_opts_t *opts, const char *algorithm);

Description
-----------

Identifies the algorithm to use for encryption. Valid values of ``algorithm`` are:

* "AEAD_AES_256_CBC_HMAC_SHA_512-Random" for randomized encryption.
* "AEAD_AES_256_CBC_HMAC_SHA_512-Deterministic" for deterministic (queryable) encryption.

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_encrypt_opts_t`
* ``algorithm``: A ``char *`` identifying the algorithm.