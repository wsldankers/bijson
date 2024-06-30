# Binary, indexable JSON

Defines a read-only binary file format that permits using a JSON
structure efficiently without first loading it in memory.

## Elements:

Every element starts with a byte that indicates its type and, where
appropriate, the size of the offset(s) used for indexing.

- 0x00: [reserved]

- 0x01: null
- 0x02: false
- 0x03: true
- 0x04: empty object
- 0x05: empty array
- 0x06: empty string
- 0x07: [empty bytes]
- 0x08: [undefined]
- 0x09: [float NaN]
- 0x0A: [decimal NaN]

- 0x11: float16
- 0x12: float32
- 0x13: float64
- 0x14: float128
- 0x15: float256

- 0x20..0x21: integer zero
- 0x22..0x23: float zero
- 0x24..0x25: [float Inf]
- 0x26..0x27: decimal zero
- 0x28..0x29: [decimal Inf]

- 0x30..0x33: string
- 0x34..0x37: [bytes]

- 0x40..0x47: integer

- 0x60..0x6F: array

- 0x80..0xBF: object

- 0xC0..0xFF: decimal

Whenever a type needs to store offsets, it does so in an integer size
that is signaled using two bits that form an integer:

0x0: 8-bit integer
0x1: 16-bit integer
0x2: 32-bit integer
0x3: 64-bit integer

Whenever a type needs to store a sign, it does so using a single bit
inside its type-indicating byte:

0x0: positive sign
0x1: negative sign

Whenever a list of offsets is stored, these offsets point to the *end*
of each entry. The offset of the first entry can always be computed
through other means, and this ensures that both the sizes of each entry
as well as the the end of the container element itself can be determined
easily.

### For each type:


#### 0x00 [reserved]

MUST NOT be used.

#### 0x01: null

Corresponds to JSON null.

#### 0x02: false

Corresponds to JSON false.

#### 0x03: true

Corresponds to JSON true.

#### 0x04: empty object

A shorthand to denote an empty object.

#### 0x05: empty array

A shorthand to denote an empty array.

#### 0x06: empty string

A shorthand to denote an empty string.

#### 0x07: [empty bytes]

A shorthand to denote an empty binary string. Non-standard.

#### 0x08: [undefined]

Corresponds to Javascript undefined. Non-standard.

#### 0x06: [float NaN]

Corresponds to floating-point NaN (not-a-number). Non-standard.

#### 0x20..0x21: integer zero

A shorthand to denote the integers +0 and -0 respectively.
The lower bit encodess the sign.

#### 0x22..0x23: float zero

A shorthand to denote the float +0.0 and -0.0 respectively.
The lower bit encodess the sign.

#### 0x24..0x25: [float Inf]

A shorthand to denote float +infinity and -infinity respectively.
The lower bit encodess the sign. Non-standard.

#### 0x30..0x33: string

Corresponds to a JSON text string. The lower two bits correspond to the
size of the following length integer. The length indicator specifies
the number of following bytes (not characters!). The following bytes
MUST form a valid UTF-8 sequence.

#### 0x34..0x37: [bytes]

Corresponds to a byte string. The lower two bits correspond to the
size of the following length integer. The length indicator specifies
the number of following bytes. Non-standard.

#### 0x40..0x47: integer

Corresponds to an integer number. The lower two bits (bits 0 and 1)
correspond to the size of the following length integer. Bit 2 indicates
the sign. The length indicator specifies the number of following bytes,
which in turn represent the number in unsigned little-endian format.

#### 0x60..0x6F: array

Corresponds to a Javascript array. The lower two bits (bits 0 and 1)
denote the size of the following integer that describes the number of
items in the array. This integer is followed by precisely that number of
integers denoting offsets of array entries. The offsets are counted from
the end of the offsets list. Bits 2 and 3 together denote the size of
each offset.

#### 0x80..0xBF: object

Corresponds to a Javascript object. The lower two bits (bits 0 and 1)
denote the size of the following integer that describes the number of
items in the array. This integer is followed by precisely that number of
integers denoting hash values of the keys. That list of hashes is in turn
followed by offsets of object keys. And that list in turn is followed
by offsets of object values.

The key offsets are counted from the end of the value offsets list. Bits
2 and 3 together denote the size of each key offset.

The value offsets are counted from the end of the last key. Bits 4 and 5
together denote the size of each value offset.

Hashes are twice the size of the item number integer.

Keys MUST be encoded in UTF-8. Entries are sorted by their hash value.
Hashes can be looked up quickly using weighted bisection.

#### 0xC0..0xFF: decimal

Mantissa, exponent, sign, sign.

Mantissa: bits 0 and 1 denote the length of the mantissa in bytes. A
mantissa is any number of 64-bit unsigned integers, followed by one
32-bit unsigned integer, one 16-bit unsigned integer and one 8-bit
integer (all optional) so that the stated length is achieved.

64-bit integers denote values between 0 and 1000000000 (exclusive);
32-bit integers denote values between 0 and 1000000 (exclusive);
16-bit integers denote values between 0 and 10000 (exclusive);
8-bit integers denote values between 0 and 100 (exclusive);

The value of the mantissa is equal to the zero-padded decimal
representations of each integer concatenated (little-endian,
so first value is the least significant).

[111111111, 222222222, 33] becomes: 33222222222111111111.

Bit 4 is the sign of the mantissa.

Bits 2 and 3 denote the size of the exponent, which is an unsigned
integer. Bit 5 is the sign of the exponent.
