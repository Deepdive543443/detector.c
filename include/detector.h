#ifndef DETECTOR_C_H
#define DETECTOR_C_H

#include <stdint.h>

extern const char   *class_names[];
extern const uint8_t color_list[80][3];

/**
 * @ https://stackoverflow.com/questions/4694401/how-to-replicate-vector-in-c
 *  -- Dynamic array of boxxes
 */
typedef struct {
    float x1;
    float y1;
    float x2;
    float y2;

    float prob;
    int   label;
} BoxInfo;

typedef struct {
    BoxInfo *data;
    size_t   capacity;
    size_t   num_item;
} BoxVec;

int     create_box_vector(BoxVec *box_vector, size_t capacity);
int     BoxVec_push_back(BoxInfo item, void *self_ptr);
int     BoxVec_insert(BoxInfo item, int index, void *self_ptr);
int     BoxVec_fit_size(void *self_ptr);
void    BoxVec_free(void *self_ptr);
BoxInfo BoxVec_getItem(int index, void *self_ptr);
BoxInfo BoxVec_pop(void *self_ptr);
BoxInfo BoxVec_remove(int index, void *self_ptr);

/**
 * Detector modules
 */
typedef struct {
    void      *net_ctx;
    int        input_size;
    float      mean_vals[3];
    float      norm_vals[3];
    BoxVec (*detect)(unsigned char *pixels, int pixel_w, int pixel_h, void *self_ptr);
} Detector;

/**
 * -- Pool allocator assignee
 */
Detector detector_init();
void     set_model_default_options(void *net_ctx);

/**
 * -- General function that share with all detector
 */
static inline float fast_exp(float x)
{
    union {
        __uint32_t i;
        float      f;
    } v;
    v.i = (1 << 23) * (1.4426950409 * x + 126.93490512f);
    return v.f;
}
#define FAST_SIGMOID(X) (1.0f / (1.0f + fast_exp(-X)))
#define FAST_TANH(X)    (2.f / (1.f + fast_exp(-2 * X)) - 1.f)

int   activation_function_softmax_inplace(float *src, int length);
void  qsort_descent_inplace(BoxVec *objects, int left, int right);
float intersection(BoxInfo *box1, BoxInfo *box2);
int   nms(BoxVec *objects, int *idx, float thresh);

void draw_boxxes(unsigned char *pixels, int pixel_w, int pixel_h, BoxVec *objects);
void destroy_detector(Detector *det);

/**
 * -- Nanodet's modules
 */
Detector create_nanodet_plus(const int input_size, const char *param, const char *bin);

/**
 * -- FastestDet's modules
 */
Detector create_fastestdet(const int input_size, const char *param, const char *bin);

#endif  // DETECTOR_C_H