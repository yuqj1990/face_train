#!/bin/sh
if ! test -f ./prototxt/inception_tiny/inception_resnet_v2_tiny_train.prototxt ;then
	echo "error: inception_resnet_v2_tiny_train.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
../../../../build/tools/caffe train --solver=./prototxt/inception_tiny/inception_resnet_v2_tiny_solver.prototxt -gpu 0 \
#--snapshot=../snapshot/face_iter_5000.solverstate