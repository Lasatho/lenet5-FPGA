import tf_keras as keras
from lenet5 import *
import numpy as np

LeNet = LeNet5()

LeNet.model.compile(
    optimizer='adam',
    loss='sparse_categorical_crossentropy',
    metrics=['accuracy']
)

(x_train, y_train), (x_test, y_test) = keras.datasets.mnist.load_data()

# resize to 32x32 
x_train = np.pad(x_train,  ((0,0), (2,2), (2,2)), 'constant')
x_test  = np.pad(x_test,  ((0,0),(2,2),(2,2)), 'constant')

# Add channel dimension and normalize to 0.0-1.0
x_train = x_train[..., np.newaxis] / 255.0
x_test  = x_test[...,  np.newaxis] / 255.0

# fit model
LeNet.model.fit(
    x_train, y_train,
    epochs=7,
    batch_size=64,
    validation_split=0.1
)

# eval model
loss, accuracy = LeNet.model.evaluate(x_test, y_test)
print(f"Accuracy: {accuracy:.4f}")

# Inference 
predictions = LeNet.model.predict(x_test[:5])
predicted_classes = np.argmax(predictions, axis=1)

print(f"Vorhersage: {predicted_classes}")

LeNet.model.save('lenet5.keras')
