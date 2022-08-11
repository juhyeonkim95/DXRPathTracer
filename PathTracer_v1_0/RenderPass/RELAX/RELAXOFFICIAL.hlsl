/*
Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.

NVIDIA CORPORATION and its licensors retain all intellectual property
and proprietary rights in and to this software, related documentation
and any modifications thereto. Any use, reproduction, disclosure or
distribution of this software and related documentation without an express
license agreement from NVIDIA CORPORATION is strictly prohibited.
*/


float getJitterRadius(float jitterDelta, float linearZ)
{
    return jitterDelta * gUnproject * (gOrthoMode == 0 ? linearZ : 1.0);
}

float isReprojectionTapValid(float3 currentWorldPos, float3 previousWorldPos, float3 currentNormal, float disocclusionThreshold)
{
    // Check if plane distance is acceptable
    float3 posDiff = currentWorldPos - previousWorldPos;
    float maxPlaneDistance = abs(dot(posDiff, currentNormal));

    return maxPlaneDistance > disocclusionThreshold ? 0.0 : 1.0;
}

#define CHECK_GEOMETRY \
    reprojectionTapValid = isReprojectionTapValid(currentWorldPos, prevWorldPosInTap, currentNormal, disocclusionThreshold)

#define CHECK_MATERIAL_ID \
    reprojectionTapValid *= CompareMaterials(currentMaterialID, gPrevMaterialID[tapPos], materialIDMask)

// Returns reprojection search result based on surface motion:
// 2 - reprojection found, bicubic footprint was used
// 1 - reprojection found, bilinear footprint was used
// 0 - reprojection not found
//
// Also returns reprojected data from previous frame calculated using filtering based on filters above.
// For better performance, some data is filtered using weighed bilinear instead of bicubic even if all bicubic taps are valid.
float loadSurfaceMotionBasedPrevData(
    int2 pixelPosOnScreen,
    float3 currentWorldPos,
    float3 currentNormal,
    float currentLinearZ,
    float3 motionVector,
    float currentMaterialID,
    uint materialIDMask,
    out float footprintQuality,
    out float historyLength
#if( defined RELAX_DIFFUSE)
    , out float4 prevDiffuseIllumAnd2ndMoment
    , out float3 prevDiffuseResponsiveIllum
#endif
)
{
    // Calculating jitter margin radius in world space
    float jitterRadius = getJitterRadius(gJitterDelta, currentLinearZ);
    float disocclusionThreshold = (gDisocclusionDepthThreshold + jitterRadius) * (gOrthoMode == 0 ? currentLinearZ : 1.0);

    // Calculating previous pixel position and UV
    float2 uv = (pixelPosOnScreen + 0.5) * gInvRectSize;
    prevUV = STL::Geometry::GetPrevUvFromMotion(uv, currentWorldPos, gPrevWorldToClip, motionVector, gIsWorldSpaceMotionEnabled);
    float2 prevPixelPosOnScreen = prevUV * gRectSizePrev;

    // Consider reprojection to the same pixel index a small motion.
    // It is useful for skipping reprojection test for static camera when the jitter is the only source of motion.
    int2 prevPixelPosInt = int2(prevPixelPosOnScreen);
    bool isSmallMotion = all(prevPixelPosInt == pixelPosOnScreen);
    bool skipReprojectionTest = gSkipReprojectionTestWithoutMotion && isSmallMotion;

    // Calculating footprint origin and weights
    int2 bilinearOrigin = int2(floor(prevPixelPosOnScreen - 0.5));
    float2 bilinearWeights = frac(prevPixelPosOnScreen - 0.5);

    // Checking bicubic footprint (with cut corners)
    // remembering bilinear taps validity and worldspace position along the way,
    // for faster weighted bilinear and for calculating previous worldspace position
    // bc - bicubic & bilinear tap,
    // bl - bilinear tap
    //
    // -- bc bc --
    // bc bl bl bc
    // bc bl bl bc
    // -- bc bc --

    float bicubicFootprintValid = 1.0;
    float4 bilinearTapsValid = 0;

    float3 prevNormalInTap;
    float prevViewZInTap;
    float3 prevWorldPosInTap;
    int2 tapPos;
    float reprojectionTapValid;
    float3 prevWorldPos00, prevWorldPos10, prevWorldPos01, prevWorldPos11;
    float3 prevNormal00, prevNormal10, prevNormal01, prevNormal11;

    prevNormal00 = UnpackPrevNormalRoughness(gPrevNormalRoughness[bilinearOrigin + int2(0, 0)]).rgb;
    prevNormal10 = UnpackPrevNormalRoughness(gPrevNormalRoughness[bilinearOrigin + int2(1, 0)]).rgb;
    prevNormal01 = UnpackPrevNormalRoughness(gPrevNormalRoughness[bilinearOrigin + int2(0, 1)]).rgb;
    prevNormal11 = UnpackPrevNormalRoughness(gPrevNormalRoughness[bilinearOrigin + int2(1, 1)]).rgb;
    float3 prevNormalFlat = normalize(prevNormal00 + prevNormal10 + prevNormal01 + prevNormal11);
    prevNormalFlat = gUseWorldPrevToWorld ? STL::Geometry::RotateVector(gWorldPrevToWorld, prevNormalFlat) : prevNormalFlat;

    // Adjusting worldspace position:
    // Applying worldspace motion first,
    motionVector *= gIsWorldSpaceMotionEnabled > 0 ? 1.0 : 0.0;

    // Then taking care of camera motion, because world space is always centered at camera position in NRD
    currentWorldPos += motionVector - gPrevCameraPosition.xyz;

    // Transforming bilinearOrigin to clip space coords to simplify previous world pos calculation
    float2 prevClipSpaceXY = ((float2)bilinearOrigin + float2(0.5, 0.5)) * (1.0 / gRectSizePrev) * 2.0 - 1.0;
    float2 dXY = (2.0 / gRectSizePrev);

    // 1st row
    tapPos = bilinearOrigin + int2(0, -1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(0.0, -1.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    tapPos = bilinearOrigin + int2(1, -1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(1.0, -1.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    // 2nd row
    tapPos = bilinearOrigin + int2(-1, 0);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(-1.0, 0.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    tapPos = bilinearOrigin + int2(0, 0);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(0.0, 0.0), prevViewZInTap);
    prevWorldPos00 = prevWorldPosInTap;
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;
    bilinearTapsValid.x = reprojectionTapValid;

    tapPos = bilinearOrigin + int2(1, 0);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(1.0, 0.0), prevViewZInTap);
    prevWorldPos10 = prevWorldPosInTap;
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;
    bilinearTapsValid.y = reprojectionTapValid;

    tapPos = bilinearOrigin + int2(2, 0);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(2.0, 0.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    // 3rd row
    tapPos = bilinearOrigin + int2(-1, 1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(-1.0, 1.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    tapPos = bilinearOrigin + int2(0, 1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(0.0, 1.0), prevViewZInTap);
    prevWorldPos01 = prevWorldPosInTap;
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;
    bilinearTapsValid.z = reprojectionTapValid;

    tapPos = bilinearOrigin + int2(1, 1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(1.0, 1.0), prevViewZInTap);
    prevWorldPos11 = prevWorldPosInTap;
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;
    bilinearTapsValid.w = reprojectionTapValid;

    tapPos = bilinearOrigin + int2(2, 1);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(2.0, 1.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    // 4th row
    tapPos = bilinearOrigin + int2(0, 2);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(0.0, 2.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    tapPos = bilinearOrigin + int2(1, 2);
    prevViewZInTap = gPrevViewZ[tapPos];
    prevWorldPosInTap = GetPreviousWorldPosFromClipSpaceXY(prevClipSpaceXY + dXY * float2(1.0, 2.0), prevViewZInTap);
    CHECK_GEOMETRY;
    CHECK_MATERIAL_ID;
    bicubicFootprintValid *= reprojectionTapValid;

    bilinearTapsValid = skipReprojectionTest ? float4(1.0, 1.0, 1.0, 1.0) : bilinearTapsValid;

    // Reject backfacing history: if angle between current normal and previous normal is larger than 90 deg
    if (dot(currentNormal, prevNormalFlat) < 0.0)
    {
        bilinearTapsValid = 0;
        bicubicFootprintValid = 0;
    }

    // Checking bicubic footprint validity for being in rect
    if (any(bilinearOrigin < int2(1, 1)) || any(bilinearOrigin >= int2(gRectSizePrev)-int2(2, 2)))
        bicubicFootprintValid = 0;

    // Checking bilinear footprint validity for being in rect
    // Bilinear footprint:
    // x y
    // z w
    if (bilinearOrigin.x < 0)
        bilinearTapsValid.xz = 0;
    if (bilinearOrigin.x >= gRectSizePrev.x)
        bilinearTapsValid.yw = 0;
    if (bilinearOrigin.y < 0)
        bilinearTapsValid.xy = 0;
    if (bilinearOrigin.y >= gRectSizePrev.y)
        bilinearTapsValid.zw = 0;

    // Calculating bilinear weights in advance
    STL::Filtering::Bilinear bilinear;
    bilinear.weights = bilinearWeights;
    float4 bilinearWeightsWithValidity = STL::Filtering::GetBilinearCustomWeights(bilinear, float4(bilinearTapsValid.x, bilinearTapsValid.y, bilinearTapsValid.z, bilinearTapsValid.w));

    bool useBicubic = (bicubicFootprintValid > 0);

    // Fetching normal history
    BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights(
        prevPixelPosOnScreen, gInvResourceSize, bilinearWeightsWithValidity,
        gLinearClamp, useBicubic
#if( defined RELAX_DIFFUSE )
        , gPrevDiffuseIllumination, prevDiffuseIllumAnd2ndMoment
#endif

    );

    // Fetching fast history
    float4 spec;
    float4 diff;
    BicubicFilterNoCornersWithFallbackToBilinearFilterWithCustomWeights(
        prevPixelPosOnScreen, gInvResourceSize, bilinearWeightsWithValidity,
        gLinearClamp, useBicubic
#if( defined RELAX_DIFFUSE )
        , gPrevDiffuseIlluminationResponsive, diff
#endif
    );
#if( defined RELAX_DIFFUSE )
    prevDiffuseResponsiveIllum = diff.rgb;
#endif

    // Fitering previous data that does not need bicubic
    float interpolatedBinaryWeight = STL::Filtering::ApplyBilinearFilter(bilinearTapsValid.x, bilinearTapsValid.y, bilinearTapsValid.z, bilinearTapsValid.w, bilinear);
    interpolatedBinaryWeight = max(1e-6, interpolatedBinaryWeight);

    float2 gatherOrigin = (bilinearOrigin + 1.0) * gInvResourceSize;

    float4 prevHistoryLengths = gPrevHistoryLength.GatherRed(gNearestClamp, gatherOrigin).wzxy;
    historyLength = 255.0 * BilinearWithBinaryWeightsImmediateFloat(
        prevHistoryLengths.x,
        prevHistoryLengths.y,
        prevHistoryLengths.z,
        prevHistoryLengths.w,
        bilinearWeights, bilinearTapsValid, interpolatedBinaryWeight);

    float reprojectionFound = (bicubicFootprintValid > 0) ? 2.0 : 1.0;
    footprintQuality = (bicubicFootprintValid > 0) ? 1.0 : interpolatedBinaryWeight;
    if (!any(bilinearTapsValid))
    {
        reprojectionFound = 0;
        footprintQuality = 0;
    }

    return reprojectionFound;
}

// Returns specular reprojection search result based on virtual motion
void Preload(uint2 sharedPos, int2 globalPos)
{
    globalPos = clamp(globalPos, 0, gRectSize - 1.0);

    float4 normalRoughness = NRD_FrontEnd_UnpackNormalAndRoughness(gNormalRoughness[globalPos + gRectOrigin]);
    sharedNormalRoughness[sharedPos.y][sharedPos.x] = normalRoughness;
}

// Main
[numthreads(GROUP_X, GROUP_Y, 1)]
NRD_EXPORT void NRD_CS_MAIN(uint2 pixelPos : SV_DispatchThreadId, uint2 threadPos : SV_GroupThreadId, uint threadIndex : SV_GroupIndex)
{

    PRELOAD_INTO_SMEM;

    // Calculating checkerboard fields
    uint2 checkerboardPixelPos = pixelPos.xx;
    uint checkerboard = STL::Sequence::CheckerBoard(pixelPos, gFrameIndex);

#if( defined RELAX_DIFFUSE)
    bool diffHasData = true;
    if (gDiffCheckerboard != 2)
    {
        diffHasData = checkerboard == gDiffCheckerboard;
        checkerboardPixelPos.x >>= 1;
    }
#endif


    // Early out if linearZ is beyond denoising range
    float currentLinearZ = gViewZ[pixelPos.xy + gRectOrigin];

    [branch]
    if (currentLinearZ > gDenoisingRange)
        return;

    uint2 sharedMemoryIndex = threadPos.xy + int2(BORDER, BORDER);

    // Center data
#if( defined RELAX_DIFFUSE)
    float3 diffuseIllumination = gDiffuseRadiance[pixelPos.xy + gRectOrigin].rgb;
#endif

    // Reading current GBuffer data
    float4 currentNormalRoughness = sharedNormalRoughness[sharedMemoryIndex.y][sharedMemoryIndex.x];
    float3 currentNormal = currentNormalRoughness.xyz;
    float currentRoughness = currentNormalRoughness.w;

    // Handling materialID
    // Combining logic is valid even if non-combined denoisers are used
    // since undefined masks are zeroes in those cases
    float currentMaterialID;
    NRD_FrontEnd_UnpackNormalAndRoughness(gNormalRoughness[gRectOrigin + pixelPos], currentMaterialID);
    currentMaterialID = floor(currentMaterialID * 3.0 + 0.5) / 255.0; // IMPORTANT: properly repack 2-bits to 8-bits

    // Getting current frame worldspace position and view vector for current pixel
    float3 currentWorldPos = GetCurrentWorldPosFromPixelPos(pixelPos, currentLinearZ);
    float3 currentViewVector = (gOrthoMode == 0) ?
        currentWorldPos :
        currentLinearZ * normalize(gFrustumForward.xyz);

    float3 currentNormalAveraged = currentNormal;

    [unroll]
    for (int k = -1; k <= 1; k++)
    {
        [unroll]
        for (int l = -1; l <= 1; l++)
        {
            // Skipping center pixel
            if ((k == 0) && (l == 0))
                continue;

            float3 pNormal = sharedNormalRoughness[sharedMemoryIndex.y + k][sharedMemoryIndex.x + l].xyz;
            currentNormalAveraged += pNormal;

        }
    }
    currentNormalAveraged /= 9.0;


#if( defined RELAX_DIFFUSE )
    float diffuse1stMoment = STL::Color::Luminance(diffuseIllumination.rgb);
    float diffuse2ndMoment = diffuse1stMoment * diffuse1stMoment;
#endif

    // Reading motion vector
    float3 motionVector = gMotion[gRectOrigin + pixelPos].xyz * gMotionVectorScale.xyy;

    // Loading previous data based on surface motion vectors
    float footprintQuality;
#if( defined RELAX_DIFFUSE )
    float4 prevDiffuseIlluminationAnd2ndMomentSMB;
    float3 prevDiffuseIlluminationAnd2ndMomentSMBResponsive;
#endif

    float historyLength;

    float surfaceMotionBasedReprojectionFound = loadSurfaceMotionBasedPrevData(
        pixelPos,
        currentWorldPos,
        normalize(currentNormalAveraged),
        currentLinearZ,
        motionVector,
        currentMaterialID,
        gDiffMaterialMask | gSpecMaterialMask, // TODO: improve?
        footprintQuality,
        historyLength
#if( defined RELAX_DIFFUSE)
        , prevDiffuseIlluminationAnd2ndMomentSMB
        , prevDiffuseIlluminationAnd2ndMomentSMBResponsive
#endif
    );

    // History length is based on surface motion based disocclusion
    historyLength = historyLength + 1.0;
    historyLength = min(RELAX_MAX_ACCUM_FRAME_NUM, historyLength);

    // Avoid footprint momentary stretching due to changed viewing angle
    float3 prevWorldPos = currentWorldPos + motionVector * float(gIsWorldSpaceMotionEnabled != 0);
    float3 Vprev = normalize(prevWorldPos - gPrevCameraPosition.xyz);
    float VoNflat = abs(dot(currentNormalAveraged, normalize(currentViewVector))) + 1e-3;
    float VoNflatprev = abs(dot(currentNormalAveraged, Vprev)) + 1e-3;
    float sizeQuality = VoNflatprev / VoNflat; // this order because we need to fix stretching only, shrinking is OK
    sizeQuality *= sizeQuality;
    sizeQuality *= sizeQuality;
    footprintQuality *= lerp(0.1, 1.0, saturate(sizeQuality + abs(gOrthoMode)));

    // Minimize "getting stuck in history" effect when only fraction of bilinear footprint is valid
    // by shortening the history length
    if (footprintQuality < 1.0)
    {
        historyLength *= sqrt(footprintQuality);
        historyLength = max(historyLength, 1.0);
    }

    // Handling history reset if needed
    if (gResetHistory != 0)
        historyLength = 1.0;

#if( defined RELAX_DIFFUSE )
    // DIFFUSE ACCUMULATION BELOW
    //
    // Temporal accumulation of diffuse illumination
    float diffMaxAccumulatedFrameNum = gDiffuseMaxAccumulatedFrameNum;
    float diffMaxFastAccumulatedFrameNum = gDiffuseMaxFastAccumulatedFrameNum;

    if (gUseConfidenceInputs)
    {
        float inDiffConfidence = gDiffConfidence[pixelPos];
        diffMaxAccumulatedFrameNum *= inDiffConfidence;
        diffMaxFastAccumulatedFrameNum *= inDiffConfidence;
    }

    float diffHistoryLength = historyLength;

    float diffuseAlpha = (surfaceMotionBasedReprojectionFound > 0) ? max(1.0 / (diffMaxAccumulatedFrameNum + 1.0), 1.0 / diffHistoryLength) : 1.0;
    float diffuseAlphaResponsive = (surfaceMotionBasedReprojectionFound > 0) ? max(1.0 / (diffMaxFastAccumulatedFrameNum + 1.0), 1.0 / diffHistoryLength) : 1.0;

    if ((!diffHasData) && (diffHistoryLength > 1.0))
    {
        // Adjusting diffuse accumulation weights for checkerboard
        diffuseAlpha *= 1.0 - gCheckerboardResolveAccumSpeed;
        diffuseAlphaResponsive *= 1.0 - gCheckerboardResolveAccumSpeed;
    }

    float4 accumulatedDiffuseIlluminationAnd2ndMoment = lerp(prevDiffuseIlluminationAnd2ndMomentSMB, float4(diffuseIllumination.rgb, diffuse2ndMoment), diffuseAlpha);
    float3 accumulatedDiffuseIlluminationResponsive = lerp(prevDiffuseIlluminationAnd2ndMomentSMBResponsive.rgb, diffuseIllumination.rgb, diffuseAlphaResponsive);

    // Write out the diffuse results
    gOutDiffuseIllumination[pixelPos] = accumulatedDiffuseIlluminationAnd2ndMoment;
    gOutDiffuseIlluminationResponsive[pixelPos] = float4(accumulatedDiffuseIlluminationResponsive, 0);
#endif

    gOutHistoryLength[pixelPos] = historyLength / 255.0;
}