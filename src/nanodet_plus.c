#include <float.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include "c_api.h"
#include "detector.h"

static void generate_proposals(ncnn_mat_t dis_pred, ncnn_mat_t cls_pred, int stride, float prob_thresh, BoxVec *objects)
{
    const int num_grid_x = ncnn_mat_get_w(cls_pred);
    const int num_grid_y = ncnn_mat_get_h(cls_pred);
    const int num_class  = ncnn_mat_get_c(cls_pred);
    const int cstep_cls  = ncnn_mat_get_cstep(cls_pred);

    const int reg_max_1 = ncnn_mat_get_w(dis_pred) / 4;
    const int hstep_dis = ncnn_mat_get_cstep(dis_pred);

    float *cls_data = (float *)ncnn_mat_get_data(cls_pred);
    float *dis_data = (float *)ncnn_mat_get_data(dis_pred);

    for (int i = 0; i < num_grid_y; i++) {
        for (int j = 0; j < num_grid_x; j++) {
            float *score_ptr = &cls_data[i * num_grid_x + j];
            float  max_score = -FLT_MAX;
            int    max_label = -1;

            for (int cls = 0; cls < num_class; cls++) {
                if (score_ptr[cls * cstep_cls] > max_score) {
                    max_score = score_ptr[cls * cstep_cls];
                    max_label = cls;
                }
            }

            if (max_score >= prob_thresh) {
                float  pred_ltrb[4];
                float *dis_ptr = &dis_data[(j * reg_max_1 * 4) + (i * hstep_dis)];
                for (int k = 0; k < 4; k++) {
                    float dis = 0.f;
                    activation_function_softmax_inplace(dis_ptr, 8);
                    for (int reg = 0; reg < reg_max_1; reg++) {
                        dis += reg * dis_ptr[reg];
                    }
                    pred_ltrb[k] = dis * stride;

                    dis_ptr += reg_max_1;
                }

                float x_center = j * stride;
                float y_center = i * stride;

                BoxInfo obj;
                obj.x1    = x_center - pred_ltrb[0];
                obj.y1    = y_center - pred_ltrb[1];
                obj.x2    = x_center + pred_ltrb[2];
                obj.y2    = y_center + pred_ltrb[3];
                obj.prob  = max_score;
                obj.label = max_label;

                BoxVec_push_back(obj, objects);
            }
        }
    }
}

static BoxVec nanodet_plus_detect(unsigned char *pixels, int pixel_w, int pixel_h, void *self_ptr)
{
    Detector   *self = (Detector *)self_ptr;
    ncnn_net_t *net  = (ncnn_net_t *)self->net_ctx;

    int   w, h;
    float scale;
    if (pixel_w > pixel_h) {
        scale = (float)self->input_size / pixel_w;
        w     = self->input_size;
        h     = pixel_h * scale;
    } else {
        scale = (float)self->input_size / pixel_h;
        h     = self->input_size;
        w     = pixel_w * scale;
    }

    int wpad = (w + 31) / 32 * 32 - w;
    int hpad = (h + 31) / 32 * 32 - h;

    ncnn_mat_t mat = ncnn_mat_from_pixels_resize(pixels, NCNN_MAT_PIXEL_BGR, pixel_w, pixel_h, pixel_w * 3, w, h, NULL);
    ncnn_mat_t mat_pad = ncnn_mat_create();
    ncnn_copy_make_border(mat, mat_pad, hpad / 2, hpad - hpad / 2, wpad / 2, wpad - wpad / 2, NCNN_BORDER_CONSTANT, 0.f,
                          NULL);
    ncnn_mat_destroy(mat);

    ncnn_mat_substract_mean_normalize(mat_pad, self->mean_vals, self->norm_vals);
    ncnn_extractor_t ex = ncnn_extractor_create(*net);
    ncnn_extractor_input(ex, "data", mat_pad);

    /**
     * Extract output from 4 scales
     */
    BoxVec proposals = {.data = NULL, .capacity = 0, .num_item = 0};
    if (create_box_vector(&proposals, 50)) {
        const char *outputs[] = {"dis8", "cls8", "dis16", "cls16", "dis32", "cls32", "dis64", "cls64"};
        const int   strides[] = {8, 16, 32, 64};
        for (int i = 0; i < 4; i++) {
            ncnn_mat_t out_mat_dis;
            ncnn_mat_t out_mat_cls;
            ncnn_extractor_extract(ex, outputs[i * 2], &out_mat_dis);
            ncnn_extractor_extract(ex, outputs[i * 2 + 1], &out_mat_cls);
            generate_proposals(out_mat_dis, out_mat_cls, strides[i], 0.4f, &proposals);  // prob thresh 0.4

            ncnn_mat_destroy(out_mat_dis);
            ncnn_mat_destroy(out_mat_cls);
        }
    }
    ncnn_mat_destroy(mat_pad);

    BoxVec objects = {.data = NULL, .capacity = 0, .num_item = 0};
    if (proposals.num_item > 2 && BoxVec_fit_size(&proposals)) {
        /**
         * Sort, non-max supression
         */
        qsort_descent_inplace(&proposals, 0, proposals.num_item - 1);
        int picked_box_idx[proposals.num_item];
        int num_picked = nms(&proposals, picked_box_idx, 0.5f);  // nms thresh 0.5

        /**
         * Scale back and shift
         */
        create_box_vector(&objects, num_picked);

        for (int i = 0; i < num_picked; i++) {
            BoxInfo box = BoxVec_getItem(picked_box_idx[i], &proposals);

            box.x1 = (box.x1 - (wpad / 2)) / scale;
            box.x2 = (box.x2 - (wpad / 2)) / scale;
            box.y1 = (box.y1 - (hpad / 2)) / scale;
            box.y2 = (box.y2 - (hpad / 2)) / scale;

            box.x1 = fmaxf(fminf(box.x1, (float)(pixel_w - 1)), 0.f);
            box.y1 = fmaxf(fminf(box.y1, (float)(pixel_h - 1)), 0.f);
            box.x2 = fmaxf(fminf(box.x2, (float)(pixel_w - 1)), 0.f);
            box.y2 = fmaxf(fminf(box.y2, (float)(pixel_h - 1)), 0.f);

            BoxVec_push_back(box, &objects);
        }
    }

    // Clean up
    BoxVec_free(&proposals);
    ncnn_extractor_destroy(ex);
    return objects;
}

Detector create_nanodet_plus(const int input_size, const char *param, const char *bin)
{
    ncnn_net_t *net = (ncnn_net_t *)malloc(sizeof(ncnn_net_t));

    *net = ncnn_net_create();
    set_model_default_options(net);
    ncnn_net_load_param(*net, param);
    ncnn_net_load_model(*net, bin);

    Detector nanodet_plus;
    nanodet_plus.net_ctx      = (void *)net;
    nanodet_plus.input_size   = input_size;
    nanodet_plus.mean_vals[0] = 103.53f;
    nanodet_plus.mean_vals[1] = 116.28f;
    nanodet_plus.mean_vals[2] = 123.675f;
    nanodet_plus.norm_vals[0] = 1.f / 57.375f;
    nanodet_plus.norm_vals[1] = 1.f / 57.12f;
    nanodet_plus.norm_vals[2] = 1.f / 58.395f;

    nanodet_plus.detect = &nanodet_plus_detect;
    return nanodet_plus;
}