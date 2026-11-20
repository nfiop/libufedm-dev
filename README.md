# **libufedm-dev**

**A cross-platform device abstraction library for ufedm.**

`libufedm-dev` is a library designed to abstract platform-specific device operations and provide a common interface for interacting with ufedm_proxy devices.

The goal is to separate low-level, platform-dependent functionality from the core ufedm implementation, making it easier to support additional operating systems and hardware platforms without introducing platform-specific code into the main project.

## **Background**

This library was originally part of the ufedm repository, where its functionality was maintained alongside the rest of the project.

It has since been extracted into a separate repository to make platform support easier to develop, maintain, and extend.

Keeping this functionality independent allows different platform implementations to evolve separately and makes it possible to introduce support for additional platforms without unnecessarily complicating the main ufedm codebase.

Although it originated as part of ufedm, the intention is to keep this library modular and reusable.

## **Features**

* **Platform abstraction:** Isolates platform-specific device operations behind a common interface.  
* **Extensible architecture:** Provides a foundation for adding support for additional platforms.  
* **Asynchronous I/O processing:** Uses dedicated worker threads to handle device read and write requests.  
* **Shared memory communication:** Supports memory-mapped shared buffers for communication with the ufedm proxy device.  
* **Event-driven processing:** Uses event file descriptors to notify worker threads about pending I/O operations.  
* **Callback-based processing:** Allows applications to provide their own read, write, and fatal-error handling callbacks.  
* **Queue management:** Supports initialization and management of multiple I/O queues.

## **Architecture**

The library provides an abstraction between ufedm's device interface and the application consuming it.

Its main responsibilities include:

* Opening and managing proxy device instances.  
* Retrieving device and shared-memory information.  
* Managing shared-memory mappings.  
* Initializing I/O queues and their associated worker threads.  
* Registering and unregistering event file descriptors.  
* Dispatching read and write requests through application-provided callbacks.  
* Handling acknowledgements, negative acknowledgements, and fatal errors.  
* Managing resource cleanup and worker-thread lifecycle.

### **I/O processing**

The library uses a queue-based architecture.

Each queue contains multiple I/O slots, with worker threads responsible for processing requests associated with those slots.

Applications provide callbacks for handling read and write operations, allowing the library to manage the underlying communication mechanism independently of the actual data-processing implementation.

## **Repository structure**

text

```
libufedm-dev/
├── linux-mtd/       # Linux MTD-related definitions and interfaces
├── CMakeLists.txt   # CMake build configuration
├── proxy_dev.c      # Device abstraction and I/O implementation
├── proxy_dev.h      # Public API
└── README.md
```

## **Building**

The project uses CMake and requires a C11-compatible compiler.

### **Requirements**

* CMake 3.20 or newer  
* C compiler with C11 support  
* POSIX threads  
* Linux system interfaces required by the current implementation

### **Compile**

bash

```
git clone https://github.com/nfiop/libufedm-dev.git
cd libufedm-dev

mkdir build
cd build

cmake ..
make
```

This produces the static library:

```
libufedm-dev.a
```

### **Installation**

The CMake configuration provides an installation directive for the static library.

bash

```
cmake --install build
```

## **API overview**

The public API is exposed through `proxy_dev.h`.

The main interfaces include:

| Function | Description |
| ----- | ----- |
| `proxy_device_close()` | Releases the resources associated with a proxy device. |
| `proxy_device_create_queue_state()` | Initializes an individual I/O queue. |
| `proxy_device_initialize_queues()` | Initializes multiple I/O queues. |
| `proxy_device_destroy_queue_state()` | Destroys an initialized I/O queue. |

Read and write processing is provided through application-defined callbacks using `struct proxy_queue_func_ops`.

See `proxy_dev.h` for the complete API and callback definitions.

## **Linux-specific API**

The library supports 2 kind of proxy device - a Linux ufedm proxy device
and an emulated proxy device.

An emulated device is always supported, to aid user programs generating
a demo environment, for example.

It can also be used with Windows and macOS machines, that lack any MTD
subsystem.

The emulated device mimicks as much as possible the structure and
principles of the Linux ufedm proxy device structure, hence it's an
"emulation" of that in userspace.

When the library platform target is Linux, an extension of the public
API is enabled - mainly `open`-like functions and eventfd registration
as well.

See `linux-mtd/dev.h` for more information about that API.

## **Platform support and end goal**

The project is intended to provide a common foundation for platform-specific device functionality.

The current implementation contains Linux-specific functionality, including the use of `ioctl`, `mmap`, `eventfd`, and POSIX threads.

Additional platform implementations can be introduced as the project evolves, with the objective of keeping platform-specific details isolated from the common interface.

The end goal is to make platform support easier to develop independently while keeping the interface consistent across implementations.

## **License**

This project is licensed under **GPL-2.0-only**.

See the source file headers for copyright and licensing information.

