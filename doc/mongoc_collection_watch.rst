:man_page: mongoc_collection_watch

mongoc_collection_watch()
=========================

Synopsis
--------

.. code-block:: c

  mongoc_change_stream_t*
  mongoc_collection_watch (const mongoc_collection_t* coll,
                           const bson_t* pipeline,
                           const bson_t* opts)


Creates a change stream using the read preference of the collection.

``pipeline`` and ``opts`` may be null. If not, they are copied.

.. warning::

   A change stream is only supported with majority read preference.

TODO: finish docs.

Parameters
----------

* ``coll``:
* ``pipeline``:
* ``opts``:

Returns
-------
