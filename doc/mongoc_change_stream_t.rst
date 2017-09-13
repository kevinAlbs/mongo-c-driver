:man_page: mongoc_change_stream_t

mongoc_change_stream_t
======================

Synopsis
--------

.. code-block:: c

   #include <mongoc.h>
   typedef struct _mongoc_change_stream_t mongoc_change_stream_t;

``mongoc_change_stream_t`` provides access to a collection change stream. It has similar behavior to a cursor created with the flags: ``MONGOC_QUERY_TAILABLE_CURSOR | MONGOC_QUERY_AWAIT_CURSOR``.

.. warning::

   Although it is possible to create a change stream using mongoc_collection_aggregate, it is recommended to use the mongoc_change_stream_t API.


Example
-------
.. literalinclude:: ../examples/example-collection-watch.c
   :language: c
   :caption: example-collection-watch.c

.. only:: html

  Functions
  ---------

  .. toctree::
    :titlesonly:
    :maxdepth: 1

    mongoc_collection_watch
    mongoc_change_stream_next
    mongoc_change_stream_error
    mongoc_change_stream_destroy