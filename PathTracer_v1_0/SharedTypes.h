#ifdef __cplusplus
#define SEMANTIC(sem)
#else
#define SEMANTIC(sem) : sem
#endif

struct Material
{
	float3 diffuse SEMANTIC(DIFFUSE0);
};