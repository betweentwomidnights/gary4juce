/* Vendored from stems.cpp v0.1.2 (https://github.com/betweentwomidnights/stems.cpp, src/libstems_v1.h).
 * MIT License, Copyright (c) 2026 ath (tinycrops). Keep it identical to the release gary4juce pins.
 */
/* libstems V1 — stable C ABI for embedding stems.cpp (stem separation on ggml).
 *
 * Same contract as sa3.cpp's libsa3 V1, so a host that already embeds one can embed the other:
 *
 *   stems_get_api(STEMS_ABI_VERSION_1) -> context_create -> request_init -> separate
 *                                      -> use result.samples -> result_free -> context_destroy
 *
 * Every public struct is size tagged: zero it, set `size = sizeof(struct)`, then call its
 * initializer from the table, which writes the defaults. Structs may grow by appending fields
 * in a later revision of V1; the library validates against the frozen *_MIN_SIZE values
 * below, never against a future sizeof. Types embedded by value are frozen for V1.
 *
 * Audio in: any sample rate, any channel count, planar or interleaved float. The library
 * converts to the model's format (44.1 kHz stereo) and converts every stem back to the
 * input's sample rate, so result.n_samples == input.n_samples. Stems are always
 * audio_channels() channels (stereo for every published model).
 *
 * Threading: a context is not reentrant. Use one context per thread, or serialise calls.
 * separate() blocks; progress and cancel callbacks run on the calling thread, between
 * segments (one every ~7.8 s of audio per pass).
 */
#ifndef LIBSTEMS_V1_H
#define LIBSTEMS_V1_H

#include <stddef.h>
#include <stdint.h>

#ifndef STEMS_API
#  if defined(_WIN32) && defined(STEMS_BUILD_DLL)
#    define STEMS_API __declspec(dllexport)
#  elif defined(__GNUC__) && defined(STEMS_BUILD_DLL)
#    define STEMS_API __attribute__((visibility("default")))
#  else
#    define STEMS_API
#  endif
#endif

#if defined(_WIN32)
#  define STEMS_CALL __cdecl
#else
#  define STEMS_CALL
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct stems_context stems_context;

#define STEMS_ABI_VERSION_1 1u
#define STEMS_MAX_SOURCES_V1 8

typedef int32_t stems_status_v1;
enum {
    STEMS_STATUS_OK_V1               = 0,
    STEMS_STATUS_INVALID_ARGUMENT_V1 = 1,
    STEMS_STATUS_UNSUPPORTED_ABI_V1  = 2,
    STEMS_STATUS_CANCELLED_V1        = 3,
    STEMS_STATUS_MODEL_ERROR_V1      = 4,
    STEMS_STATUS_IO_ERROR_V1         = 5,
    STEMS_STATUS_OUT_OF_MEMORY_V1    = 6,
    STEMS_STATUS_INTERNAL_ERROR_V1   = 7
};

typedef int32_t stems_audio_layout_v1;
enum {
    STEMS_AUDIO_PLANAR_V1      = 0,   /* samples[channel * n_samples + sample] */
    STEMS_AUDIO_INTERLEAVED_V1 = 1    /* samples[sample * n_channels + channel] */
};

typedef struct {
    uint32_t size;
    stems_status_v1 code;
    char message[1024];
} stems_error_v1;

typedef struct {
    uint32_t size;
    const char* model_path;     /* a stems.cpp GGUF, e.g. models/htdemucs-42M-v1.0-F32.gguf */
    const char* device;         /* NULL or "" = best GPU, falling back to CPU; "cpu" = CPU */
    int32_t cpu_threads;        /* 0 = one per physical core (performance cores on Apple Silicon) */
} stems_context_config_v1;

/* Non-owning input view. n_samples is per channel. */
typedef struct {
    uint32_t size;
    const float* samples;
    uint64_t n_samples;
    uint32_t n_channels;
    uint32_t sample_rate;
    stems_audio_layout_v1 layout;
    uint32_t reserved;
} stems_audio_view_v1;

typedef struct {
    uint32_t size;
    int32_t pass;               /* segment passes completed */
    int32_t total;              /* segment passes in this job */
    float fraction;             /* pass / total */
} stems_progress_v1;

typedef void (STEMS_CALL *stems_progress_callback_v1)(void* user, const stems_progress_v1* progress);
/* Return non-zero to cancel; separate() then returns STEMS_STATUS_CANCELLED_V1. */
typedef int32_t (STEMS_CALL *stems_cancel_callback_v1)(void* user);
typedef void (STEMS_CALL *stems_reserved_function_v1)(void);

typedef struct {
    uint32_t size;
    stems_audio_view_v1 input;  /* frozen by-value type */
    int32_t shifts;             /* random time shifts averaged; 0 = one deterministic pass */
    float overlap;              /* segment overlap, [0, 1); negative (the default) = the model's
                                   own: 0.25 for HTDemucs, 0.5 for the published RoFormers */
    uint32_t seed;              /* for the shift offsets */
    stems_progress_callback_v1 on_progress;
    stems_cancel_callback_v1 should_cancel;
    void* callback_user;
} stems_request_v1;

/* Library-owned planar audio: samples[(source * n_channels + channel) * n_samples + sample].
 * Stem s is named model_info.source_names[s]. result_free is safe on a zeroed or freed result. */
typedef struct {
    uint32_t size;
    float* samples;
    uint32_t n_sources;
    uint32_t n_channels;
    uint64_t n_samples;
    uint32_t sample_rate;
    uint32_t reserved;
} stems_result_v1;

/* Borrowed from the context: strings stay valid until context_destroy. */
typedef struct {
    uint32_t size;
    const char* name;           /* "htdemucs", "htdemucs_6s", "htdemucs_ft" */
    const char* backend;        /* ggml backend name, e.g. "CUDA0", "CPU" */
    uint32_t n_sources;
    const char* source_names[STEMS_MAX_SOURCES_V1];
    uint32_t sample_rate;       /* the model's own rate (44100) */
    uint32_t audio_channels;    /* 2 */
    uint32_t n_models;          /* > 1 for a bag (htdemucs_ft) */
    uint32_t segment_samples;
    /* Appended in V1.1; read it only when size >= STEMS_MODEL_INFO_V1_1_SIZE. */
    const char* architecture;   /* GGUF general.architecture, e.g. "htdemucs" */
} stems_model_info_v1;

typedef struct stems_api_v1 {
    uint32_t size;
    uint32_t abi_version;
    const char* (STEMS_CALL *runtime_version)(void);

    void (STEMS_CALL *error_init)(stems_error_v1* error);
    void (STEMS_CALL *context_config_init)(stems_context_config_v1* config);
    void (STEMS_CALL *audio_view_init)(stems_audio_view_v1* audio);
    void (STEMS_CALL *request_init)(stems_request_v1* request);
    void (STEMS_CALL *result_init)(stems_result_v1* result);
    void (STEMS_CALL *model_info_init)(stems_model_info_v1* info);

    stems_status_v1 (STEMS_CALL *context_create)(const stems_context_config_v1* config,
                                                 stems_context** out_context,
                                                 stems_error_v1* error);
    void (STEMS_CALL *context_destroy)(stems_context* context);
    stems_status_v1 (STEMS_CALL *model_info)(stems_context* context,
                                             stems_model_info_v1* info,
                                             stems_error_v1* error);
    stems_status_v1 (STEMS_CALL *separate)(stems_context* context,
                                           const stems_request_v1* request,
                                           stems_result_v1* result,
                                           stems_error_v1* error);
    void (STEMS_CALL *result_free)(stems_result_v1* result);

    stems_reserved_function_v1 reserved[16];
} stems_api_v1;

/* Frozen V1 prefixes: each names the last field of the initial V1 contract. */
#define STEMS_FIELD_END_(type, field) \
    ((uint32_t)(offsetof(type, field) + sizeof(((type*)0)->field)))
#define STEMS_ERROR_V1_MIN_SIZE          STEMS_FIELD_END_(stems_error_v1, message)
#define STEMS_CONTEXT_CONFIG_V1_MIN_SIZE STEMS_FIELD_END_(stems_context_config_v1, cpu_threads)
#define STEMS_AUDIO_VIEW_V1_MIN_SIZE     STEMS_FIELD_END_(stems_audio_view_v1, reserved)
#define STEMS_PROGRESS_V1_MIN_SIZE       STEMS_FIELD_END_(stems_progress_v1, fraction)
#define STEMS_REQUEST_V1_MIN_SIZE        STEMS_FIELD_END_(stems_request_v1, callback_user)
#define STEMS_RESULT_V1_MIN_SIZE         STEMS_FIELD_END_(stems_result_v1, reserved)
#define STEMS_MODEL_INFO_V1_MIN_SIZE     STEMS_FIELD_END_(stems_model_info_v1, segment_samples)
#define STEMS_MODEL_INFO_V1_1_SIZE       STEMS_FIELD_END_(stems_model_info_v1, architecture)
#define STEMS_API_V1_MIN_SIZE            STEMS_FIELD_END_(stems_api_v1, result_free)

/* The only symbol a dynamically loaded consumer needs to resolve. NULL for an unsupported ABI
 * major. The table is static and owned by the library for the lifetime of the module. */
STEMS_API const stems_api_v1* STEMS_CALL stems_get_api(uint32_t abi_version);

#ifdef __cplusplus
}
#endif

#endif /* LIBSTEMS_V1_H */
