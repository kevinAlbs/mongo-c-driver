:man_page: mongoc_change_stream_next

mongoc_change_stream_next()
===========================

Synopsis
--------

.. code-block:: c

  bool
  mongoc_change_stream_next (mongoc_change_stream_t* stream,
                             const bson_t** doc);

Parameters
----------

* ``stream``: A :symbol:`mongoc_change_stream_t` obtained from ``mongoc_collection_watch``.
* ``doc``: An out pointer for the doc.

Returns
-------
A boolean indicating whether or not there was another document in the stream.

Similar to ``mongoc_cursor_next`` the lifetime of ``doc`` is until the next call to ``mongoc_change_stream_watch``, so it needs to be copied if needed.
