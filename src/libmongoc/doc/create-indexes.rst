:man_page: mongoc_create_indexes

Creating Indexes
================

To create indexes on a MongoDB collection, execute the ``createIndexes`` command
with a command function like :symbol:`mongoc_database_write_command_with_opts` or
:symbol:`mongoc_collection_write_command_with_opts`. See `the MongoDB
Manual entry for the createIndexes command
<https://www.mongodb.com/docs/manual/reference/command/createIndexes/>`_ for details.

.. warning::

   The ``commitQuorum`` option to the ``createIndexes`` command is only
   supported in MongoDB 4.4+ servers, but it is not validated in the command
   functions. Do not pass ``commitQuorum`` if connected to server versions less
   than 4.4. Using the ``commitQuorum`` option on server versions less than 4.4
   may have adverse effects on index builds.

Example
-------

.. literalinclude:: ../examples/example-create-indexes.c
   :language: c
   :caption: example-create-indexes.c

Creating Search Indexes
=======================

To create an Atlas Search Index, use the createSearchIndexes command:

.. literalinclude:: ../examples/example-create-search-indexes.c
   :language: c
   :start-after: // Create an Atlas Search Index ... begin
   :end-before: // Create an Atlas Search Index ... end
   :dedent: 6

To list Atlas Search Indexes, use the $listSearchIndexes aggregation stage:

.. literalinclude:: ../examples/example-create-search-indexes.c
   :language: c
   :start-after: // List Atlas Search Indexes ... begin
   :end-before: // List Atlas Search Indexes ... end
   :dedent: 6

To update an Atlas Search Index, use the updateSearchIndex command:

.. literalinclude:: ../examples/example-create-search-indexes.c
   :language: c
   :start-after: // Update an Atlas Search Index ... begin
   :end-before: // Update an Atlas Search Index ... end
   :dedent: 6

To drop an Atlas Search Index, use the dropSearchIndex command:

.. literalinclude:: ../examples/example-create-search-indexes.c
   :language: c
   :start-after: // Drop an Atlas Search Index ... begin
   :end-before: // Drop an Atlas Search Index ... end
   :dedent: 6

