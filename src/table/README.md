Table
===============================================================================
The table is a level of abstraction overtop of the storage layer (e.g. B+ tree
storage). The goal is to provide a uniform interface for upperlayer systems to
interact with the storage layer.

Two main interfaces exist:
1. Table: the table interface is as mentioned earlier to provide a useful way
   reading and writing to the storage layer. This is done by providing a iterator
   styled interface and an interface for creating, deleting, and updating rows
   directly.
2. Table Manager: the follow interface will be used for creating the tables
   themselves depending on how the tables are stored (i.e. in seperate files,
   a singular file, in memory, etc...). Once tables are creating using the
   table manager, a handle can be created using the table interface to interact
   with the table data.
