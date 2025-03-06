# Binary, indexable JSON

Defines a read-only binary file format that permits using a JSON structure
efficiently without first loading it in memory.

## Elements:

Every element starts with a byte that indicates its type:

- 0x00: [reserved]

- 0x01: null
- 0x02: false
- 0x03: true
- 0x04: [undefined]

- 0x05..0x07: [reserved]

- 0x08: string
- 0x09: [bytes]
- 0x0A: IEEE 754-2008 binary floating point
- 0x0B: IEEE 754-2008 decimal floating point

- 0x0C..0x0F: [reserved]

- 0x10..0x11: [sNaN]
- 0x12..0x13: [qNaN]
- 0x14..0x15: [Inf]

- 0x1C..0x1F: [reserved]

- 0x18..0x19: binary integer
- 0x1A..0x1B: decimal integer

- 0x1C..0x1F: [reserved]

- 0x20..0x2F: decimal

- 0x30..0x3F: array

- 0x40..0x7F: object

- 0x80..0xFF: [reserved]

Any lookup requires the total size of the buffer, this is referred to as the
bounding size.

All basic integers are encoded as little-endian.

Whenever a type needs to store offsets, it does so in an integer size that is
signaled using two bits (bit 0 is the least significant) that form an integer:

- 0x0: 8-bit integer
- 0x1: 16-bit integer
- 0x2: 32-bit integer
- 0x3: 64-bit integer

Whenever a type needs to store a sign, it does so using a single bit
inside its type-indicating byte:

- 0x0: positive sign
- 0x1: negative sign

Unless otherwise noted, whenever a list of offsets is stored, the first offset
is omitted because it's always zero. The size of the last entry can always be
computed using its offset and the bounding size of the container.

Whenever a length or count is stored and the length cannot reasonably be 0
(given the existence of dedicated empty array/object/zero types) then the
stored value is one less than the actual length. This is denoted as length-1.

### For each type:

#### 0x00 [reserved]

Must not be used. Implies file corruption if encountered.

#### 0x01: null

Corresponds to JSON null.

#### 0x02: false

Corresponds to JSON false.

#### 0x03: true

Corresponds to JSON true.

#### 0x04: [undefined]

Corresponds to Javascript undefined. Non-standard.

#### 0x05..0x07: [reserved]

Must not be used. May be used in the future.

#### 0x08: string

Corresponds to a JSON text string. The following bytes MUST form a valid UTF-8
sequence.

#### 0x09: [bytes]

Corresponds to a byte string. Non-standard.

#### 0x0A: IEEE 754-2008 binary floating point

The bounding size determines the number of bits in the floating point number.
Uses little-endian encoding.

#### 0x0B: IEEE 754-2008 decimal floating point

The bounding size determines the number of bits in the floating point number.
Uses little-endian encoding.

#### 0x09..0x0F: [reserved]

Must not be used. May be used in the future.

#### 0x10..0x11: [sNaN]

Denotes a sigmaling NaN (Not a Number). The lower bit encodes the sign.
Non-standard.

#### 0x12..0x13: [qNaN]

Denotes a quiet NaN (Not a Number). The lower bit encodes the sign.
Non-standard.

#### 0x14..0x15: [Inf]

Denotes +infinity and -infinity respectively. The lower bit encodes the sign.
Non-standard.

#### 0x18..0x19: binary integer

Corresponds to an integer number. The lower bit indicates the sign. The
following bytes represent the absolute value of the number in unsigned
little-endian format.

#### 0x1A..0x1B: decimal integer

A shorthand to denote decimal integers, which are the same as decimal values
(see below) but without an exponent. The lower bit encodes the sign. See the
decimal type for the method of encoding the mantissa.

#### 0x1A..0x1F: [reserved]

Must not be used. May be used in the future.

#### 0x20..0x2F: decimal

A decimal number with exponent.

Exponent length size, sign, sign.

Exponent: bits 0 and 1 denote the size of the following integer, which describes
the length-1 of the exponent in bytes.

Bit 2 is the sign of the mantissa. Bit 3 is the sign of the exponent.

After the length integer follow the exponent and mantissa, respectively. The
length of the mantissa can be inferred from the bounding size. If this length
is 0 then the number is 0.

The mantissa and exponent consist of any number of 64-bit unsigned integers,
the last of which is reduced by one and truncated if and only if the full
64-bit integer is not necessary to encode that part.

64-bit integers denote values between 0 (inclusive) and 10000000000000000000
(exclusive). That's 1e19, the largest power of 10 that fits in a 64-bit
unsigned integer.

The value of the mantissa and exponent are equal to the zero-padded decimal
representations of each integer concatenated in reverse order. In other words,
the stored value is little-endian and the last value is the most significant.

As an example, this 17 byte sequence: [1 (8 bytes), 2 (8 bytes), 3 (1 byte)]
would represent the value 400000000000000000020000000000000000001 (recall that
the last value was reduced by one before storing).

#### 0x30..0x3F: array

Corresponds to a Javascript array. The lower two bits (bits 0 and 1) denote
the size of the following length-1 integer that describes the number of items
in the array. This integer is followed by that number-1 of integers denoting
offsets of array entries. Bits 2 and 3 together denote the size of each
offset. The type identification bytes are not included in the item offsets.
When fetching the n'th item, add n to the offset. The last entry's size is
inferred from the total size of the array representation.

If the bounding size is 0, the array is empty. In this case the type value
should be 0x30 (no other bits set).

#### 0x40..0x7F: object

Corresponds to a Javascript object.

Count size, key offset size, value offset size.

The lower two bits (bits 0 and 1) denote the size of the following length-1
integer that describes the number of key/value pairs in the object. This integer
is followed by that number of offsets of object keys. These offsets point to the
end of each object key. That list in turn is followed by length-1 offsets of
object values.

The key offsets are counted from the end of the values offsets list. Bits 2
and 3 together denote the size of each key offset.

The value offsets are counted from the end of the last key. Bits 4 and 5
together denote the size of each value offset. The type identification bytes
are not included in the value offsets. When fetching the n'th item, add n to
the offset.

Keys must be encoded in UTF-8. Entries are sorted by the hash value of their
respective keys, then the length of their keys, then the keys themselves. This
way entries can be looked up using weighted bisection, with performance very
similar to hash tables and with excellent cache locality.

If weighted bisection takes too many steps (>⌈log₂​(n)⌉, with n being the number
of key/value pairs in the object), fall back to normal bisection. This ensures
that lookups finish in O(log n) time.

Uses XXH3_128 for hashing. Use the xxhash supplied comparison functions for
128-bit hashes while sorting and performing lookups.

If the bounding size is 0, the object is empty. In this case the type value
should be 0x40 (no other bits set).
