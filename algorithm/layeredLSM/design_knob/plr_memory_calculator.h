#ifndef __H_PLR_MEM_CALC__
#define __H_PLR_MEM_CALC__
#include <stdint.h>
void plr_memory_calc_init(uint32_t DEV_size);
uint64_t plr_memory_calc(uint32_t entry_num, uint32_t error, uint32_t DEV_size, bool wiskey);
double plr_memory_calc_avg(uint32_t entry_num, uint32_t target_bit,
		uint32_t error, uint32_t DEV_size, bool wiskey, double *line_per_chunk, double *normal_plr_dp, 
		uint64_t *line_cnt, uint64_t *chunk_cnt);
#endif
