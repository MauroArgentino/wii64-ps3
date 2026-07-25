/**
 * spu_job.h - SPU Job Protocol for wii64-ps3
 * Defines job types, payloads, and communication protocol between PPU and SPU workers.
 */

#ifndef SPU_JOB_H
#define SPU_JOB_H

#include <stdint.h>
#include <ppu-types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Job Types */
typedef enum {
    SPU_JOB_NONE = 0,
    SPU_JOB_TL_VERTEX,      // Transform & Light (RSP vertex processing)
    SPU_JOB_TEX_DECODE,     // Texture decode: I4/IA4/CI4/I8/IA8/CI8 → RGBA32
    SPU_JOB_VTX_VALIDATE,   // Clamp NaN/Inf, fix vertex coordinates
    SPU_JOB_AUDIO_RESAMPLE, // Audio resample (SPU 0 dedicated)
} spu_job_type_t;

/* Job Status */
typedef enum {
    SPU_JOB_STATUS_PENDING = 0,
    SPU_JOB_STATUS_RUNNING,
    SPU_JOB_STATUS_DONE,
    SPU_JOB_STATUS_ERROR,
} spu_job_status_t;

/* Generic Job Header (always first in payload) */
typedef struct {
    uint32_t job_type;      // spu_job_type_t
    uint32_t job_id;        // Unique ID for tracking
    uint32_t payload_size;  // Size of job-specific payload following this header
    uint32_t result_addr;   // EA where to write result
    uint32_t flags;         // Job-specific flags
} spu_job_header_t;

/* SPU_JOB_TL_VERTEX Payload */
typedef struct {
    spu_job_header_t hdr;
    uint32_t vertex_count;      // Number of vertices
    uint32_t input_ea;          // EA of input vertices (GLVertex*)
    uint32_t output_ea;         // EA for transformed vertices
    float    mvp_matrix[16];    // Model-View-Projection matrix (column-major)
    float    viewport_scale[2]; // x_scale, y_scale
    float    viewport_trans[2]; // x_trans, y_trans
} spu_job_tl_vertex_t;

/* SPU_JOB_TEX_DECODE Payload */
typedef enum {
    TEX_FMT_I4   = 0,
    TEX_FMT_IA4  = 1,
    TEX_FMT_CI4  = 2,
    TEX_FMT_I8   = 3,
    TEX_FMT_IA8  = 4,
    TEX_FMT_CI8  = 5,
    TEX_FMT_RGBA16 = 6,
    TEX_FMT_RGBA32 = 7,
} spu_tex_format_t;

typedef struct {
    spu_job_header_t hdr;
    spu_tex_format_t format;      // Source texture format
    uint32_t width;               // Texture width
    uint32_t height;              // Texture height
    uint32_t input_ea;            // EA of source texture data
    uint32_t palette_ea;          // EA of palette (for CI formats), 0 if none
    uint32_t palette_format;      // G_TT_RGBA16=2, G_TT_IA16=3
    uint32_t output_ea;           // EA for RGBA32 output
    uint32_t unpack_alignment;    // glPixelStorei UNPACK_ALIGNMENT (1 or 4)
} spu_job_tex_decode_t;

/* SPU_JOB_VTX_VALIDATE Payload */
typedef struct {
    spu_job_header_t hdr;
    uint32_t vertex_count;      // Number of vertices
    uint32_t input_ea;          // EA of input vertices (GLVertex*)
    uint32_t output_ea;         // EA for validated vertices (can be same as input)
    float    clamp_min;         // Minimum coordinate value
    float    clamp_max;         // Maximum coordinate value
    uint32_t fix_nan_inf;       // 1 = replace NaN/Inf with clamp values
} spu_job_vtx_validate_t;

/* Result Structure (written back by SPU) */
typedef struct {
    uint32_t job_id;            // Must match request
    uint32_t status;            // spu_job_status_t
    uint32_t cycles_elapsed;    // SPU cycles for profiling
    uint32_t error_code;        // 0 = OK, non-zero = error
} spu_job_result_t;

/* Worker Pool Configuration */
typedef struct {
    uint32_t num_workers;       // Number of SPU workers (1-4)
    uint32_t stack_size;        // Stack size per worker
    uint32_t mailbox_size;      // Commands per worker mailbox
} spu_pool_config_t;

#ifdef __cplusplus
}
#endif

#endif /* SPU_JOB_H */