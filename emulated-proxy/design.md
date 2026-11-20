# Generic proxy device support

On Windows and macOS, there's no support for any kind of MTD subsystem.

Thus, we need to implement a layer that will wrap the proxy
functionality that is being used in ufedmd.

## Emulated behavior

We would want to create a `proxy_device_state` structure, and populate
it with everything that is needed for proper function.

On Linux, we simply do a set of ioctls to find the properties, like the
shared memory buffer size, queues' count, slots' count, etc.

For non-Linux OSes, the user should supply these parameters themselves.

## Sync queue

A sync queue is the POSIX replacement for `eventfd`s, acting as a
semaphore.

There are presumably 2 I/O proxy queues - one for read, another for
write.

Each I/O proxy queue has its slots, and in the ufedm kernel module
we simply allocate a slot for the entire transaction.

The sync queue is based around NVMe semantics - each I/O proxy queue
has its submission queue and a completion queue.

For example, when the user wants to read from a NAND device, our layer
enqueues a request by -
 - Allocate a slot on the I/O (read) proxy queue.
 - Populate this slot with a proper shared-memory header
 - Copy the content of the request to the data (DATA+ECC) buffer
 - Enqueue a request with NAND_PROXY_SYNC_REQUEST_SUBMISSION on the read
   `nand_proxy_sync_queue_t` queue.

Similar operation could be done for write.

When a proxy application is done processing it will respond with either
`NAND_PROXY_SYNC_REQUEST_COMPLETION_ACK` for ACKing or for a NACK -
`NAND_PROXY_SYNC_REQUEST_COMPLETION_NACK`.

It will make sure to put the correct details in either the nack or ack
structures in the `union {} response`.

When an ACK completion is signalled, it means the buffer is now ready
to be used for the next step in the I/O chain (either write or read). 
