:man_page: mongoc_client_set_sockettimeoutms

mongoc_client_set_sockettimeoutms()
===================================

Synopsis
--------

.. code-block:: c

  void
  mongoc_client_set_sockettimeoutms (mongoc_client_t *client, int32_t timeoutms);

Change the ``sockettimeoutms`` of the :symbol:`mongoc_client_t`.

Parameters
----------

* ``client``: A :symbol:`mongoc_client_t`.
* ``timeoutms``: The requested timeout value in milliseconds.

