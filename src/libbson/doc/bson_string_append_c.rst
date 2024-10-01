:man_page: bson_string_append_c

bson_string_append_c()
======================

.. warning::
   .. deprecated:: 1.29.0

      This function is deprecated and should not be used in new code.

Synopsis
--------

.. code-block:: c

  void
  bson_string_append_c (bson_string_t *string, char str);

Parameters
----------

* ``string``: A :symbol:`bson_string_t`.
* ``str``: An ASCII char.

Description
-----------

Appends ``c`` to the string builder ``string``.

.. warning:: The length of the resulting string (including the ``NULL`` terminator) MUST NOT exceed ``UINT32_MAX``.
