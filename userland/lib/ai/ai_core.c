// userland/lib/ai/ai_core.c
#include "ai_core.h"
#include "../../libc/stdlib.h"
#include "../../libc/string.h"
#include "../../libc/math.h"

extern void sys_yield(void);

static float ai_random_float(uint32_t *seed)
{
    *seed = (*seed * 1103515245u + 12345u) & 0x7fffffff;
    return (float)(*seed) / 2147483647.0f;
}

AiModel* ai_model_create(AiModelType type, uint32_t layer_count, const uint32_t *neurons, float learning_rate)
{
    if(!neurons || layer_count == 0 || layer_count > AI_MAX_LAYERS) return NULL;

    AiModel *model = malloc(sizeof(AiModel));
    if(!model) return NULL;

    model->type = type;
    model->layer_count = layer_count;
    model->learning_rate = learning_rate;
    model->parameter_count = 0;
    model->parameters = NULL;
    model->gradients = NULL;
    model->activations = NULL;
    model->cache = NULL;

    uint32_t total_neurons = 0;
    for(uint32_t i = 0; i < layer_count && i < AI_MAX_LAYERS; i++) {
        model->neuron_count[i] = neurons[i];
        total_neurons += neurons[i];
    }

    if(total_neurons == 0) {
        free(model);
        return NULL;
    }

    model->parameter_count = total_neurons * 4;
    model->parameters = malloc(sizeof(float) * model->parameter_count);
    model->gradients = malloc(sizeof(float) * model->parameter_count);
    model->activations = malloc(sizeof(float) * total_neurons);
    model->cache = malloc(sizeof(float) * total_neurons);

    if(!model->parameters || !model->gradients || !model->activations || !model->cache) {
        ai_model_destroy(model);
        return NULL;
    }

    memset(model->parameters, 0, sizeof(float) * model->parameter_count);
    memset(model->gradients, 0, sizeof(float) * model->parameter_count);
    memset(model->activations, 0, sizeof(float) * total_neurons);
    memset(model->cache, 0, sizeof(float) * total_neurons);

    return model;
}

void ai_model_destroy(AiModel *model)
{
    if(!model) return;
    if(model->parameters) free(model->parameters);
    if(model->gradients) free(model->gradients);
    if(model->activations) free(model->activations);
    if(model->cache) free(model->cache);
    free(model);
}

AiDataset* ai_dataset_create(uint32_t input_size, uint32_t output_size, uint32_t sample_count)
{
    if(input_size == 0 || output_size == 0 || sample_count == 0) return NULL;

    AiDataset *dataset = malloc(sizeof(AiDataset));
    if(!dataset) return NULL;

    dataset->input_size = input_size;
    dataset->output_size = output_size;
    dataset->sample_count = sample_count;
    dataset->inputs = malloc(sizeof(float) * input_size * sample_count);
    dataset->outputs = malloc(sizeof(float) * output_size * sample_count);

    if(!dataset->inputs || !dataset->outputs) {
        ai_dataset_destroy(dataset);
        return NULL;
    }

    memset(dataset->inputs, 0, sizeof(float) * input_size * sample_count);
    memset(dataset->outputs, 0, sizeof(float) * output_size * sample_count);
    return dataset;
}

void ai_dataset_destroy(AiDataset *dataset)
{
    if(!dataset) return;
    if(dataset->inputs) free(dataset->inputs);
    if(dataset->outputs) free(dataset->outputs);
    free(dataset);
}

int ai_model_randomize(AiModel *model, uint32_t seed)
{
    if(!model) return -1;
    for(uint32_t i = 0; i < model->parameter_count; i++) {
        model->parameters[i] = ai_random_float(&seed) * 2.0f - 1.0f;
    }
    return 0;
}

float ai_activation_relu(float x) {
    return x > 0.0f ? x : 0.0f;
}

float ai_activation_leaky_relu(float x) {
    return x > 0.0f ? x : 0.01f * x;
}

float ai_activation_sigmoid(float x) {
    return 1.0f / (1.0f + expf(-x));
}

float ai_activation_tanh(float x) {
    float e_pos = expf(x);
    float e_neg = expf(-x);
    return (e_pos - e_neg) / (e_pos + e_neg);
}

float ai_activation_softmax(const float *input, float *output, uint32_t size)
{
    if(!input || !output || size == 0) return 0.0f;

    float max_val = input[0];
    for(uint32_t i = 1; i < size; i++) {
        if(input[i] > max_val) max_val = input[i];
    }

    float sum = 0.0f;
    for(uint32_t i = 0; i < size; i++) {
        output[i] = expf(input[i] - max_val);
        sum += output[i];
    }

    if(sum == 0.0f) sum = 1.0f;
    for(uint32_t i = 0; i < size; i++) {
        output[i] /= sum;
    }

    return sum;
}

float ai_activation_linear(float x) {
    return x;
}

static int ai_chunk_bounds(uint32_t total, uint32_t chunk_size, uint32_t index,
                             uint32_t *start, uint32_t *end)
{
    if(!start || !end || chunk_size == 0) return -1;
    uint32_t chunk_count = (total + chunk_size - 1) / chunk_size;
    if(index >= chunk_count) return -1;
    *start = index * chunk_size;
    *end = *start + chunk_size;
    if(*end > total) *end = total;
    return 0;
}

int ai_parallel_for(uint32_t count, uint32_t chunk_size,
                    AiParallelTaskFn task, void *ctx)
{
    if(count == 0 || chunk_size == 0 || !task) return -1;
    uint32_t chunk_count = (count + chunk_size - 1) / chunk_size;

    for(uint32_t chunk = 0; chunk < chunk_count; chunk++) {
        uint32_t start, end;
        if(ai_chunk_bounds(count, chunk_size, chunk, &start, &end) != 0) return -1;
        int rc = task(start, end, ctx);
        if(rc != 0) return rc;
        sys_yield();
    }

    return 0;
}

int ai_dense_forward(const float *input, const float *weights, const float *bias,
                     float *output, uint32_t in_dim, uint32_t out_dim,
                     AiActivationFn activation)
{
    return ai_dense_forward_chunk(input, weights, bias, output, in_dim, out_dim,
                                  activation, 0, out_dim);
}

int ai_dense_forward_chunk(const float *input, const float *weights,
                           const float *bias, float *output,
                           uint32_t in_dim, uint32_t out_dim,
                           AiActivationFn activation,
                           uint32_t start_output, uint32_t end_output)
{
    if(!input || !weights || !output || in_dim == 0 || out_dim == 0) return -1;
    if(start_output >= end_output || end_output > out_dim) return -1;

    for(uint32_t o = start_output; o < end_output; o++) {
        float sum = 0.0f;
        for(uint32_t i = 0; i < in_dim; i++) {
            sum += weights[o * in_dim + i] * input[i];
        }
        if(bias) sum += bias[o];
        output[o] = activation ? activation(sum) : sum;
    }

    return 0;
}

int ai_conv2d_forward(const float *input, uint32_t in_h, uint32_t in_w, uint32_t in_c,
                      const float *kernel, uint32_t kernel_h, uint32_t kernel_w,
                      uint32_t out_c, uint32_t stride, uint32_t padding,
                      const float *bias, float *output)
{
    uint32_t total_units = 0;
    if(!input || !kernel || !output || in_h == 0 || in_w == 0 || in_c == 0 ||
       kernel_h == 0 || kernel_w == 0 || out_c == 0 || stride == 0) return -1;

    uint32_t out_h = (in_h + 2 * padding - kernel_h) / stride + 1;
    uint32_t out_w = (in_w + 2 * padding - kernel_w) / stride + 1;
    total_units = out_c * out_h * out_w;
    return ai_conv2d_forward_chunk(input, in_h, in_w, in_c, kernel, kernel_h,
                                   kernel_w, out_c, stride, padding,
                                   bias, output, 0, total_units);
}

int ai_conv2d_forward_chunk(const float *input, uint32_t in_h, uint32_t in_w,
                            uint32_t in_c, const float *kernel, uint32_t kernel_h,
                            uint32_t kernel_w, uint32_t out_c, uint32_t stride,
                            uint32_t padding, const float *bias, float *output,
                            uint32_t start_unit, uint32_t end_unit)
{
    if(!input || !kernel || !output || in_h == 0 || in_w == 0 || in_c == 0 ||
       kernel_h == 0 || kernel_w == 0 || out_c == 0 || stride == 0) return -1;
    if(start_unit >= end_unit) return -1;

    uint32_t out_h = (in_h + 2 * padding - kernel_h) / stride + 1;
    uint32_t out_w = (in_w + 2 * padding - kernel_w) / stride + 1;
    uint32_t total_units = out_c * out_h * out_w;
    if(end_unit > total_units) return -1;

    for(uint32_t unit = start_unit; unit < end_unit; unit++) {
        uint32_t oc = unit / (out_h * out_w);
        uint32_t remainder = unit % (out_h * out_w);
        uint32_t oy = remainder / out_w;
        uint32_t ox = remainder % out_w;

        float sum = 0.0f;
        for(uint32_t ic = 0; ic < in_c; ic++) {
            for(uint32_t ky = 0; ky < kernel_h; ky++) {
                for(uint32_t kx = 0; kx < kernel_w; kx++) {
                    int32_t iy = (int32_t)oy * (int32_t)stride + (int32_t)ky - (int32_t)padding;
                    int32_t ix = (int32_t)ox * (int32_t)stride + (int32_t)kx - (int32_t)padding;
                    if(iy < 0 || ix < 0 || iy >= (int32_t)in_h || ix >= (int32_t)in_w) continue;
                    uint32_t input_index = (iy * in_w + ix) * in_c + ic;
                    uint32_t kernel_index = ((oc * in_c + ic) * kernel_h + ky) * kernel_w + kx;
                    sum += input[input_index] * kernel[kernel_index];
                }
            }
        }
        if(bias) sum += bias[oc];
        output[unit] = ai_activation_relu(sum);
    }

    return 0;
}

int ai_rnn_step(const float *input, const float *prev_hidden,
                const float *Wx, const float *Wh, const float *b,
                float *next_hidden, uint32_t input_dim, uint32_t hidden_dim)
{
    return ai_rnn_step_chunk(input, prev_hidden, Wx, Wh, b,
                             next_hidden, input_dim, hidden_dim,
                             0, hidden_dim);
}

int ai_rnn_step_chunk(const float *input, const float *prev_hidden,
                      const float *Wx, const float *Wh, const float *b,
                      float *next_hidden, uint32_t input_dim,
                      uint32_t hidden_dim, uint32_t start_hidden,
                      uint32_t end_hidden)
{
    if(!input || !prev_hidden || !Wx || !Wh || !next_hidden || hidden_dim == 0) return -1;
    if(start_hidden >= end_hidden || end_hidden > hidden_dim) return -1;

    for(uint32_t h = start_hidden; h < end_hidden; h++) {
        float sum = 0.0f;
        for(uint32_t i = 0; i < input_dim; i++) {
            sum += Wx[h * input_dim + i] * input[i];
        }
        for(uint32_t j = 0; j < hidden_dim; j++) {
            sum += Wh[h * hidden_dim + j] * prev_hidden[j];
        }
        if(b) sum += b[h];
        next_hidden[h] = ai_activation_tanh(sum);
    }

    return 0;
}

int ai_liquid_neuron_step(const float *input, float *state,
                           const float *W_input, const float *W_state,
                           const float *bias, uint32_t input_dim,
                           uint32_t state_dim, float decay)
{
    return ai_liquid_neuron_step_chunk(input, state, W_input, W_state,
                                       bias, input_dim, state_dim, decay,
                                       0, state_dim);
}

int ai_liquid_neuron_step_chunk(const float *input, float *state,
                                const float *W_input, const float *W_state,
                                const float *bias, uint32_t input_dim,
                                uint32_t state_dim, float decay,
                                uint32_t start_state, uint32_t end_state)
{
    if(!input || !state || !W_input || !W_state || state_dim == 0) return -1;
    if(start_state >= end_state || end_state > state_dim) return -1;

    for(uint32_t s = start_state; s < end_state; s++) {
        float sum = decay * state[s];
        for(uint32_t i = 0; i < input_dim; i++) {
            sum += W_input[s * input_dim + i] * input[i];
        }
        for(uint32_t j = 0; j < state_dim; j++) {
            sum += W_state[s * state_dim + j] * state[j];
        }
        if(bias) sum += bias[s];
        state[s] = ai_activation_leaky_relu(sum);
    }

    return 0;
}

int ai_ssm_step(const float *input, const float *A, const float *B,
                const float *C, const float *D, float *state, float *output,
                uint32_t state_dim, uint32_t input_dim, uint32_t output_dim)
{
    if(!input || !A || !B || !C || !state || !output) return -1;

    float next_state[AI_MAX_NEURONS];
    if(state_dim > AI_MAX_NEURONS) return -1;

    if (ai_ssm_state_update_chunk(input, A, B, state, next_state,
                                  state_dim, input_dim, 0, state_dim) != 0) {
        return -1;
    }

    for(uint32_t i = 0; i < state_dim; i++) {
        state[i] = next_state[i];
    }

    if (ai_ssm_output_update_chunk(input, C, D, state, output,
                                   state_dim, input_dim, output_dim,
                                   0, output_dim) != 0) {
        return -1;
    }

    return 0;
}

int ai_ssm_state_update_chunk(const float *input, const float *A,
                              const float *B, const float *state,
                              float *next_state, uint32_t state_dim,
                              uint32_t input_dim, uint32_t start_state,
                              uint32_t end_state)
{
    if(!input || !A || !B || !state || !next_state) return -1;
    if(start_state >= end_state || end_state > state_dim) return -1;

    for(uint32_t i = start_state; i < end_state; i++) {
        float sum = 0.0f;
        for(uint32_t j = 0; j < state_dim; j++) {
            sum += A[i * state_dim + j] * state[j];
        }
        for(uint32_t j = 0; j < input_dim; j++) {
            sum += B[i * input_dim + j] * input[j];
        }
        next_state[i] = sum;
    }

    return 0;
}

int ai_ssm_output_update_chunk(const float *input, const float *C,
                               const float *D, const float *state,
                               float *output, uint32_t state_dim,
                               uint32_t input_dim, uint32_t output_dim,
                               uint32_t start_output, uint32_t end_output)
{
    if(!input || !C || !D || !state || !output) return -1;
    if(start_output >= end_output || end_output > output_dim) return -1;

    for(uint32_t i = start_output; i < end_output; i++) {
        float sum = 0.0f;
        for(uint32_t j = 0; j < state_dim; j++) {
            sum += C[i * state_dim + j] * state[j];
        }
        for(uint32_t j = 0; j < input_dim; j++) {
            sum += D[i * input_dim + j] * input[j];
        }
        output[i] = sum;
    }

    return 0;
}

int ai_parallel_vector_activation(const float *input, float *output,
                                  uint32_t count, AiActivationFn activation)
{
    if(!input || !output || count == 0) return -1;
    if(!activation) return -1;

    for(uint32_t i = 0; i < count; i++) {
        output[i] = activation(input[i]);
        if ((i & 0x7F) == 0) sys_yield();
    }

    return 0;
}

float ai_loss_mse(const float *target, const float *prediction, uint32_t size)
{
    if(!target || !prediction || size == 0) return 0.0f;
    float sum = 0.0f;
    for(uint32_t i = 0; i < size; i++) {
        float diff = target[i] - prediction[i];
        sum += diff * diff;
    }
    return sum / (float)size;
}

int ai_model_forward(AiModel *model, const float *input, float *output)
{
    if(!model || !input || !output) return -1;

    uint32_t input_index = 0;
    uint32_t activation_index = 0;
    uint32_t param_index = 0;

    for(uint32_t layer = 0; layer < model->layer_count; layer++) {
        uint32_t neuron_count = model->neuron_count[layer];
        for(uint32_t neuron = 0; neuron < neuron_count; neuron++) {
            float sum = 0.0f;
            uint32_t prev_neurons = (layer == 0) ? model->neuron_count[0] : model->neuron_count[layer - 1];
            for(uint32_t j = 0; j < prev_neurons; j++) {
                float weight = model->parameters[param_index++];
                sum += weight * input[j];
            }

            float bias = model->parameters[param_index++];
            sum += bias;
            float activated = ai_activation_relu(sum);
            model->activations[activation_index++] = activated;
        }

        if(layer == 0) {
            input = model->activations;
        }
    }

    for(uint32_t i = 0; i < model->neuron_count[model->layer_count - 1]; i++) {
        output[i] = model->activations[(model->layer_count - 1) * model->neuron_count[0] + i];
    }

    return 0;
}

int ai_model_train_batch(AiModel *model, AiDataset *dataset, uint32_t epochs)
{
    if(!model || !dataset || epochs == 0) return -1;

    for(uint32_t epoch = 0; epoch < epochs; epoch++) {
        float loss = 0.0f;
        for(uint32_t sample = 0; sample < dataset->sample_count; sample++) {
            float *input = &dataset->inputs[sample * dataset->input_size];
            float *target = &dataset->outputs[sample * dataset->output_size];
            float prediction[AI_MAX_NEURONS] = {0};

            ai_model_forward(model, input, prediction);
            loss += ai_loss_mse(target, prediction, dataset->output_size);

            for(uint32_t i = 0; i < model->parameter_count; i++) {
                model->gradients[i] = (ai_random_float(&epoch) - 0.5f) * 0.01f;
            }

            for(uint32_t i = 0; i < model->parameter_count; i++) {
                model->parameters[i] -= model->learning_rate * model->gradients[i];
            }
        }

        if(loss < 0.001f) break;
    }

    return 0;
}

int ai_model_update(AiModel *model, float *gradients)
{
    if(!model || !gradients) return -1;
    for(uint32_t i = 0; i < model->parameter_count; i++) {
        model->parameters[i] -= model->learning_rate * gradients[i];
    }
    return 0;
}
