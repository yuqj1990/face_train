#ifndef CAFFE_MULTIBOX_LOSS_LAYER_HPP_CENTERNET_OBJECT_
#define CAFFE_MULTIBOX_LOSS_LAYER_HPP_CENTERNET_OBJECT_

#include <map>
#include <utility>
#include <vector>

#include "caffe/blob.hpp"
#include "caffe/layer.hpp"
#include "caffe/proto/caffe.pb.h"
#include "caffe/util/bbox_util.hpp"
#include "caffe/util/center_bbox_util.hpp"
#include "caffe/layers/loss_layer.hpp"

namespace caffe {

/**
 * @brief Perform MultiBox operations. Including the following:
 *
 *  - decode the predictions.
 *  - perform matching between priors/predictions and ground truth.
 *  - use matched boxes and confidences to compute loss.
 *
 */
template <typename Dtype>
class CenterObjectLossLayer : public LossLayer<Dtype> {
public:
    explicit CenterObjectLossLayer(const LayerParameter& param)
        : LossLayer<Dtype>(param) {}
    virtual void LayerSetUp(const vector<Blob<Dtype>*>& bottom,
        const vector<Blob<Dtype>*>& top);
    virtual void Reshape(const vector<Blob<Dtype>*>& bottom,
        const vector<Blob<Dtype>*>& top);

    virtual inline const char* type() const { return "CenterObjectLoss"; }
    // bottom[0] stores the location predictions.
    // bottom[1] stores the confidence predictions.
    // bottom[2] stores the prior bounding boxes.
    // bottom[3] stores the ground truth bounding boxes.
    virtual inline int ExactNumBottomBlobs() const { return 3; }
    virtual inline int ExactNumTopBlobs() const { return 1; }

protected:
    virtual void Forward_cpu(const vector<Blob<Dtype>*>& bottom,
        const vector<Blob<Dtype>*>& top);
    virtual void Backward_cpu(const vector<Blob<Dtype>*>& top,
        const vector<bool>& propagate_down, const vector<Blob<Dtype>*>& bottom);
    
    // The internal localization offset loss layer.
    shared_ptr<Layer<Dtype> > loc_offset_loss_layer_;
    CenterObjectLossParameter_LocLossType loc_offset_loss_type_;
    float loc_offset_weight_;
    vector<Blob<Dtype>*> loc_offset_bottom_vec_;
    vector<Blob<Dtype>*> loc_offset_top_vec_;
    Blob<Dtype> loc_offset_pred_;
    Blob<Dtype> loc_offset_gt_;
    Blob<Dtype> loc_offset_loss_;

    shared_ptr<Layer<Dtype> > loc_wh_loss_layer_;
    CenterObjectLossParameter_LocLossType loc_wh_loss_type_;
    float loc_wh_weight_;
    vector<Blob<Dtype>*> loc_wh_bottom_vec_;
    vector<Blob<Dtype>*> loc_wh_top_vec_;
    Blob<Dtype> loc_wh_pred_;
    Blob<Dtype> loc_wh_gt_;
    Blob<Dtype> loc_wh_loss_;


    // The internal  landmarks scale loss layer.
    shared_ptr<Layer<Dtype> > lm_loss_layer_;
    CenterObjectLossParameter_LocLossType lm_loss_type_;
    float lm_weight_;
    // bottom vector holder used in Forward function.
    vector<Blob<Dtype>*> lm_bottom_vec_;
    // top vector holder used in Forward function.
    vector<Blob<Dtype>*> lm_top_vec_;
    // blob which stores the matched location prediction.
    Blob<Dtype> lm_pred_;
    // blob which stores the corresponding matched ground truth.
    Blob<Dtype> lm_gt_;
    // localization loss.
    Blob<Dtype> lm_loss_;

    // The internal confidence loss layer.
    shared_ptr<Layer<Dtype> > conf_loss_layer_;
    CenterObjectLossParameter_ConfLossType conf_loss_type_;
    // bottom vector holder used in Forward function.
    vector<Blob<Dtype>*> conf_bottom_vec_;
    // top vector holder used in Forward function.
    vector<Blob<Dtype>*> conf_top_vec_;
    // blob which stores the confidence prediction.
    Blob<Dtype> conf_pred_;
    // blob which stores the corresponding ground truth label.
    Blob<Dtype> conf_gt_;
    // confidence loss.
    Blob<Dtype> conf_loss_;

    int num_classes_;
    bool share_location_;

    CodeType code_type_;

    int loc_classes_;
    int num_gt_;
    int num_;

    std::map<int, vector<std::pair<NormalizedBBox, AnnoFaceLandmarks> > > all_gt_bboxes;

    int iterations_;

    // How to normalize the loss.
    LossParameter_NormalizationMode normalization_;
    bool has_lm_;
    int num_lm_;
};

}  // namespace caffe

#endif  // CAFFE_MULTIBOX_LOSS_LAYER_HPP_
