:man_page: mongoc_ssl_opt_t

mongoc_ssl_opt_t
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
     const char *thumbprint;
     void *padding[5];
  } mongoc_ssl_opt_t;

.. note::
   |ssl:naming|

Description
-----------

This structure is used to set the TLS options for a :symbol:`mongoc_client_t` or :symbol:`mongoc_client_pool_t`.

.. versionchanged:: 1.2.0 Once a pool or client has any TLS options set, all connections use TLS, even if ``ssl=true`` is omitted from the MongoDB URI. Before, TLS options were ignored unless ``tls=true`` was included in the URI.

.. versionchanged:: 1.4.0 The :symbol:`mongoc_client_pool_set_ssl_opts` and :symbol:`mongoc_client_set_ssl_opts` will not only shallow copy the struct, but will also copy the ``const char*``. It is therefore no longer needed to make sure the values remain valid after setting them.

.. versionadded:: 2.1.0 ``thumbprint`` is added.

`Local Machine Certificate Store <https://learn.microsoft.com/en-us/windows-hardware/drivers/install/certificate-stores>`_

.. _sslopts_and_uri:

Options
-------

Some options in :symbol:`mongoc_ssl_opt_t` have corresponding URI options.
Others may only be set with :symbol:`mongoc_ssl_opt_t`.

The following documents the corresponding URI option and support in each TLS back-end.

``pem_file``
------------

Path to a file containing a client certificate and private key.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - ``MONGOC_URI_TLSCERTIFICATEKEYFILE`` (``tlsCertificateKeyFile``)
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Supported.
   * - **Windows Secure Channel**
     - Supported. Only RSA keys are supported. Consider using ``thumbprint`` to avoid limitations with ephemeral keys in Secure Channel.

``pem_pwd``
-----------

Password to decrypt ``pem_file``.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - ``MONGOC_URI_TLSCERTIFICATEKEYFILEPASSWORD`` (``tlsCertificateKeyFilePassword``)
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Supported.
   * - **Windows Secure Channel**
     - Not supported. Set ``pem_file`` to an unencrypted certificate or use ``thumbprint`` to select a certificate from the `Local Machine Certificate Store`_.

``ca_file``
-----------

Path to a file containing Certificate Authority (CA) certificates.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - ``MONGOC_URI_TLSCAFILE`` (``tlsCAFile``)
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Supported.
   * - **Windows Secure Channel**
     - Supported. CA certificate is imported into the `Local Machine Certificate Store`_.

``ca_dir``
----------

Path to a directory containing Certificate Authority (CA) certificates.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - None.
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Not supported.
   * - **Windows Secure Channel**
     - Not supported.

``crl_file``
------------

Path to a file containing a Certificate Revocation List (CRL).

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - None.
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Not supported.
   * - **Windows Secure Channel**
     - Supported. CRL certificate is imported into the `Local Machine Certificate Store`_.

``weak_cert_validation``
------------------------

Disables validation of the server certificate.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - ``MONGOC_URI_TLSALLOWINVALIDCERTIFICATES`` (``tlsAllowInvalidCertificates``)
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Supported.
   * - **Windows Secure Channel**
     - Supported.

``allow_invalid_hostname``
--------------------------

Allows mismatch of the server hostname and hostname specified by the server certificate.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - ``MONGOC_URI_TLSALLOWINVALIDCERTIFICATES`` (``tlsAllowInvalidCertificates``)
   * - **OpenSSL**
     - Supported.
   * - **macOS Secure Transport**
     - Supported.
   * - **Windows Secure Channel**
     - Supported.

``thumbprint``
--------------

Set to a SHA1 hash of a client certificate to select from a certificate store.

``thumbprint`` is useful to avoid limitations with Windows Secure Channel.

.. list-table::
   :widths: 30 70
   :width: 100%

   * - **URI option**
     - None.
   * - **OpenSSL**
     - Not supported.
   * - **macOS Secure Transport**
     - Not supported.
   * - **Windows Secure Channel**
     - Supported. Certificate is selected from the `Local Machine Certificate Store`_.

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

