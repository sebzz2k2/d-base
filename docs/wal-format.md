# WAL file format

The write-ahead log (WAL) is an append-only binary file. It stores requests as
records so that durable changes can be recovered after a restart or crash.

## Blocks

The file is divided into fixed-size 32 KiB blocks. A block can contain multiple
records or record fragments.

```text
+--------------------------- 32 KiB block ----------------------------+
| record / fragment | record / fragment | ... | padding (if required) |
+----------------------------------------------------------------------+
```

Padding consists of zero bytes. It is written when there is insufficient space
left in a block for another fragment header. A fragment header never crosses a
block boundary.

## Fragment layout

Every stored fragment has its own CRC32 checksum.

```text
+-----------+----------+--------+----------------+------------------+
| CRC32     | Size     | Type   | LSN    | Transaction ID | Fragment payload |
| 4 bytes   | 2 bytes  | 1 byte | 8 bytes | 4 bytes        | `Size` bytes     |
+-----------+----------+--------+----------------+------------------+
```

- **CRC32**: CRC32 of every byte after the CRC field in this fragment: `Size`,
  `Type`, `LSN`, `Transaction ID`, and fragment payload.
- **Size**: the number of bytes in the fragment payload.
- **Type**: identifies whether the fragment contains a whole record or part of
  a record.
- **LSN**: a unique, monotonically increasing 64-bit log sequence number for a
  logical WAL record. All fragments of that record carry the same LSN.
- **Transaction ID**: the `txn_id` associated with the record. A transaction
  may have multiple records, all sharing the same transaction ID.

All integer fields use little-endian byte order.

## Fragment types

```text
FULL   = 1  Entire logical record fits in one fragment.
FIRST  = 2  First fragment of a record that spans blocks.
MIDDLE = 3  Intermediate fragment of a spanning record.
LAST   = 4  Final fragment of a spanning record.
```

Examples:

```text
One fragment:     FULL
Two fragments:    FIRST, LAST
Four fragments:   FIRST, MIDDLE, MIDDLE, LAST
```

There is no checksum for an entire block, batch, or reconstructed logical
record. Each fragment is checked independently using its own CRC32.

## Logical-record payload

The payload produced by `marshal_()` is:

```text
+---------+------------+-----+-------+
| Command | Key length | Key | Value |
| 1 byte  | 4 bytes    | ... | ...   |
+---------+------------+-----+-------+
```

- **Command**: the `VaulticCmds` value as one byte.
- **Key length**: number of bytes in `Key`.
- **Key**: raw key bytes.
- **Value**: remaining payload bytes.

The key length tells the reader where the key ends and the value begins.

## Writing and recovery

The writer batches serialized fragments, writes the bytes to the WAL, then
calls `fsync()` before completing the request promises successfully. During
recovery, the reader should validate each fragment CRC, join spanning fragments
in `FIRST`/`MIDDLE`/`LAST` order, and stop or report an error on malformed
headers, invalid CRCs, or incomplete records.
