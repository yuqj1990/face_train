#!/bin/sh
if ! test -f ../prototxt/deepano_face_train_v1.prototxt ;then
	echo "error: ../prototxt/deepano_face_train_v1.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
if ! test -f ../prototxt/deepano_face_test_v1.prototxt ;then
	echo "error: ../prototxt/deepano_face_test_v1.prototxt does not exit."
	echo "please generate your own model prototxt primarily."
        exit 1
fi
<<<<<<< HEAD
../../../../build/tools/caffe train --solver=../solver/deepano_solver_train_v1.prototxt -gpu 0 \
--snapshot=../snapshot/face_detector_v1_iter_3415.solverstate
=======
../../../../build/tools/caffe train --solver=../solver/deepano_solver_train_v1.prototxt -gpu 2 \
#--snapshot=../snapshot/face_detector_v1_iter_1836.solverstate
>>>>>>> c368102f788bd32345ef12d452cf7e0b7e144c25
