# tiffloader
Portable single file ANSI C TIFF file loader

The idea is that this is a one file drop-in TIFF loader.
So there will be no messing about with external dependencies.

It's meant to be totally portable so it won't break anywhere
there's a C compiler.

We need to take a decision on JPEG support, realistically
we'll need to break the single file rule to provide this.

Added by pochi in November 2023
Converted to c++
Added bmp and added a sample that can be checked as a bitmap.
Nothing done for unsupported parts.

Fixed for some trivial bugs.
No support, but fix bugs where possible.
