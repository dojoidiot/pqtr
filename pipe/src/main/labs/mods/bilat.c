/*
 * bilat (local contrast) - COPIED from darktable locallaplacian.c
 *
 * This is a direct copy with DT macros replaced by standard C.
 */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

/* Inlined types (GPU shader compatible) */
#ifndef DT_ALIGNED_PIXEL_T_DEFINED
#define DT_ALIGNED_PIXEL_T_DEFINED
typedef float dt_aligned_pixel_t[4];
#endif

#ifndef FOR_FOUR_CHANNELS_DEFINED
#define FOR_FOUR_CHANNELS_DEFINED
#define for_four_channels(c) for(int c = 0; c < 4; c++)
#endif

#ifndef DT_ALLOC_ALIGN_FLOAT_DEFINED
#define DT_ALLOC_ALIGN_FLOAT_DEFINED
static inline float* dt_alloc_align_float(size_t count)
{
    return (float*)malloc(count * sizeof(float));
}
static inline void dt_free_align(void* ptr)
{
    free(ptr);
}
#endif

/* Replace DT macros */
#define DT_OMP_FOR()
#define dt_omploop_sfence()
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define CLAMP(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))
#define CLAMPS(x, lo, hi) ((x) < (lo) ? (lo) : ((x) > (hi) ? (hi) : (x)))

#ifndef DT_FAST_EXPF_DEFINED
#define DT_FAST_EXPF_DEFINED
static inline float dt_fast_expf(float x) { return expf(x); }
#endif

/* Count leading zeros - portable version */
static inline int my_clz(unsigned int x) {
    if(x == 0) return 32;
    int n = 0;
    if((x & 0xFFFF0000) == 0) { n += 16; x <<= 16; }
    if((x & 0xFF000000) == 0) { n += 8; x <<= 8; }
    if((x & 0xF0000000) == 0) { n += 4; x <<= 4; }
    if((x & 0xC0000000) == 0) { n += 2; x <<= 2; }
    if((x & 0x80000000) == 0) { n += 1; }
    return n;
}
#define __builtin_clz my_clz

// the maximum number of levels for the gaussian pyramid
#define max_levels 30
// the number of segments for the piecewise linear interpolation
#define num_gamma 6

// downsample width/height to given level
static inline int dl(int size, const int level)
{
  for(int l=0;l<level;l++)
    size = (size-1)/2+1;
  return size;
}

// needs a boundary of 1 or 2px around i,j or else it will crash.
// (translates to a 1px boundary around the corresponding pixel in the coarse buffer)
// more precisely, 1<=i<wd-1 for even wd and
//                 1<=i<wd-2 for odd wd (j likewise with ht)
static inline float ll_expand_gaussian(
    const float *const coarse,
    const int i,
    const int j,
    const int wd,
    const int ht)
{
  const int cw = (wd-1)/2+1;
  const int ind = (j/2)*cw+i/2;
  // case 0:     case 1:     case 2:     case 3:
  //  x . x . x   x . x . x   x . x . x   x . x . x
  //  . . . . .   . . . . .   . .[.]. .   .[.]. . .
  //  x .[x]. x   x[.]x . x   x . x . x   x . x . x
  //  . . . . .   . . . . .   . . . . .   . . . . .
  //  x . x . x   x . x . x   x . x . x   x . x . x
  switch((i&1) + 2*(j&1))
  {
    case 0: // both are even, 3x3 stencil
      return 4./256. * (
          6.0f*(coarse[ind-cw] + coarse[ind-1] + 6.0f*coarse[ind] + coarse[ind+1] + coarse[ind+cw])
          + coarse[ind-cw-1] + coarse[ind-cw+1] + coarse[ind+cw-1] + coarse[ind+cw+1]);
    case 1: // i is odd, 2x3 stencil
      return 4./256. * (
          24.0*(coarse[ind] + coarse[ind+1]) +
          4.0*(coarse[ind-cw] + coarse[ind-cw+1] + coarse[ind+cw] + coarse[ind+cw+1]));
    case 2: // j is odd, 3x2 stencil
      return 4./256. * (
          24.0*(coarse[ind] + coarse[ind+cw]) +
          4.0*(coarse[ind-1] + coarse[ind+1] + coarse[ind+cw-1] + coarse[ind+cw+1]));
    default: // case 3: // both are odd, 2x2 stencil
      return .25f * (coarse[ind] + coarse[ind+1] + coarse[ind+cw] + coarse[ind+cw+1]);
  }
}

// helper to fill in one pixel boundary by copying it
static inline void ll_fill_boundary1(
    float *const input,
    const int wd,
    const int ht)
{
  for(int j=1;j<ht-1;j++) input[j*wd] = input[j*wd+1];
  for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2];
  memcpy(input,    input+wd, sizeof(float)*wd);
  memcpy(input+wd*(ht-1), input+wd*(ht-2), sizeof(float)*wd);
}

// helper to fill in two pixels boundary by copying it
static inline void ll_fill_boundary2(
    float *const input,
    const int wd,
    const int ht)
{
  for(int j=1;j<ht-1;j++) input[j*wd] = input[j*wd+1];
  if(wd & 1) for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2];
  else       for(int j=1;j<ht-1;j++) input[j*wd+wd-1] = input[j*wd+wd-2] = input[j*wd+wd-3];
  memcpy(input, input+wd, sizeof(float)*wd);
  if(!(ht & 1)) memcpy(input+wd*(ht-2), input+wd*(ht-3), sizeof(float)*wd);
  memcpy(input+wd*(ht-1), input+wd*(ht-2), sizeof(float)*wd);
}

static void pad_by_replication(
    float *buf,			// the buffer to be padded
    const uint32_t w,		// width of a line
    const uint32_t h,		// total height, including top and bottom padding
    const uint32_t padding)	// number of lines of padding on each side
{
  DT_OMP_FOR()
  for(int j=0;j<(int)padding;j++)
  {
    memcpy(buf + w*j, buf+padding*w, sizeof(float)*w);
    memcpy(buf + w*(h-padding+j), buf+w*(h-padding-1), sizeof(float)*w);
  }
}

static inline void gauss_expand(
    const float *const input, // coarse input
    float *const fine,        // upsampled, blurry output
    const int wd,             // fine res
    const int ht)
{
  DT_OMP_FOR()
  for(int j=1;j<((ht-1)&~1);j++)  // even ht: two px boundary. odd ht: one px.
    for(int i=1;i<((wd-1)&~1);i++)
      fine[j*wd+i] = ll_expand_gaussian(input, i, j, wd, ht);
  ll_fill_boundary2(fine, wd, ht);
}

static inline void _convolve_14641_vert(dt_aligned_pixel_t conv, const float *in, const size_t wd)
{
  static const dt_aligned_pixel_t four = { 4.f, 4.f, 4.f, 4.f };
  dt_aligned_pixel_t r0, r1, r2, r3, r4;
  for_four_channels(c)
  {
    // 'in' is only 4-byte aligned, so we can't use copy_pixel here
    r0[c] = in[c];
    r1[c] = in[wd+c];
    r2[c] = in[2*wd+c];
    r3[c] = in[3*wd+c];
    r4[c] = in[4*wd+c];
  }
  dt_aligned_pixel_t t;
  for_four_channels(c)
  {
    r0[c] = r0[c] + r4[c];		// r0 = r0+r4
    r1[c] = r1[c] + r2[c] + r3[c];	// r1 = r1+r2+r2
    r0[c] = r0[c] + r2[c] + r2[c];	// r0 = r0 + 2*r2 * r4
    t[c] = r1[c] * four[c];		// t = 4*r1 + 4*r2 + r*43
    conv[c] = r0[c] + t[c];		// conv = r0 + 4*r1 + 6*r2 + 4*r3 + r4
  }
}

static inline void gauss_reduce(
    const float *const input, // fine input buffer
    float *const coarse,      // coarse scale, blurred input buf
    const size_t wd,             // fine res
    const size_t ht)
{
  // blur, store only coarse res
  const size_t cw = (wd-1)/2+1, ch = (ht-1)/2+1;
  // DON'T parallelize the very smallest levels of the pyramid, as the threading overhead
  // is greater than the time needed to do it sequentially
  DT_OMP_FOR()
  for(size_t j=1;j<ch-1;j++)
  {
    const float *base = input + 2*(j-1)*wd;
    float *const out = coarse + j*cw + 1;
    // prime the vertical axis
    static const dt_aligned_pixel_t kernel = { 1.0f, 4.0f, 6.0f, 4.0f };
    dt_aligned_pixel_t left;
    _convolve_14641_vert(left,base,wd);
    for(size_t col=0; col<cw-3; col += 2)
    {
      // convolve the next four pixel wide vertical slice
      base += 4;
      dt_aligned_pixel_t right;
      _convolve_14641_vert(right,base,wd);
      // horizontal pass, generate two output values from convolving with 1 4 6 4 1
      // the first uses pixels 0-4, the second uses 2-6
      dt_aligned_pixel_t conv;
      for_four_channels(c)
        conv[c] = left[c] * kernel[c];
      out[col] = (conv[0] + conv[1] + conv[2] + conv[3] + right[0]) / 256.0f;
      out[col+1] = (left[2] + 4*(left[3]+right[1]) + 6.0f*right[0] + right[2]) / 256.0f;
      // shift to next pair of output columns (four input columns)
      copy_pixel(left, right);
    }
    // handle the left-over pixel if the output size is odd
    if(cw % 2)
    {
      base += 4;
      // convolve the right-most column
      float right = base[0] + 4.0f*(base[wd]+base[3*wd]) + 6.0f*base[2*wd] + base[4*wd];
      dt_aligned_pixel_t conv;
      for_four_channels(c)
        conv[c] = left[c] * kernel[c];
      out[cw-3] = (conv[0] + conv[1] + conv[2] + conv[3] + right) / 256.0f;
    }
  }
  dt_omploop_sfence();
  ll_fill_boundary1(coarse, cw, ch);
}

// allocate output buffer with monochrome brightness channel from input, padded
// up by max_supp on all four sides, dimensions written to wd2 ht2
static inline float *ll_pad_input(
    const float *const input,
    const int wd,
    const int ht,
    const int max_supp,
    int *wd2,
    int *ht2)
{
  const int stride = 4;
  *wd2 = 2*max_supp + wd;
  *ht2 = 2*max_supp + ht;
  float *const out = dt_alloc_align_float((size_t) *wd2 * *ht2);
  if(!out) return NULL;

  // pad by replication:
  DT_OMP_FOR()
  for(int j=0;j<ht;j++)
  {
    for(int i=0;i<max_supp;i++)
      out[(j+max_supp)**wd2+i] = input[stride*wd*j]* 0.01f; // L -> [0,1]
    for(int i=0;i<wd;i++)
      out[(j+max_supp)**wd2+i+max_supp] = input[stride*(wd*j+i)] * 0.01f; // L -> [0,1]
    for(int i=wd+max_supp;i<*wd2;i++)
      out[(j+max_supp)**wd2+i] = input[stride*(j*wd+wd-1)] * 0.01f; // L -> [0,1]
  }
  pad_by_replication(out, *wd2, *ht2, max_supp);
  return out;
}


static inline float ll_laplacian(
    const float *const coarse,   // coarse res gaussian
    const float *const fine,     // fine res gaussian
    const int i,                 // fine index
    const int j,
    const int wd,                // fine width
    const int ht)                // fine height
{
  const float c = ll_expand_gaussian(coarse,
      CLAMPS(i, 1, ((wd-1)&~1)-1), CLAMPS(j, 1, ((ht-1)&~1)-1), wd, ht);
  return fine[j*wd+i] - c;
}

static inline float curve_scalar(
    const float x,
    const float g,
    const float sigma,
    const float shadows,
    const float highlights,
    const float clarity)
{
  const float c = x-g;
  float val;
  // blend in via quadratic bezier
  if     (c >  2*sigma) val = g + sigma + shadows    * (c-sigma);
  else if(c < -2*sigma) val = g - sigma + highlights * (c+sigma);
  else if(c > 0.0f)
  { // shadow contrast
    const float t = CLAMPS(c / (2.0f*sigma), 0.0f, 1.0f);
    const float t2 = t * t;
    const float mt = 1.0f-t;
    val = g + sigma * 2.0f*mt*t + t2*(sigma + sigma*shadows);
  }
  else
  { // highlight contrast
    const float t = CLAMPS(-c / (2.0f*sigma), 0.0f, 1.0f);
    const float t2 = t * t;
    const float mt = 1.0f-t;
    val = g - sigma * 2.0f*mt*t + t2*(- sigma - sigma*highlights);
  }
  // midtone local contrast
  val += clarity * c * dt_fast_expf(-c*c/(2.0f*sigma*sigma/3.0f));
  return val;
}

static void apply_curve(
    float *const out,
    const float *const in,
    const uint32_t w,
    const uint32_t h,
    const uint32_t padding,
    const float g,
    const float sigma,
    const float shadows,
    const float highlights,
    const float clarity)
{
  DT_OMP_FOR()
  for(uint32_t j=padding;j<h-padding;j++)
  {
    const float *in2  = in  + j*w + padding;
    float *out2 = out + j*w + padding;
    for(uint32_t i=padding;i<w-padding;i++)
      (*out2++) = curve_scalar(*(in2++), g, sigma, shadows, highlights, clarity);
    out2 = out + j*w;
    for(uint32_t i=0;i<padding;i++)   out2[i] = out2[padding];
    for(uint32_t i=w-padding;i<w;i++) out2[i] = out2[w-padding-1];
  }
  pad_by_replication(out, w, h, padding);
}

static void local_laplacian_internal(
    const float *const input,   // input buffer in some Labx or yuvx format
    float *const out,           // output buffer with colour
    const int wd,               // width and
    const int ht,               // height of the input buffer
    const float sigma,          // user param: separate shadows/mid-tones/highlights
    const float shadows,        // user param: lift shadows
    const float highlights,     // user param: compress highlights
    const float clarity)        // user param: increase clarity/local contrast
{
  if(wd <= 1 || ht <= 1) return;

  // don't divide by 2 more often than we can:
  const int num_levels = MIN(max_levels, 31-__builtin_clz(MIN(wd,ht)));
  int last_level = num_levels-1;
  const int max_supp = 1<<last_level;
  int w, h;
  float *padded[max_levels] = {0};
  padded[0] = ll_pad_input(input, wd, ht, max_supp, &w, &h);

  // allocate pyramid pointers for padded input
  int success = padded[0] != NULL;
  for(int l=1;l<=last_level;l++)
  {
    padded[l] = dt_alloc_align_float((size_t)dl(w,l) * dl(h,l));
    if(!padded[l])
    {
      success = 0;
      break;
    }
  }

  // allocate pyramid pointers for output
  float *output[max_levels] = {0};
  for(int l=0;l<=last_level;l++)
  {
    output[l] = dt_alloc_align_float((size_t)dl(w,l) * dl(h,l));
    if(!output[l])
    {
      success = 0;
      break;
    }
  }

  if(!success)
  {
    // we can't jump to cleanup from here because it would reference a
    // variable which hasn't been initialized yet because it is
    // declared below.  So just free whatever we've allocated and return.
    for(int l = 0; l <= last_level; l++)
    {
      dt_free_align(padded[l]);
      dt_free_align(output[l]);
    }
    // copy the input buffer to the output so that we at least get a
    // valid result
    for(size_t k = 0; k < (size_t)4 * wd * ht; k++)
      out[k] = input[k];
    return;
  }

  // create gauss pyramid of padded input, write coarse directly to output
  for(int l=1;l<last_level;l++)
    gauss_reduce(padded[l-1], padded[l], dl(w,l-1), dl(h,l-1));
  gauss_reduce(padded[last_level-1], output[last_level], dl(w,last_level-1), dl(h,last_level-1));

  // evenly sample brightness [0,1]:
  float gamma[num_gamma] = {0.0f};
  for(int k=0;k<num_gamma;k++) gamma[k] = (k+.5f)/(float)num_gamma;
  // for(int k=0;k<num_gamma;k++) gamma[k] = k/(num_gamma-1.0f);

  // allocate memory for intermediate laplacian pyramids
  float *buf[num_gamma][max_levels];
  memset(buf, 0, sizeof(buf));
  for(int k=0;k<num_gamma;k++)
    for(int l=0;l<=last_level;l++)
    {
      buf[k][l] = dt_alloc_align_float((size_t)dl(w,l)*dl(h,l));
      if(!buf[k][l])
      {
        // copy the input buffer to the output so that we at least get a
        // valid result
        for(size_t p = 0; p < (size_t)4 * wd * ht; p++)
          out[p] = input[p];
        goto cleanup;
      }
    }

  // the paper says remapping only level 3 not 0 does the trick, too
  // (but i really like the additional octave of sharpness we get,
  // willing to pay the cost).
  for(int k=0;k<num_gamma;k++)
  { // process images
    apply_curve(buf[k][0], padded[0], w, h, max_supp, gamma[k], sigma, shadows, highlights, clarity);

    // create gaussian pyramids
    for(int l=1;l<=last_level;l++)
      gauss_reduce(buf[k][l-1], buf[k][l], dl(w,l-1), dl(h,l-1));
  }

  // assemble output pyramid coarse to fine
  for(int l=last_level-1;l >= 0; l--)
  {
    const int pw = dl(w,l), ph = dl(h,l);

    gauss_expand(output[l+1], output[l], pw, ph);
    // go through all coefficients in the upsampled gauss buffer:
    DT_OMP_FOR()
    for(int j=0;j<ph;j++) for(int i=0;i<pw;i++)
    {
      const float v = padded[l][j*pw+i];
      int hi = 1;
      for(;hi<num_gamma-1 && gamma[hi] <= v;hi++);
      int lo = hi-1;
      const float a = CLAMPS((v - gamma[lo])/(gamma[hi]-gamma[lo]), 0.0f, 1.0f);
      const float l0 = ll_laplacian(buf[lo][l+1], buf[lo][l], i, j, pw, ph);
      const float l1 = ll_laplacian(buf[hi][l+1], buf[hi][l], i, j, pw, ph);
      output[l][j*pw+i] += l0 * (1.0f-a) + l1 * a;
      // we could do this to save on memory (no need for finest buf[][]).
      // unfortunately it results in a quite noticeable loss of sharpness, i think
      // the extra level is worth it.
      // else if(l == 0) // use finest scale from input to not amplify noise (and use less memory)
      //   output[l][j*pw+i] += ll_laplacian(padded[l+1], padded[l], i, j, pw, ph);
    }
  }
  DT_OMP_FOR()
  for(int j=0;j<ht;j++) for(int i=0;i<wd;i++)
  {
    out[4*(j*wd+i)+0] = 100.0f * output[0][(j+max_supp)*w+max_supp+i]; // [0,1] -> L
    out[4*(j*wd+i)+1] = input[4*(j*wd+i)+1]; // copy original colour channels
    out[4*(j*wd+i)+2] = input[4*(j*wd+i)+2];
  }

  // free all buffers
cleanup:
  for(int l=0;l<max_levels;l++)
  {
    dt_free_align(padded[l]);
    dt_free_align(output[l]);
    for(int k=0; k<num_gamma;k++) dt_free_align(buf[k][l]);
  }
}

/* Data structure */
typedef struct {
    int mode;      /* 0=bilateral, 1=local_laplacian */
    float sigma_r; /* highlights (for LL) */
    float sigma_s; /* shadows (for LL) */
    float detail;  /* clarity */
    float midtone; /* sigma */
} BilatData;

/* Main process function */
void bilat_process(
    const float *const in,
    float *const out,
    const int width,
    const int height,
    const BilatData *const d)
{
    if(d->mode == 1) {
        /* Local laplacian mode */
        local_laplacian_internal(in, out, width, height,
                                  d->midtone, d->sigma_s, d->sigma_r, d->detail);
    } else {
        /* Bilateral mode - just copy for now */
        memcpy(out, in, (size_t)width * height * 4 * sizeof(float));
    }
}

/* ============================================================================
   Helper: return BilatData with defaults (local laplacian, subtle contrast)
   ============================================================================ */

static inline BilatData bilat_defaults(void)
{
    BilatData d;
    d.mode = 1;       /* local_laplacian mode */
    d.sigma_r = 0.5f; /* highlights compression */
    d.sigma_s = 0.5f; /* shadows lift */
    d.detail = 0.0f;  /* clarity (local contrast) */
    d.midtone = 0.2f; /* sigma for tone separation */
    return d;
}
