:man_page: mongoc_collection_destroy

mongoc_collection_destroy()
===========================

Synopsis
--------

.. code-block:: c

  void
  mongoc_collection_destroy (mongoc_change_stream_t* stream)

Destroys a change stream and associated data.

Parameters
----------

* ``stream``: An allocated ``mongoc_change_stream_t``