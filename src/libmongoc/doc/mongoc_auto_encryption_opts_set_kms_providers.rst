:man_page: mongoc_auto_encryption_opts_set_kms_providers

mongoc_auto_encryption_opts_set_kms_providers()
===============================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_auto_encryption_opts_set_kms_providers (
      mongoc_auto_encryption_opts_t *opts, const bson_t *kms_providers);


Parameters
----------

* ``opts``: The :symbol:`mongoc_auto_encryption_opts_t`
* ``kms_providers``: A :symbol:`bson_t` containing credentials or configuration for an external Key Management Service (KMS).

``kms_providers`` is a BSON document containing keys for each KMS provider. At least one of ``aws`` or ``local`` must be specified, and both may be specified. The format is as follows:

.. code-block:: js

   aws: {
      accessKeyId: String,
      secretAccessKey: String
   }

   local: {
      key: byte[96] // The master key used to encrypt/decrypt data keys.
   }


See also
--------

* The guide for :doc:`Using Client-Side Field Level Encryption <using_client_side_encryption>`
