// colorin_matrix.cpp - Extract exact cam→XYZ(D50) matrix as DT/LCMS does
//
// Clean copy of DT's matrix computation for STANDARD_MATRIX profile.
// Uses LCMS2 to replicate the exact profile creation.

#include <cstdio>
#include <cmath>
#include <lcms2.h>
#include <libraw/libraw.h>

// Mat3 inversion (from DT colorspaces.c)
static void mat3inv(float* const dst, const float* const src)
{
    const float det = src[0] * (src[4] * src[8] - src[5] * src[7])
                    - src[1] * (src[3] * src[8] - src[5] * src[6])
                    + src[2] * (src[3] * src[7] - src[4] * src[6]);

    const float epsilon = 1e-20f;
    if(fabsf(det) < epsilon) {
        // Singular matrix, set to identity
        dst[0] = 1; dst[1] = 0; dst[2] = 0;
        dst[3] = 0; dst[4] = 1; dst[5] = 0;
        dst[6] = 0; dst[7] = 0; dst[8] = 1;
        return;
    }

    const float invDet = 1.0f / det;
    dst[0] =  (src[4] * src[8] - src[7] * src[5]) * invDet;
    dst[1] = -(src[1] * src[8] - src[7] * src[2]) * invDet;
    dst[2] =  (src[1] * src[5] - src[4] * src[2]) * invDet;
    dst[3] = -(src[3] * src[8] - src[6] * src[5]) * invDet;
    dst[4] =  (src[0] * src[8] - src[6] * src[2]) * invDet;
    dst[5] = -(src[0] * src[5] - src[3] * src[2]) * invDet;
    dst[6] =  (src[3] * src[7] - src[6] * src[4]) * invDet;
    dst[7] = -(src[0] * src[7] - src[6] * src[1]) * invDet;
    dst[8] =  (src[0] * src[4] - src[3] * src[1]) * invDet;
}

// Create profile exactly as DT does (from colorspaces.c:930)
static cmsHPROFILE dt_colorspaces_create_xyzmatrix_profile(const float mat[3][3])
{
    // mat: cam -> xyz (D65)
    float x[3], y[3];
    for(int k = 0; k < 3; k++)
    {
        const float norm = mat[0][k] + mat[1][k] + mat[2][k];
        x[k] = mat[0][k] / norm;
        y[k] = mat[1][k] / norm;
    }

    cmsCIExyYTRIPLE CameraPrimaries = {
        { x[0], y[0], 1.0 },
        { x[1], y[1], 1.0 },
        { x[2], y[2], 1.0 }
    };

    cmsCIEXYZ d65 = { 0.95045471, 1.0, 1.08905029 };  // From DT
    cmsCIExyY D65;
    cmsXYZ2xyY(&D65, &d65);

    cmsToneCurve *Gamma[3];
    Gamma[0] = Gamma[1] = Gamma[2] = cmsBuildGamma(NULL, 1.0);
    cmsHPROFILE profile = cmsCreateRGBProfile(&D65, &CameraPrimaries, Gamma);
    cmsFreeToneCurve(Gamma[0]);

    return profile;
}

// Create profile from XYZ→CAM matrix (from colorspaces.c:974)
static cmsHPROFILE dt_colorspaces_create_xyzimatrix_profile(float mat[3][3])
{
    // mat: xyz -> cam
    float imat[3][3];
    mat3inv((float*)imat, (float*)mat);
    return dt_colorspaces_create_xyzmatrix_profile(imat);
}

int main()
{
    const char* raw_file = "src/test/DSC00144.ARW";

    // Load with LibRaw to get cam_xyz matrix
    LibRaw raw;
    if (raw.open_file(raw_file) != LIBRAW_SUCCESS) {
        printf("Failed to open raw file\n");
        return 1;
    }
    raw.unpack();

    // Get adobe_XYZ_to_CAM (same as LibRaw's cam_xyz)
    printf("=== Input: adobe_XYZ_to_CAM (XYZ→CAM) ===\n");
    float xyz_to_cam[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            xyz_to_cam[i][j] = raw.imgdata.rawdata.color.cam_xyz[i][j];
            printf("  %10.6f", xyz_to_cam[i][j]);
        }
        printf("\n");
    }

    // Invert to get CAM→XYZ (D65)
    float cam_to_xyz[3][3];
    mat3inv((float*)cam_to_xyz, (float*)xyz_to_cam);
    printf("\n=== Inverted: cam→XYZ (D65) ===\n");
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            printf("  %10.6f", cam_to_xyz[i][j]);
        }
        printf("\n");
    }

    // Create profile as DT does
    cmsHPROFILE profile = dt_colorspaces_create_xyzimatrix_profile(xyz_to_cam);
    if (!profile) {
        printf("Failed to create profile\n");
        return 1;
    }

    // Read colorant tags (these are RGB→XYZ D50, as stored by LCMS)
    cmsCIEXYZ *R = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigRedColorantTag);
    cmsCIEXYZ *G = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigGreenColorantTag);
    cmsCIEXYZ *B = (cmsCIEXYZ*)cmsReadTag(profile, cmsSigBlueColorantTag);

    if (!R || !G || !B) {
        printf("Failed to read colorant tags\n");
        cmsCloseProfile(profile);
        return 1;
    }

    printf("\n=== LCMS Profile Colorants (cam→XYZ D50) ===\n");
    printf("Row 0 (X): %10.8f %10.8f %10.8f\n", R->X, G->X, B->X);
    printf("Row 1 (Y): %10.8f %10.8f %10.8f\n", R->Y, G->Y, B->Y);
    printf("Row 2 (Z): %10.8f %10.8f %10.8f\n", R->Z, G->Z, B->Z);

    // This is the matrix to use in colorin.cpp!
    printf("\n=== C++ Matrix for colorin.cpp ===\n");
    printf("static constexpr float CAM_TO_XYZ_D50_ILCE7M3[9] = {\n");
    printf("     %.8ff,  %.8ff,  %.8ff,\n", R->X, G->X, B->X);
    printf("     %.8ff,  %.8ff,  %.8ff,\n", R->Y, G->Y, B->Y);
    printf("     %.8ff,  %.8ff,  %.8ff\n", R->Z, G->Z, B->Z);
    printf("};\n");

    // Verify: (1,1,1) should map to D50
    printf("\n=== Verification: (1,1,1) → XYZ ===\n");
    double xyz[3] = {
        R->X + G->X + B->X,
        R->Y + G->Y + B->Y,
        R->Z + G->Z + B->Z
    };
    printf("Result: X=%.6f Y=%.6f Z=%.6f\n", xyz[0], xyz[1], xyz[2]);
    printf("D50:    X=0.964200 Y=1.000000 Z=0.824900\n");

    cmsCloseProfile(profile);
    return 0;
}
