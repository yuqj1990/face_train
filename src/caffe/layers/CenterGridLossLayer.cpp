#include <algorithm>
#include <map>
#include <utility>
#include <vector>

#include "caffe/layers/CenterGridLossLayer.hpp"
#include "caffe/util/math_functions.hpp"

namespace caffe {

template <typename Dtype>
void CenterGridLossLayer<Dtype>::LayerSetUp(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::LayerSetUp(bottom, top);
  if (this->layer_param_.propagate_down_size() == 0) {
    this->layer_param_.add_propagate_down(true);
    this->layer_param_.add_propagate_down(false);
  }
  const CenterObjectParameter& center_object_loss_param =
      this->layer_param_.center_object_loss_param();
  
  // bias_mask_
  if(center_object_loss_param.has_bias_num()){
    CHECK_EQ(center_object_loss_param.bias_scale_size(), 1);
    CHECK_EQ(center_object_loss_param.bias_num(), 1);
    anchor_scale_ = center_object_loss_param.bias_scale(0);
    int low_bbox = center_object_loss_param.low_bbox_scale();
    int up_bbox = center_object_loss_param.up_bbox_scale();
    bbox_range_scale_ = std::make_pair(low_bbox, up_bbox);
  }
  
  net_height_ = center_object_loss_param.net_height();
  net_width_ = center_object_loss_param.net_width();
  ignore_thresh_ = center_object_loss_param.ignore_thresh();
  
  num_classes_ = center_object_loss_param.num_class();
  CHECK_GE(num_classes_, 1) << "num_classes should not be less than 1.";
  CHECK_EQ((4 + 1 + num_classes_) *1, bottom[0]->channels()) 
            << "num_classes must be equal to prediction classes";
  
  if (!this->layer_param_.loss_param().has_normalization() &&
      this->layer_param_.loss_param().has_normalize()) {
    normalization_ = this->layer_param_.loss_param().normalize() ?
                     LossParameter_NormalizationMode_VALID :
                     LossParameter_NormalizationMode_BATCH_SIZE;
  } else {
    normalization_ = this->layer_param_.loss_param().normalization();
  }
  iterations_ = 0;
  vector<int> label_shape(1, 1);
  label_shape.push_back(1);
  label_data_.Reshape(label_shape);
}

template <typename Dtype>
void CenterGridLossLayer<Dtype>::Reshape(const vector<Blob<Dtype>*>& bottom,
      const vector<Blob<Dtype>*>& top) {
  LossLayer<Dtype>::Reshape(bottom, top);
}

template <typename Dtype>
void CenterGridLossLayer<Dtype>::Forward_cpu(const vector<Blob<Dtype>*>& bottom,
    const vector<Blob<Dtype>*>& top) {
  
  // gt_boxes
  const Dtype* gt_data = bottom[1]->cpu_data();
  num_gt_ = bottom[1]->height(); 
  bool use_difficult_gt_ = true;
  Dtype background_label_id_ = -1;
  num_ = bottom[0]->num();
  all_gt_bboxes.clear();
  
  GetYoloGroundTruth(gt_data, num_gt_, background_label_id_, use_difficult_gt_,
                 &all_gt_bboxes, num_);
  num_groundtruth_ = 0;
  for(int i = 0; i < all_gt_bboxes.size(); i++){
    vector<NormalizedBBox> gt_boxes = all_gt_bboxes[i];
    num_groundtruth_ += gt_boxes.size();
  }
  // prediction data
  Dtype* channel_pred_data = bottom[0]->mutable_cpu_data();
  const int output_height = bottom[0]->height();
  const int output_width = bottom[0]->width();
  const int num_channels = bottom[0]->channels();
  Dtype * bottom_diff = bottom[0]->mutable_cpu_diff();

  vector<int> label_shape(2, 1);
  label_shape.push_back(num_);
  label_shape.push_back(output_height*output_width);
  label_data_.Reshape(label_shape);

  Dtype *label_muti_data = label_data_.mutable_cpu_data();

  Dtype class_score = Dtype(0.);

  caffe_set(bottom[0]->count(), Dtype(0), bottom_diff);
  if (num_groundtruth_ >= 1) {
    class_score = EncodeCenterGridObject(num_, num_channels, num_classes_, output_width, output_height, 
                          net_width_, net_height_,
                          channel_pred_data,  anchor_scale_, 
                          bbox_range_scale_,
                          all_gt_bboxes, label_muti_data, bottom_diff, 
                          ignore_thresh_, &count_postive_);
    const Dtype * diff = bottom[0]->cpu_diff();
    Dtype sum_squre = Dtype(0.);
    int dimScale = output_height * output_width;
    for(int b = 0; b < num_; b++){
      for(int j = 0; j < 5 * dimScale; j++){ // loc loss
        sum_squre += diff[b * (4 + 1 + num_classes_) * dimScale + j] * diff[b * (4 + 1 + num_classes_) * dimScale + j];
      }
    }
    
    top[0]->mutable_cpu_data()[0] = (sum_squre + class_score) / num_;
  } else {
    top[0]->mutable_cpu_data()[0] = 0;
  }
  #if 1 
  if(iterations_ % 100 == 0){    
    LOG(INFO);     
    LOG(INFO)<<"all num_gt boxes: "<<num_gt_<<", Region "<<output_width
              <<": total loss: "<<top[0]->mutable_cpu_data()[0]
              <<", class score: "<<class_score / count_postive_
              <<", count: "<<count_postive_;
  }
  iterations_++;
  #endif
}

template <typename Dtype>
void CenterGridLossLayer<Dtype>::Backward_cpu(const vector<Blob<Dtype>*>& top,
    const vector<bool>& propagate_down,
    const vector<Blob<Dtype>*>& bottom) {
  if (propagate_down[1]) {
    LOG(FATAL) << this->type()
        << " Layer cannot backpropagate to label inputs.";
  }
  
  if (propagate_down[0]) {
    Dtype loss_weight = top[0]->cpu_diff()[0] / num_;
    caffe_scal(bottom[0]->count(), loss_weight, bottom[0]->mutable_cpu_diff());
  }
}

INSTANTIATE_CLASS(CenterGridLossLayer);
REGISTER_LAYER_CLASS(CenterGridLoss);

}  // namespace caffe