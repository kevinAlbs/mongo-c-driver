:man_page: mongoc_change_stream_error

mongoc_change_stream_error()
============================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_change_stream_error (mongoc_change_stream_t* stream,
                             bson_error_t* err);

Parameters
----------

* ``stream``: A :symbol:`mongoc_change_stream_t` obtained from ``mongoc_collection_watch``.
* ``err``: An out pointer for the doc.

Returns
-------
A boolean indicating if there was an error.
