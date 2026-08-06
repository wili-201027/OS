// userland/lib/ai/ai_data.c
#include "ai_data.h"
#include "ai_core.h"
#include "ai_utils.h"
#include "../../libc/stdlib.h"
#include "../../libc/string.h"

int ai_dataset_normalize(AiDataset *dataset)
{
    if(!dataset || !dataset->inputs || dataset->sample_count == 0) return -1;

    uint32_t total = dataset->input_size * dataset->sample_count;
    float min_val = dataset->inputs[0];
    float max_val = dataset->inputs[0];

    for(uint32_t i = 0; i < total; i++) {
        if(dataset->inputs[i] < min_val) min_val = dataset->inputs[i];
        if(dataset->inputs[i] > max_val) max_val = dataset->inputs[i];
    }

    float range = max_val - min_val;
    if(range == 0.0f) range = 1.0f;

    for(uint32_t i = 0; i < total; i++) {
        dataset->inputs[i] = (dataset->inputs[i] - min_val) / range;
    }

    return 0;
}

int ai_dataset_shuffle(AiDataset *dataset, uint32_t seed)
{
    if(!dataset || !dataset->inputs || !dataset->outputs || dataset->sample_count == 0) return -1;

    ai_seed_rng(seed);
    for(uint32_t i = dataset->sample_count - 1; i > 0; i--) {
        uint32_t j = (uint32_t)(ai_rand_float() * (i + 1));
        if(j >= dataset->sample_count) j = i;

        for(uint32_t k = 0; k < dataset->input_size; k++) {
            float tmp = dataset->inputs[i * dataset->input_size + k];
            dataset->inputs[i * dataset->input_size + k] = dataset->inputs[j * dataset->input_size + k];
            dataset->inputs[j * dataset->input_size + k] = tmp;
        }

        for(uint32_t k = 0; k < dataset->output_size; k++) {
            float tmp = dataset->outputs[i * dataset->output_size + k];
            dataset->outputs[i * dataset->output_size + k] = dataset->outputs[j * dataset->output_size + k];
            dataset->outputs[j * dataset->output_size + k] = tmp;
        }
    }

    return 0;
}

int ai_dataset_split(AiDataset *dataset, float ratio, AiDataset **train_out, AiDataset **test_out)
{
    if(!dataset || !train_out || !test_out || ratio <= 0.0f || ratio >= 1.0f) return -1;

    uint32_t train_count = (uint32_t)(dataset->sample_count * ratio);
    uint32_t test_count = dataset->sample_count - train_count;

    *train_out = ai_dataset_create(dataset->input_size, dataset->output_size, train_count);
    *test_out = ai_dataset_create(dataset->input_size, dataset->output_size, test_count);
    if(!*train_out || !*test_out) {
        if(*train_out) ai_dataset_destroy(*train_out);
        if(*test_out) ai_dataset_destroy(*test_out);
        return -1;
    }

    for(uint32_t i = 0; i < train_count; i++) {
        memcpy(&(*train_out)->inputs[i * dataset->input_size],
               &dataset->inputs[i * dataset->input_size],
               sizeof(float) * dataset->input_size);
        memcpy(&(*train_out)->outputs[i * dataset->output_size],
               &dataset->outputs[i * dataset->output_size],
               sizeof(float) * dataset->output_size);
    }

    for(uint32_t i = 0; i < test_count; i++) {
        memcpy(&(*test_out)->inputs[i * dataset->input_size],
               &dataset->inputs[(train_count + i) * dataset->input_size],
               sizeof(float) * dataset->input_size);
        memcpy(&(*test_out)->outputs[i * dataset->output_size],
               &dataset->outputs[(train_count + i) * dataset->output_size],
               sizeof(float) * dataset->output_size);
    }

    return 0;
}

int ai_dataset_summary(AiDataset *dataset, float *min_out, float *max_out, float *mean_out)
{
    if(!dataset || !dataset->inputs || dataset->sample_count == 0) return -1;

    uint32_t total = dataset->input_size * dataset->sample_count;
    float min_val = dataset->inputs[0];
    float max_val = dataset->inputs[0];
    float sum = 0.0f;

    for(uint32_t i = 0; i < total; i++) {
        if(dataset->inputs[i] < min_val) min_val = dataset->inputs[i];
        if(dataset->inputs[i] > max_val) max_val = dataset->inputs[i];
        sum += dataset->inputs[i];
    }

    if(min_out) *min_out = min_val;
    if(max_out) *max_out = max_val;
    if(mean_out) *mean_out = sum / (float)total;

    return 0;
}
