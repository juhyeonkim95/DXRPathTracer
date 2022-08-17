namespace ToneMapperOperator
{
    static const uint Linear = 0;
    static const uint Reinhard = 1;
    static const uint ReinhardModified = 2;
    static const uint HejiHableAlu = 3;
    static const uint HableUc2 = 4;
    static const uint Aces = 5;
}

/** Tone mapper parameters shared between host and device.
    Make sure struct layout follows the HLSL packing rules as it is uploaded as a memory blob.
    Do not use bool's as they are 1 byte in Visual Studio, 4 bytes in HLSL.
    https://msdn.microsoft.com/en-us/library/windows/desktop/bb509632(v=vs.85).aspx
*/
struct ToneMapperParams
{
    int mode;
    int srgbConversion;
    float whiteScale;
    float whiteMaxLuminance;
};
