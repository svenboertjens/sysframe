/*
  ## Explanation for the SFS (Simple File System) protocol # (PREVIEW, DOES NOT EXIST YET!)

  # Quick note

  In this file and the explanation, terms may be used that are not
  explained here. Those are explained in 'sbs_X.c' (X can vary, it
  should be the file placed directly under sysframe/pybytes).

  Also, this protocol is not yet implemented. Still working on
  the SBS protocol.


  # What is the 'Simple File System' protocol?

  This protocol got its name from where the initial idea came from.
  That being, a file system. Unlike regular serialization methods,
  a file system does not have to read the entire storage to read,
  modify, or add a file. Just like this protocol, which allows us
  to read and modify an index without having to do omething with
  the other values. Of course it will also be possible to get the
  entire list/dict back as a regular value.

  This provides advantages in overhead compared to the full conversion
  of a value with regular serialization. This is particularly useful
  when a container type

*/