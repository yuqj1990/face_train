import math
from net import mobilenet
from data import market1501_dataset
import numpy as np
import tensorflow as tf

CurrentAdjustStep = 0

# set tf.Flages as follow
# set learning rate policy adjust
# set data parallel
# set model net
# add train log loss, evaluation
# set train optimizer gradient
# save model to file , save val_list, restore to continue train
# log loss , accuracy
# evaluation function

FLAGS = tf.flags.FLAGS
tf.flags.DEFINE_integer('batch_size', '30', 'batch size for training')
tf.flags.DEFINE_integer('max_steps', '210000', 'max steps for training')

tf.flags.DEFINE_string('logs_dir', '../snapshot/', 'path to logs directory')
tf.flags.DEFINE_string('imgaes_path', '../train_market.txt', 'path to dataset')

tf.flags.DEFINE_float('learning_rate', '0.01', '')
tf.flags.DEFINE_string('learning_policy', 'MultiStep', 'learing rate decay policy')
tf.flags.DEFINE_float('gamma', '0.9', ' gamma used to decay')
tf.flags.DEFINE_list('MultiStepValue', '20000, 100000, 300000', 'multiStep value')

tf.flags.DEFINE_string('mode', 'train', 'Mode train, val, test')


def adjust_learning_rate_by_policy(base_lr_rate, gamma, iter, step=None, policy='MultiStep'):
    if policy == 'MultiStep':
        global CurrentAdjustStep
        if CurrentAdjustStep < len(FLAGS.MultiStepValue) and iter > FLAGS.MultiStepValue[CurrentAdjustStep]:
            CurrentAdjustStep += 1
        return base_lr_rate * math.pow(gamma, CurrentAdjustStep)
    elif policy == 'fixed':
        return base_lr_rate
    elif policy == 'step':
        return base_lr_rate *math.pow(gamma, math.floor(iter/step))
    elif policy == 'exp':
        return base_lr_rate*math.pow(gamma, iter)


def adjust_optimizer_by_tensorflow(optimizer, learning_rate_placeholder):
    if optimizer == 'ADAGRAD':
        opt = tf.train.AdagradOptimizer(learning_rate_placeholder)
    elif optimizer == 'ADADELTA':
        opt = tf.train.AdadeltaOptimizer(learning_rate_placeholder, rho=0.9, epsilon=1e-6)
    elif optimizer == 'ADAM':
        opt = tf.train.AdamOptimizer(learning_rate_placeholder, beta1=0.9, beta2=0.999, epsilon=0.1)
    elif optimizer == 'RMSPROP':
        opt = tf.train.RMSPropOptimizer(learning_rate_placeholder, decay=0.9, momentum=0.9, epsilon=1.0)
    elif optimizer == 'MOM':
        opt = tf.train.MomentumOptimizer(learning_rate_placeholder, 0.9, use_nesterov=True)
    elif optimizer == 'SGD':
        opt = tf.train.GradientDescentOptimizer(learning_rate_placeholder)
    else:
        raise ValueError('Invalid optimization algorithm')
    return opt


def train(sess, feed_dict, loss, train_op, save_opt, iter, log_dir):
    sess.run(tf.global_variables_initializer())
    if iter % 5000 == 0:
        save_opt.save(sess, log_dir + 'model.ckpt', iter)


def main():
    # train_placeholder
    images_path_placeholder = tf.placeholder(name='images_path', dtype=tf.string, shape=[None, ])
    images_placeholder = tf.placeholder(name='images', dtype=tf.float32, shape=[None, 160, 80, 3])
    labels_placeholder = tf.placeholder(name='labels', dtype=tf.int64, shape=[None, ])
    batch_size_placeholder = tf.placeholder(name='batch_size', dtype=tf.int64)
    phase_train_placeholder = tf.placeholder(name='phase_train', dtype=bool)
    learning_rate_placeholder = tf.placeholder(name = 'lr', dtype=tf.float32)
    # dataset iterator
    dataset_iterator = market1501_dataset.generate_dataset_softmax(images_path_placeholder,
                                                          labels_placeholder, batch_size_placeholder)
    # net input
    net = mobilenet.mobilenet({'image': images_placeholder}, trainable=True, conv_basechannel=1.0)
    fc_output = net.get_output(name='fc_output')
    normalize_output = tf.nn.l2_normalize(fc_output, axis= 1, epsilon= 0.000001)

    softmaxWithLoss = net.entropy_softmax_withloss(normalize_output, labels_placeholder)
    # global train solver op
    global_step = tf.Variable(0, name='global_step', trainable=False) # zhe yi bu wo you dian bu dong

    solver_opt = adjust_optimizer_by_tensorflow(optimizer='SGD', learning_rate_placeholder=learning_rate_placeholder)
    train_op = solver_opt.minimize(loss=softmaxWithLoss, global_step= global_step)

    # saver model
    saver_opt = tf.train.Saver()
    ckpt = tf.train.get_checkpoint_state(FLAGS.log_dir)

    with tf.Session() as sess:
        sess.run(tf.global_variables_initializer())
        if ckpt and ckpt.model_checkpoint_path:
            print('Restore model')
            saver_opt.restore(sess, ckpt.model_checkpoint_path)
        sess.run(global_step)
        imagepaths, labels =market1501_dataset.get_list_from_label_file(FLAGS.imgaes_path)
        sess.run(dataset_iterator.initializer, {images_path_placeholder: imagepaths,
                                                batch_size_placeholder: FLAGS.batch_size,
                                                labels_placeholder: labels})
        dataelement = dataset_iterator.get_next()
        for iter in range(FLAGS.max_steps):
            images_batch, label_batch = sess.run(dataelement)
            lr_rate = adjust_learning_rate_by_policy(FLAGS.learning_rate, gamma=FLAGS.gamma, policy=FLAGS.learning_policy, iter=iter)
            # label_batch = tf.transpose(label_batch)
            feed_dict = {images_placeholder: images_batch,
                        labels_placeholder: label_batch,
                         phase_train_placeholder: True,
                        learning_rate_placeholder: lr_rate}
            _, train_loss = sess.run([train_op, softmaxWithLoss], feed_dict=feed_dict)
            print('Step: %d, Learning rate: %f, Train loss: %f' % (iter, lr_rate, train_loss))

            if iter % 5000 == 0:
                saver_opt.save(sess, FLAGS.log_dir + 'model.ckpt', iter)


if __name__=='__main__':
    main()



















