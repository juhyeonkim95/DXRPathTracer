#ifndef PATH_TRACE_CONSTANTS
#define PATH_TRACE_CONSTANTS

#define USE_NEXT_EVENT_ESTIMATION 1
#define USE_RUSSIAN_ROULETTE 0
#define PATHTRACE_RR_BEGIN_DEPTH 4
#define PATHTRACE_SPP 1
#define DO_FILTERING 1

static const int ReSTIR_MODE_NO_REUSE = 0;
static const int ReSTIR_MODE_TEMPORAL_REUSE = 1;
static const int ReSTIR_MODE_SPATIAL_REUSE = 2;
static const int ReSTIR_MODE_SPATIOTEMPORAL_REUSE = 3;

static const float SCENE_T_MIN = 1e-5;
static const float SCENE_T_MAX = 1e5;

#endif