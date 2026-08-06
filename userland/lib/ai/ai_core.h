// userland/lib/ai/ai_core.h
#ifndef AI_CORE_H
#define AI_CORE_H

#include <stdint.h>
#include <stddef.h>

#define AI_MAX_LAYERS 8
#define AI_MAX_NEURONS 256
#define AI_MAX_PARAMETERS 4096

typedef float (*AiActivationFn)(float);

typedef enum {
    AI_MODEL_TYPE_UNKNOWN,
    AI_MODEL_TYPE_MLP,
    AI_MODEL_TYPE_CNN,
    AI_MODEL_TYPE_RNN,
} AiModelType;

typedef struct {
    AiModelType type;
    uint32_t layer_count;
    uint32_t neuron_count[AI_MAX_LAYERS];
    float learning_rate;
    uint32_t parameter_count;
    float *parameters;
    float *gradients;
    float *activations;
    float *cache;
} AiModel;

typedef struct {
    float *inputs;
    float *outputs;
    uint32_t input_size;
    uint32_t output_size;
    uint32_t sample_count;
} AiDataset;

typedef int (*AiParallelTaskFn)(uint32_t start, uint32_t end, void *ctx);

AiModel* ai_model_create(AiModelType type, uint32_t layer_count, const uint32_t *neurons, float learning_rate);
void ai_model_destroy(AiModel *model);

AiDataset* ai_dataset_create(uint32_t input_size, uint32_t output_size, uint32_t sample_count);
void ai_dataset_destroy(AiDataset *dataset);

int ai_model_randomize(AiModel *model, uint32_t seed);
int ai_model_forward(AiModel *model, const float *input, float *output);
int ai_model_train_batch(AiModel *model, AiDataset *dataset, uint32_t epochs);
int ai_model_update(AiModel *model, float *gradients);

float ai_activation_relu(float x);
float ai_activation_leaky_relu(float x);
float ai_activation_sigmoid(float x);
float ai_activation_tanh(float x);
float ai_activation_softmax(const float *input, float *output, uint32_t size);
float ai_activation_linear(float x);

int ai_dense_forward(const float *input, const float *weights, const float *bias,
                     float *output, uint32_t in_dim, uint32_t out_dim,
                     AiActivationFn activation);
int ai_conv2d_forward(const float *input, uint32_t in_h, uint32_t in_w, uint32_t in_c,
                      const float *kernel, uint32_t kernel_h, uint32_t kernel_w,
                      uint32_t out_c, uint32_t stride, uint32_t padding,
                      const float *bias, float *output);
int ai_rnn_step(const float *input, const float *prev_hidden,
                const float *Wx, const float *Wh, const float *b,
                float *next_hidden, uint32_t input_dim, uint32_t hidden_dim);
int ai_liquid_neuron_step(const float *input, float *state,
                           const float *W_input, const float *W_state,
                           const float *bias, uint32_t input_dim,
                           uint32_t state_dim, float decay);
int ai_liquid_neuron_step_chunk(const float *input, float *state,
                                const float *W_input, const float *W_state,
                                const float *bias, uint32_t input_dim,
                                uint32_t state_dim, float decay,
                                uint32_t start_state, uint32_t end_state);
int ai_ssm_step(const float *input, const float *A, const float *B,
                const float *C, const float *D, float *state, float *output,
                uint32_t state_dim, uint32_t input_dim, uint32_t output_dim);
int ai_ssm_state_update_chunk(const float *input, const float *A,
                              const float *B, const float *state,
                              float *next_state, uint32_t state_dim,
                              uint32_t input_dim, uint32_t start_state,
                              uint32_t end_state);
int ai_ssm_output_update_chunk(const float *input, const float *C,
                               const float *D, const float *state,
                               float *output, uint32_t state_dim,
                               uint32_t input_dim, uint32_t output_dim,
                               uint32_t start_output, uint32_t end_output);
int ai_parallel_for(uint32_t count, uint32_t chunk_size,
                    AiParallelTaskFn task, void *ctx);
int ai_parallel_vector_activation(const float *input, float *output,
                                  uint32_t count, AiActivationFn activation);
int ai_dense_forward_chunk(const float *input, const float *weights,
                           const float *bias, float *output,
                           uint32_t in_dim, uint32_t out_dim,
                           AiActivationFn activation,
                           uint32_t start_output, uint32_t end_output);
int ai_conv2d_forward_chunk(const float *input, uint32_t in_h,
                            uint32_t in_w, uint32_t in_c,
                            const float *kernel, uint32_t kernel_h,
                            uint32_t kernel_w, uint32_t out_c,
                            uint32_t stride, uint32_t padding,
                            const float *bias, float *output,
                            uint32_t start_unit, uint32_t end_unit);
int ai_rnn_step_chunk(const float *input, const float *prev_hidden,
                      const float *Wx, const float *Wh, const float *b,
                      float *next_hidden, uint32_t input_dim,
                      uint32_t hidden_dim, uint32_t start_hidden,
                      uint32_t end_hidden);
int ai_ssm_step_chunk(const float *input, const float *A, const float *B,
                      const float *C, const float *D, float *state,
                      float *next_state, float *output, uint32_t state_dim,
                      uint32_t input_dim, uint32_t output_dim,
                      uint32_t start_state, uint32_t end_state,
                      uint32_t start_output, uint32_t end_output);
float ai_loss_mse(const float *target, const float *prediction, uint32_t size);

#endif // AI_CORE_H
