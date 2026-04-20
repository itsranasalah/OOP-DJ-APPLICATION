/* =========================================================================================

   This is an auto-generated file: Any edits you make may be overwritten!

*/

#pragma once

namespace BinaryData
{
    extern const char*   beat1_mp3;
    const int            beat1_mp3Size = 17044;

    extern const char*   beat2_mp3;
    const int            beat2_mp3Size = 2415;

    extern const char*   beat3_mp3;
    const int            beat3_mp3Size = 9110;

    extern const char*   beat4_mp3;
    const int            beat4_mp3Size = 3774;

    extern const char*   beat5_mp3;
    const int            beat5_mp3Size = 2836;

    extern const char*   beat6_mp3;
    const int            beat6_mp3Size = 176013;

    extern const char*   beat7_mp3;
    const int            beat7_mp3Size = 8418;

    extern const char*   beat8_mp3;
    const int            beat8_mp3Size = 24270;

    extern const char*   beat9_mp3;
    const int            beat9_mp3Size = 15908;

    extern const char*   beat10_mp3;
    const int            beat10_mp3Size = 5182;

    extern const char*   beat11_mp3;
    const int            beat11_mp3Size = 13889;

    extern const char*   beat12_mp3;
    const int            beat12_mp3Size = 11262;

    // Number of elements in the namedResourceList and originalFileNames arrays.
    const int namedResourceListSize = 12;

    // Points to the start of a list of resource names.
    extern const char* namedResourceList[];

    // Points to the start of a list of resource filenames.
    extern const char* originalFilenames[];

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding data and its size (or a null pointer if the name isn't found).
    const char* getNamedResource (const char* resourceNameUTF8, int& dataSizeInBytes);

    // If you provide the name of one of the binary resource variables above, this function will
    // return the corresponding original, non-mangled filename (or a null pointer if the name isn't found).
    const char* getNamedResourceOriginalFilename (const char* resourceNameUTF8);
}
