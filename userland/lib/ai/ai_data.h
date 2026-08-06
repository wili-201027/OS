// userland/lib/ai/ai_data.h
#ifndef AI_DATA_H
#define AI_DATA_H

#include <stdint.h>
#include "ai_core.h"

int ai_dataset_normalize(AiDataset *dataset);
int ai_dataset_shuffle(AiDataset *dataset, uint32_t seed);
int ai_dataset_split(AiDataset *dataset, float ratio, AiDataset **train_out, AiDataset **test_out);
int ai_dataset_summary(AiDataset *dataset, float *min_out, float *max_out, float *mean_out);

#endif // AI_DATA_H
