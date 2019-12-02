:man_page: mongoc_client_encryption_datakey_opts_set_masterkey

mongoc_client_encryption_datakey_opts_set_masterkey()
=====================================================

Synopsis
--------

.. code-block:: c

   void
   mongoc_client_encryption_datakey_opts_set_masterkey (
      mongoc_client_encryption_datakey_opts_t *opts, const bson_t *masterkey);

Description
-----------

Identifies the masterkey for the KMS provider used to encrypt a new data key.

This is required if using AWS KMS, and ``masterkey`` must have the form:

.. code-block:: javascript

   {
      region: string, // Required.
      key: string // Required. The Amazon Resource Name (ARN) to the AWS customer master key (CMK).
      endpoint: string // Optional. An alternate host identifier to send KMS requests to. May include port number.
   }

The value of "endpoint" is a host name with optional port number separated by a colon. E.g. "kms.us-east-1.amazonaws.com" or "kms.us-east-1.amazonaws.com:443"

This function is not applicable for creating data keys with the local KMS provider.

Parameters
----------

* ``opts``: A :symbol:`mongoc_client_encryption_datakey_opts_t`
* ``masterkey``: A :symbol:`bson_t` document describing the KMS provider specific masterkey.