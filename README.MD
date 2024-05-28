# FZFS
## Project
FZFS is a simple tool to unpack/extract Fireglow Games' Sudden Strike, Sudden Strike Resource War, and Sudden Strike 2 .sue files.

## Code
```
git clone --recurse-submodules https://github.com/americusmaximus/FZFS
```

## Requirements & Dependencies
1. [Microsoft Visual Studio](https://visualstudio.microsoft.com/downloads/)
2. [ZLib](https://github.com/madler/zlib)

## Similar & Related Projects
1. [War Action](https://github.com/americusmaximus/WarAction)
2. [War Motion](https://github.com/americusmaximus/WarMotion)
3. [War Storm](https://github.com/americusmaximus/WarStorm)
4. [War Tool Kit](https://github.com/americusmaximus/WarToolKit)

## FZFS (SUE) File Format
The SUE files consists of the following 5 segments:
1. Header segment, 8 bytes.
2. Data segment, split into chunks, variable length.
3. Information segment, consists of a segment header and deflated array of file details. See information segment section below.
4. Name segment, consists of a segment header and deflated stream of multiple zero-terminated stings (file names) combined into one array.
5. Offset segment, consists of a segment header and deflated stream of multiple unsighed integers -- offsets into the file where each data chunk is.

### Header Segment
The file header consists of 8 bytes:
1. 	First 4 bytes is 0x53465a46 (ASCII: 'FZFS').
2. 	The second 4 bytes -- a **relative** offset into the file (i.e. after reading the header's 8 bytes), where the information segment can be read from.

### Data Segment
The data section consists of multiple deflated chunks, each of them has to be inflated separately, and one file can consist of one or more chunks.

### Information Segment
The information section consists of a segment header and deflated data. The segment header is the same for all metadata sections and has 3 elements:
1. 	The total size of deflated content.
2. 	The number of items in this section.
3. 	The resulting size of each item.
Note: The resulting unpacked size of the data is the size of each item multiplied by the number of the items.

The segment data contains a number of structs (one struct per archived file) that describe it within the container file. These fields are:
1. Name, the offset into the names array, where the file name is defined. The name value comes from the content of the name segment.
2. Type, the type of compression and/or storage. All the test files I had always had value of 8.
3. Index, an index of the first offset for the file. The offset value comes from the content of the offset segment.
4. Size, the complete unpacked size of the archived file.
5. Chunk, the size of the resulting chunk, after decompression, except when the file is smaller than the chunk size, or it is the last chunk of the file.

### Name Segment
The name segment consists of the segment header, as in information segment, and the deflated linear representation of multiple zero-terminated file names. The file names can have '&bsol;' (backslash) characters to indicate nested structure of the archive.

### Offset Segment
The offset segment, similar to previous segments consisits of the header and deflated array of unsigned integers that represent **absolute** offsets into the file, where each chunk is being stored.

## Notes
This project is a quick and dirty tool, intended only for research purposes, please refer to [War Tool Kit](https://github.com/americusmaximus/WarToolKit) for better tools.

## Legal
1. The source code in this repository is mostly produced by reverse engineering the original binaries. There are a couple of exceptions for reverse engineering under DMCA -- documentation, interoperability, fair use. See goals section for the interoperability and fair use cases. The documentation is needed to support those. Also please see an article about [software preservation](https://en.wikipedia.org/wiki/Digital_preservation).
2. Sudden Strike, Fireglow Games, CDV, and others are trademarks of their respective owners.
