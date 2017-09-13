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


Creates a change stream. This uses the read preference and read concern inherited from the collection.

:symbol:``pipeline``

:symbol:``opts``

.. warning::

   A change stream is only supported with majority read concern.

Parameters
----------

* ``coll``: is a ``mongoc_collection_t`` specifying the collection which the change stream listens to.
* ``pipeline``: is an optional ``bson_t`` representing an aggregation pipeline appended to the change stream. This may be ``NULL``.
* ``opts``: is an optional ``bson_t`` containing change stream options. This may be ``NULL``.

Returns
-------
A newly allocated ``mongoc_change_stream_t`` which must be freed with ``mongoc_change_stream_destroy``.