import numpy as np
import argparse
import sys,os  
import cv2
caffe_root = '../../../../caffe_train/'
sys.path.insert(0, caffe_root + 'python')  
import caffe  

def make_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument('--model', type=str, help='.prototxt file for inference', default ='../net/face_detector.prototxt')
    parser.add_argument('--weights', type=str, help='.caffemodel file for inference', default ='../net/face_detector.caffemodel')
    parser.add_argument('--input', type = int, help='net input', default = 320)
    parser.add_argument('--sameAvg', type = int, help='net input', default = 1)
    return parser
parser1 = make_parser()
args = parser1.parse_args()
net_file= args.model
caffe_model= args.weights
test_dir = "../images"

inputsize = args.input
mean_value = [127.5, 127.5, 127.5]
if not args.sameAvg:
    mean_value = [103.94, 116.78, 123.68]
    inputsize = args.input

if not os.path.exists(caffe_model):
    print(caffe_model + " does not exist")
    exit()
if not os.path.exists(net_file):
    print(net_file + " does not exist")
    exit()
caffe.set_mode_gpu();
caffe.set_device(0);
net = caffe.Net(net_file,caffe_model,caffe.TEST)  

CLASSES = ('background',
           'face')


def preprocess(src, inputsize, mean_value):
    img = cv2.resize(src, (inputsize,inputsize))
    img = img -mean_value
    img = img * 0.007843
    return img

def postprocess(img, out):   
    h = img.shape[0]
    w = img.shape[1]
    box = out['detection_out'][0,0,:,3:7] * np.array([w, h, w, h])

    cls = out['detection_out'][0,0,:,1]
    conf = out['detection_out'][0,0,:,2]
    return (box.astype(np.int32), conf, cls)

def detect(imgfile):
    origimg = cv2.imread(imgfile)
    img = preprocess(origimg, inputsize, mean_value)
    
    img = img.astype(np.float32)
    img = img.transpose((2, 0, 1))

    net.blobs['data'].data[...] = img
    out = net.forward()
    print("out shape",out['detection_out'].shape)  
    box, conf, cls = postprocess(origimg, out)

    for i in range(len(box)):
       if conf[i]>=0.25:
           p1 = (box[i][0], box[i][1])
           p2 = (box[i][2], box[i][3])
           cv2.rectangle(origimg, p1, p2, (0,255,0))
           p3 = (max(p1[0], 15), max(p1[1], 15))
           title = "%s:%.2f" % (CLASSES[int(cls[i])], conf[i])
           cv2.putText(origimg, title, p3, cv2.FONT_ITALIC, 0.6, (0, 255, 0), 1)
    cv2.imshow("facedetector", origimg)
 
    k = cv2.waitKey(0) & 0xff
        #Exit if ESC pressed
    if k == 27 : return False
    return True

for f in os.listdir(test_dir):
    if detect(test_dir + "/" + f) == False:
       break

