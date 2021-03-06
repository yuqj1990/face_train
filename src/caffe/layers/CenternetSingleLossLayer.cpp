#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "caffe/layers/CenternetSingleLossLayer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void CenterObjectSingleLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
    LossLayer<Dtype>::LayerSetUp(bottom, top);
    const CenterObjectLossParameter& center_object_loss_param =
        this->layer_param_.center_object_loss_param();
    has_lm_ = center_object_loss_param.has_lm();
    if (this->layer_param_.propagate_down_size() == 0) {
        this->layer_param_.add_propagate_down(true);
        this->layer_param_.add_propagate_down(true);
        this->layer_param_.add_propagate_down(true);
        if(has_lm_){
            this->layer_param_.add_propagate_down(true);
            this->layer_param_.add_propagate_down(false);
        }else{
            this->layer_param_.add_propagate_down(false);
        }
        
    }
    CHECK_EQ(bottom[0]->channels(), bottom[1]->channels());
    CHECK_EQ(bottom[0]->channels(), 2);
    if(has_lm_){
        CHECK_EQ(bottom[3]->channels(), 10);
    }
    
    num_classes_ = center_object_loss_param.num_class();
    CHECK_GE(num_classes_, 1) << "num_classes should not be less than 1.";
    CHECK_EQ(num_classes_, bottom[2]->channels()) << "num_classes must be equal to prediction classes";

    num_ = bottom[0]->num();
    num_gt_ = bottom[3]->height();
    if(has_lm_){
        num_gt_ = bottom[4]->height();
    }

    share_location_ = center_object_loss_param.share_location();
    loc_classes_ = share_location_ ? 1 : num_classes_;

    if (!this->layer_param_.loss_param().has_normalization() &&
        this->layer_param_.loss_param().has_normalize()) {
        normalization_ = this->layer_param_.loss_param().normalize() ?
                        LossParameter_NormalizationMode_VALID :
                        LossParameter_NormalizationMode_BATCH_SIZE;
    } else {
        normalization_ = this->layer_param_.loss_param().normalization();
    }

    vector<int> loss_shape(1, 1);
    // Set up loc offset & wh scale loss layer.
    loc_offset_weight_ = center_object_loss_param.loc_weight();
    loc_offset_loss_type_ = center_object_loss_param.loc_loss_type();
    // fake shape.
    vector<int> loc_offset_shape(1, 1);
    loc_offset_shape.push_back(2);
    loc_offset_pred_.Reshape(loc_offset_shape);
    loc_offset_gt_.Reshape(loc_offset_shape);
    //loc_channel_gt_.Reshape(loc_shape);
    loc_offset_bottom_vec_.push_back(&loc_offset_pred_);
    loc_offset_bottom_vec_.push_back(&loc_offset_gt_);
    //loc_bottom_vec_.push_back(&loc_channel_gt_);
    loc_offset_loss_.Reshape(loss_shape);
    loc_offset_top_vec_.push_back(&loc_offset_loss_);
    if (loc_offset_loss_type_ == CenterObjectLossParameter_LocLossType_L2) {
        LayerParameter layer_param;
        layer_param.set_name(this->layer_param_.name() + "_offset_l2_loc");
        layer_param.set_type("EuclideanLoss");
        layer_param.add_loss_weight(loc_offset_weight_);
        loc_offset_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
        loc_offset_loss_layer_->SetUp(loc_offset_bottom_vec_, loc_offset_top_vec_);
    } else if (loc_offset_loss_type_ == CenterObjectLossParameter_LocLossType_SMOOTH_L1) {
        LayerParameter layer_param;
        layer_param.set_name(this->layer_param_.name() + "_offset_smooth_L1_loc");
        layer_param.set_type("SmoothL1Loss");
        layer_param.add_loss_weight(loc_offset_weight_);
        loc_offset_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
        loc_offset_loss_layer_->SetUp(loc_offset_bottom_vec_, loc_offset_top_vec_);
    } else {
        LOG(FATAL) << "Unknown loc loss type.";
    }

    loc_wh_weight_ = center_object_loss_param.loc_weight();
    loc_wh_loss_type_ = center_object_loss_param.loc_loss_type();
    // fake shape.
    vector<int> loc_wh_shape(1, 1);
    loc_wh_shape.push_back(2);
    loc_wh_pred_.Reshape(loc_wh_shape);
    loc_wh_gt_.Reshape(loc_wh_shape);
    loc_wh_bottom_vec_.push_back(&loc_wh_pred_);
    loc_wh_bottom_vec_.push_back(&loc_wh_gt_);
    loc_wh_loss_.Reshape(loss_shape);
    loc_wh_top_vec_.push_back(&loc_wh_loss_);
    if (loc_wh_loss_type_ == CenterObjectLossParameter_LocLossType_L2) {
        LayerParameter layer_param;
        layer_param.set_name(this->layer_param_.name() + "_wh_l2_loc");
        layer_param.set_type("EuclideanLoss");
        layer_param.add_loss_weight(loc_wh_weight_);
        loc_wh_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
        loc_wh_loss_layer_->SetUp(loc_wh_bottom_vec_, loc_wh_top_vec_);
    } else if (loc_wh_loss_type_ == CenterObjectLossParameter_LocLossType_SMOOTH_L1) {
        LayerParameter layer_param;
        layer_param.set_name(this->layer_param_.name() + "_wh_smooth_L1_loc");
        layer_param.set_type("SmoothL1Loss");
        layer_param.add_loss_weight(loc_wh_weight_);
        loc_wh_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
        loc_wh_loss_layer_->SetUp(loc_wh_bottom_vec_, loc_wh_top_vec_);
    } else {
        LOG(FATAL) << "Unknown loc loss type.";
    }
    // Set up landmark loss layer.
    if(has_lm_){
        lm_weight_ = center_object_loss_param.loc_weight();
        lm_loss_type_ = center_object_loss_param.lm_loss_type();
        // fake shape.
        vector<int> lm_shape(1, 1);
        lm_shape.push_back(10);
        lm_pred_.Reshape(lm_shape);
        lm_gt_.Reshape(lm_shape);
        lm_bottom_vec_.push_back(&lm_pred_);
        lm_bottom_vec_.push_back(&lm_gt_);
        lm_loss_.Reshape(loss_shape);
        lm_top_vec_.push_back(&lm_loss_);
        if (lm_loss_type_ == CenterObjectLossParameter_LocLossType_L2) {
            LayerParameter layer_param;
            layer_param.set_name(this->layer_param_.name() + "_l2_lm");
            layer_param.set_type("EuclideanLoss");
            layer_param.add_loss_weight(lm_weight_);
            lm_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
            lm_loss_layer_->SetUp(lm_bottom_vec_, lm_top_vec_);
        } else if (lm_loss_type_ == CenterObjectLossParameter_LocLossType_SMOOTH_L1) {
            LayerParameter layer_param;
            layer_param.set_name(this->layer_param_.name() + "_smooth_L1_lm");
            layer_param.set_type("SmoothL1Loss");
            layer_param.add_loss_weight(lm_weight_);
            lm_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
            lm_loss_layer_->SetUp(lm_bottom_vec_, lm_top_vec_);
        } else {
            LOG(FATAL) << "Unknown lm loss type.";
        }
    }
    
    // Set up confidence loss layer.
    conf_loss_type_ = center_object_loss_param.conf_loss_type();
    conf_bottom_vec_.push_back(&conf_pred_);
    conf_bottom_vec_.push_back(&conf_gt_);
    conf_loss_.Reshape(loss_shape);
    conf_top_vec_.push_back(&conf_loss_);
    if (conf_loss_type_ == CenterObjectLossParameter_ConfLossType_FOCALSIGMOID) {
        LayerParameter layer_param;
        layer_param.set_name(this->layer_param_.name() + "_sigmoid_conf");
        layer_param.set_type("CenterNetfocalSigmoidWithLoss");
        layer_param.add_loss_weight(Dtype(1.));
        layer_param.mutable_loss_param()->set_normalization(LossParameter_NormalizationMode_NONE);
        // Fake reshape.
        vector<int> conf_shape(1, 1);
        conf_gt_.Reshape(conf_shape);
        conf_shape.push_back(num_classes_);
        conf_pred_.Reshape(conf_shape);
        conf_loss_layer_ = LayerRegistry<Dtype>::CreateLayer(layer_param);
        conf_loss_layer_->SetUp(conf_bottom_vec_, conf_top_vec_);
    } else {
        LOG(FATAL) << "Unknown confidence loss type.";
    }
    iterations_ = 0;
}

template <typename Dtype>
void CenterObjectSingleLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
    LossLayer<Dtype>::Reshape(bottom, top);
    CHECK_GE(num_classes_, 1) << "num_classes should not be less than 1.";
    CHECK_EQ(num_classes_, bottom[2]->channels()) << "num_classes must be equal to prediction classes";
    CHECK_EQ(bottom[0]->width(), bottom[1]->width());
    CHECK_EQ(bottom[0]->height(), bottom[1]->height());
    CHECK_EQ(bottom[0]->width(), bottom[2]->width());
    CHECK_EQ(bottom[0]->height(), bottom[2]->height());
    if(has_lm_){
        CHECK_EQ(bottom[0]->width(), bottom[3]->width());
        CHECK_EQ(bottom[0]->height(), bottom[3]->height());
    }
}

template <typename Dtype>
void CenterObjectSingleLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
    const Dtype* loc_data = bottom[0]->cpu_data();
    const int output_height = bottom[0]->height();
    const int output_width = bottom[0]->width();
    const int loc_channels = bottom[0]->channels();

    const Dtype* wh_data = bottom[1]->cpu_data();
    if(has_lm_){
        lm_pred_temp_data_.CopyFrom(*bottom[3]);
    }else{
        lm_pred_temp_data_.ReshapeLike(*bottom[3]);
        caffe_set(lm_pred_temp_data_.count(), Dtype(0.), lm_pred_temp_data_.mutable_cpu_data());
    }

    num_gt_ = bottom[3]->height();
    if(has_lm_){
        num_gt_ = bottom[4]->height();
    }
    // Retrieve all ground truth.
    if(has_lm_){
        const Dtype* gt_data = bottom[4]->cpu_data();
        bool use_difficult_gt_ = true;
        Dtype background_label_id_ = -1;
        all_gt_bboxes.clear();
        GetCenternetGroundTruth(gt_data, num_gt_, background_label_id_, use_difficult_gt_,
                        &all_gt_bboxes, has_lm_);
    }else{
        const Dtype* gt_data = bottom[3]->cpu_data();
        bool use_difficult_gt_ = true;
        Dtype background_label_id_ = -1;
        all_gt_bboxes.clear();
        GetCenternetGroundTruth(gt_data, num_gt_, background_label_id_, use_difficult_gt_,
                        &all_gt_bboxes, has_lm_);
    }
    
    int num_groundtruth = 0;
    num_lm_ = 0;
    for(int i = 0; i < all_gt_bboxes.size(); i++){
        vector<std::pair<NormalizedBBox, AnnoFaceLandmarks> > gt_boxes = all_gt_bboxes[i];
        num_groundtruth += gt_boxes.size();
        for(unsigned ii = 0;  ii < gt_boxes.size(); ii++){
            if(gt_boxes[ii].second.lefteye().x() > 0 && gt_boxes[ii].second.lefteye().y() > 0 &&
                   gt_boxes[ii].second.righteye().x() > 0 && gt_boxes[ii].second.righteye().y() > 0 && 
                   gt_boxes[ii].second.nose().x() > 0 && gt_boxes[ii].second.nose().y() > 0 &&
                   gt_boxes[ii].second.leftmouth().x() > 0 && gt_boxes[ii].second.leftmouth().y() > 0 &&
                   gt_boxes[ii].second.rightmouth().x() > 0 && gt_boxes[ii].second.rightmouth().y() > 0){
                num_lm_++;
            }
        }
    }
    CHECK_EQ(num_gt_, num_groundtruth);
    if (num_gt_ >= 1) {
        // Form data to pass on to loc_loss_layer_.
        vector<int> loc_shape(2);
        loc_shape[0] = 1;
        loc_shape[1] = num_gt_ * 2;
        loc_offset_pred_.Reshape(loc_shape);
        loc_offset_gt_.Reshape(loc_shape);
        Dtype* loc_offset_pred_data = loc_offset_pred_.mutable_cpu_data();
        Dtype* loc_offset_gt_data = loc_offset_gt_.mutable_cpu_data();
        
        loc_wh_pred_.Reshape(loc_shape);
        loc_wh_gt_.Reshape(loc_shape);
        Dtype* loc_wh_pred_data = loc_wh_pred_.mutable_cpu_data();
        Dtype* loc_wh_gt_data = loc_wh_gt_.mutable_cpu_data();
        if(num_lm_ >0 && has_lm_){
            loc_shape[0] = 1;
            loc_shape[1] = num_lm_ * 10;
            lm_pred_.Reshape(loc_shape);
            lm_gt_.Reshape(loc_shape);
        }else{
            loc_shape[0] = 1;
            loc_shape[1] = 10;
            lm_pred_.Reshape(loc_shape);
            lm_gt_.Reshape(loc_shape);
        }
        Dtype* lm_pred_data = lm_pred_.mutable_cpu_data();
        Dtype* lm_gt_data = lm_gt_.mutable_cpu_data();
        const Dtype* lm_temp_data_ = lm_pred_temp_data_.cpu_data();
        const int lm_channels = lm_pred_temp_data_.channels();
        GetLocTruthAndPrediction(loc_offset_gt_data, loc_offset_pred_data,
                                    loc_wh_gt_data, loc_wh_pred_data,
                                    lm_gt_data, lm_pred_data,
                                    output_width, output_height, share_location_,
                                    loc_data, wh_data, lm_temp_data_,loc_channels, lm_channels,
                                    all_gt_bboxes, has_lm_);
        loc_offset_loss_layer_->Reshape(loc_offset_bottom_vec_, loc_offset_top_vec_);
        loc_offset_loss_layer_->Forward(loc_offset_bottom_vec_, loc_offset_top_vec_);

        loc_wh_loss_layer_->Reshape(loc_wh_bottom_vec_, loc_wh_top_vec_);
        loc_wh_loss_layer_->Forward(loc_wh_bottom_vec_, loc_wh_top_vec_);

        if(has_lm_ && num_lm_ >0){
            lm_loss_layer_->Reshape(lm_bottom_vec_, lm_top_vec_);
            lm_loss_layer_->Forward(lm_bottom_vec_, lm_top_vec_);
        }

    } else {
        loc_offset_loss_.mutable_cpu_data()[0] = 0;
        loc_wh_loss_.mutable_cpu_data()[0] = 0;
    }

    if (num_gt_ >= 1) {
        if (conf_loss_type_ == CenterObjectLossParameter_ConfLossType_FOCALSIGMOID) {
            conf_gt_.ReshapeLike(*bottom[2]);
            conf_pred_.ReshapeLike(*bottom[2]);
            conf_pred_.CopyFrom(*bottom[2]);
        }else {
            LOG(FATAL) << "Unknown confidence loss type.";
        }
        Dtype* conf_gt_data = conf_gt_.mutable_cpu_data();
        caffe_set(conf_gt_.count(), Dtype(0), conf_gt_data);
        GenerateBatchHeatmap(all_gt_bboxes, conf_gt_data, num_classes_, output_width, output_height);
        conf_loss_layer_->Reshape(conf_bottom_vec_, conf_top_vec_);
        conf_loss_layer_->Forward(conf_bottom_vec_, conf_top_vec_);
    } else {
        conf_loss_.mutable_cpu_data()[0] = 0;
    }

    top[0]->mutable_cpu_data()[0] = 0;
    Dtype loc_offset_loss = Dtype(0.), loc_wh_loss = Dtype(0.), cls_loss = Dtype(0.), lm_loss = Dtype(0.);
    Dtype normalizer = LossLayer<Dtype>::GetNormalizer(
            normalization_, num_, 1, num_gt_);

    Dtype lm_normalizer = LossLayer<Dtype>::GetNormalizer(
            normalization_, num_, 1, num_lm_);

    normalizer = normalizer > 0 ? normalizer : num_;
    lm_normalizer = lm_normalizer > 0 ?  lm_normalizer : num_;

    if (this->layer_param_.propagate_down(0)) {
        loc_offset_loss = 1. * Dtype(loc_offset_loss_.cpu_data()[0] / normalizer);
    }
    if (this->layer_param_.propagate_down(1)) {
        loc_wh_loss = Dtype(loc_wh_loss_.cpu_data()[0] / normalizer);
    }

    if (this->layer_param_.propagate_down(2)) {
        cls_loss = 1. * Dtype(conf_loss_.cpu_data()[0] / normalizer);
    }

    if(has_lm_ && num_lm_>0){
        if (this->layer_param_.propagate_down(3)) {
            lm_loss = Dtype(lm_loss_.cpu_data()[0] / lm_normalizer);
        }
    }

    if(has_lm_ && num_lm_ >0){
        top[0]->mutable_cpu_data()[0] = cls_loss + loc_offset_loss + loc_wh_loss + lm_loss;
    }else{
        top[0]->mutable_cpu_data()[0] = cls_loss + loc_offset_loss + loc_wh_loss;
    }

    #if 1 
    if(iterations_ % 100 == 0){
        LOG(INFO)<<"total loss: "<<top[0]->mutable_cpu_data()[0]
                <<", loc offset loss: "<<loc_offset_loss
                <<", loc wh loss: "<<loc_wh_loss
                <<", conf loss: "<< cls_loss
                <<", lm loss: "<< lm_loss;
        LOG(INFO)<<"normalizer: "<< normalizer
                <<", lm_normalizer: "<<lm_normalizer
                <<", num_lm_: "<<num_lm_
                <<", num_gt_box_: "<<num_gt_
                <<", num_class: "<<num_classes_
                <<", output_width: "<<output_width
                <<", output_height: "<<output_height;
    }
    iterations_++;
    #endif
}

template <typename Dtype>
void CenterObjectSingleLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
    const int output_height = bottom[0]->height();
    const int output_width = bottom[0]->width();
    const int loc_channels = bottom[0]->channels();
    const int lm_channels = bottom[3]->channels();
    if(has_lm_){
        if (propagate_down[4]) {
            LOG(FATAL) << this->type()
                << " Layer cannot backpropagate to label inputs.";
        }
    }else{
        if (propagate_down[3]) {
            LOG(FATAL) << this->type()
                << " Layer cannot backpropagate to label inputs.";
        }
    }
    // Back propagate on location offset prediction.
    Dtype normalizer = LossLayer<Dtype>::GetNormalizer(
            normalization_, num_, 1, num_gt_);
    Dtype lm_normalizer = LossLayer<Dtype>::GetNormalizer(
            normalization_, num_, 1, num_lm_);
    normalizer = normalizer > 0 ? normalizer : num_;
    lm_normalizer = lm_normalizer > 0 ? lm_normalizer : num_;
    
    if (num_gt_ >= 1) {
        vector<bool> layer_propagate_down;
        layer_propagate_down.push_back(true);
        layer_propagate_down.push_back(false);
        if (propagate_down[0]) {
            Dtype* offset_bottom_diff = bottom[0]->mutable_cpu_diff();
            caffe_set(bottom[0]->count(), Dtype(0), offset_bottom_diff);
            // diff of offset
            loc_offset_loss_layer_->Backward(loc_offset_top_vec_, layer_propagate_down,
                                        loc_offset_bottom_vec_);
            Dtype loss_offset_weight = 1 * top[0]->cpu_diff()[0] / normalizer;
            caffe_scal(loc_offset_pred_.count(), loss_offset_weight, loc_offset_pred_.mutable_cpu_diff());
        }
        if(propagate_down[1]){
            Dtype* wh_bottom_diff = bottom[1]->mutable_cpu_diff();
            caffe_set(bottom[1]->count(), Dtype(0), wh_bottom_diff);
            // diff of wh
            loc_wh_loss_layer_->Backward(loc_wh_top_vec_, layer_propagate_down,
                                        loc_wh_bottom_vec_);
            Dtype loss_wh_weight = top[0]->cpu_diff()[0] / normalizer;
            caffe_scal(loc_wh_pred_.count(), loss_wh_weight, loc_wh_pred_.mutable_cpu_diff());
        }
        if(has_lm_ && num_lm_ >0){
            if(propagate_down[3]){
                Dtype* lm_bottom_diff = bottom[3]->mutable_cpu_diff();
                caffe_set(bottom[3]->count(), Dtype(0), lm_bottom_diff);
                lm_loss_layer_->Backward(lm_top_vec_, layer_propagate_down,
                                            lm_bottom_vec_);
                Dtype lm_weight = top[0]->cpu_diff()[0] / lm_normalizer;
                caffe_scal(lm_pred_.count(), lm_weight, lm_pred_.mutable_cpu_diff());
            }
        }
        const Dtype* loc_offset_pred_diff = loc_offset_pred_.cpu_diff();
        Dtype* offset_bottom_diff = bottom[0]->mutable_cpu_diff();
        const Dtype* loc_wh_pred_diff = loc_wh_pred_.cpu_diff();
        Dtype* wh_bottom_diff = bottom[1]->mutable_cpu_diff();
        const Dtype* lm_pred_diff = lm_pred_.cpu_diff();
        Dtype* lm_bottom_diff = bottom[2]->mutable_cpu_diff();

        CopySingleDiffToBottom(loc_offset_pred_diff, loc_wh_pred_diff, output_width, 
                                output_height, has_lm_, lm_pred_diff, share_location_, 
                                offset_bottom_diff, wh_bottom_diff, lm_bottom_diff, 
                                loc_channels, lm_channels, all_gt_bboxes);
    }

    // Back propagate on confidence prediction.
    if (propagate_down[2]) {
        Dtype* conf_bottom_diff = bottom[2]->mutable_cpu_diff();
        caffe_set(bottom[2]->count(), Dtype(0), conf_bottom_diff);
        if (num_gt_ >= 1) {
            vector<bool> conf_propagate_down;
            conf_propagate_down.push_back(true);
            conf_propagate_down.push_back(false);
            conf_loss_layer_->Backward(conf_top_vec_, conf_propagate_down,
                                        conf_bottom_vec_);
            Dtype loss_weight = top[0]->cpu_diff()[0] / normalizer;
            caffe_scal(conf_pred_.count(), loss_weight, conf_pred_.mutable_cpu_diff());
            bottom[2]->ShareDiff(conf_pred_);
        }
    }
}

INSTANTIATE_CLASS(CenterObjectSingleLossLayer);
REGISTER_LAYER_CLASS(CenterObjectSingleLoss);

}  // namespace caffe
