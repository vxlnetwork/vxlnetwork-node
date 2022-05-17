# Disk-based hashtable

[![Travis](https://api.travis-ci.com/luispedro/diskhash.png)](https://travis-ci.com/luispedro/diskhash)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](https://opensource.org/licenses/MIT)


A simple disk-based hash table (i.e., persistent hash table).

It is a hashtable implemented on memory-mapped disk, so that it can be loaded
with a single `mmap()` system call and used in memory directly (being as fast
as an in-memory hashtable once it is loaded from disk).

The code is in C, wrappers are provided for Python, Haskell, and C++. The
wrappers follow similar APIs with variations to accommodate the language
specificity. They all use the same underlying code, so you can open a hashtable
created in C from Haskell, modify it within your Haskell code, and later open
the result in Python.

Cross-language functionality will only work for simple types where you can
control their binary representation (64-bit integers, for example).

Reading does not touch the disk representation at all and, thus, can be done on
top of read-only files or using multiple threads (and different processes will
share the memory: the operating system does that for you). Writing or modifying
values is, however, not thread-safe.

## Examples

The following examples all create a hashtable to store longs (`int64_t`), then
set the value associated with the key `"key"` to 9. In the current API, the
maximum size of the keys needs to be pre-specified, which is the value `15`
below.

### Raw C

```c
#include <stdio.h>
#include <inttypes.h>
#include "diskhash.h"

int main(void) {
    HashTableOpts opts;
    opts.key_maxlen = 15;
    opts.object_datalen = sizeof(int64_t);
    char* err = NULL;
    HashTable* ht = dht_open("testing.dht", opts, O_RDWR|O_CREAT, &err);
    if (!ht) {
        if (!err) err = "Unknown error";
        fprintf(stderr, "Failed opening hash table: %s.\n", err);
        return 1;
    }
    long i = 9;
    dht_insert(ht, "key", &i);
    
    long* val = (long*) dht_lookup(ht, "key");
    printf("Looked up value: %l\n", *val);

    dht_free(ht);
    return 0;
}
```

The C API relies on error codes and error strings (the `&err` argument above).
The header file has [decent
documentation](https://github.com/luispedro/diskhash/blob/master/src/diskhash.h).

### Haskell

In Haskell, you have different types/functions for read-write and read-only
hashtables. Read-write operations are `IO` operations, read-only hashtables are
pure.

Read write example:

```haskell
import Data.DiskHash
import Data.Int
main = do
    ht <- htOpenRW "testing.dht" 15
    htInsertRW ht "key" (9 :: Int64)
    val <- htLookupRW "key" ht
    print val
```

Read only example (`htLookupRO` is pure in this case):

```haskell
import Data.DiskHash
import Data.Int
main = do
    ht <- htOpenRO "testing.dht" 15
    let val :: Int64
        val = htLookupRO "key" ht
    print val
```


### Python

Python's interface is based on the [struct
module](https://docs.python.org/3/library/struct.html). For example, `'ll'`
refers to a pair of 64-bit ints (_longs_):

```python
import diskhash

tb = diskhash.StructHash(
    fname="testing.dht", 
    keysize=15, 
    structformat='ll',  # store pairs of longs
    mode='rw',
)
value = [1, 2]  # pair of longs
tb.insert("key", *value)
print(tb.lookup("key"))
```

The Python interface is currently Python 3 only. Patches to extend it to 2.7
are welcome, but it's not a priority.


### C++

In C++, a simple wrapper is defined, which provides a modicum of type-safety.
You use the `DiskHash<T>` template. Additionally, errors are reported through
exceptions (both `std::bad_alloc` and `std::runtime_error` can be thrown) and
not return codes.

```c++
#include <iostream>
#include <string>

#include <diskhash.hpp>

int main() {
    const int key_maxlen = 15;
    dht::DiskHash<uint64_t> ht("testing.dht", key_maxlen, dht::DHOpenRW);
    std::string line;
    uint64_t ix = 0;
    while (std::getline(std::cine, line)) {
        if (line.length() > key_maxlen) {
            std::cerr << "Key too long: '" << line << "'. Aborting.\n";
            return 2;
        }
        const bool inserted = ht.insert(line.c_str(), ix);
        if (!inserted) {
            std::cerr  << "Found repeated key '" << line << "' (ignored).\n";
        }
        ++ix;
    }
    return 0;
}
```

## Stability

This is _beta_ software. It is good enough that I am using it, but the API can
change in the future with little warning. The binary format is versioned (the
magic string encodes its version, so changes can be detected and you will get
an error message in the future rather than some silent misbehaviour.

[Automated unit testing](https://travis-ci.com/luispedro/diskhash) ensures that
basic mistakes will not go uncaught.

## Limitations

- You must specify the maximum key size. This can be worked around either by
  pre-hashing the keys (with a strong hash) or using multiple hash tables for
  different key sizes. Neither is currently implemented in diskhash.

- You cannot delete objects. This was not a necessity for my uses, so it was
  not implemented. A simple implementation could be done by marking objects as
  "deleted" in place and recompacting when the hash table size changes or with
  an explicit `dht_gc()` call. It may also be important to add functionality to
  shrink hashtables so as to not waste disk space.

- The algorithm is a rather naïve implementation of linear addression. It would
  not be hard to switch to [robin hood
  hashing](https://www.sebastiansylvan.com/post/robin-hood-hashing-should-be-your-default-hash-table-implementation/)
  and this may indeed happen in the near future.

License: MIT

