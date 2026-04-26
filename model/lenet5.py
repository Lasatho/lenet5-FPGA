import tf_keras as keras
from tf_keras import layers

class LeNet5():
    def __init__(self):
        self.model = keras.Sequential([
            keras.Input(shape=(32,32,1)),
            layers.Conv2D(6, kernel_size=5, activation='relu'),
            layers.AveragePooling2D(pool_size=2),
            layers.Conv2D(16, kernel_size=5, activation='relu'),
            layers.AveragePooling2D(pool_size=2),
            layers.Flatten(),
            layers.Dense(120, activation='relu'),
            layers.Dense(84, activation='relu'),
            layers.Dense(10, activation='softmax')
        ])

if __name__ == "__main__":
    model = LeNet5()
    model.model.summary()
