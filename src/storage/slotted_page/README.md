Pages
===============================================================================

Pages are used as a friendly wrapper around frames from the buffer pool
manager. They help facilitate the tracking of page access for the replacement
policy.

Pages have a simple interface to allow for acquiring views to the frame data as
well as perform locking operations on the underlying frames in the buffer pool
manager for exclusive access.
