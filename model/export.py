import subprocess
import onnx
from tensorflow import keras
import hls4ml

# load trained model
model = keras.models.load_model('lenet5.keras')

# save as SavedModel first
model.export('lenet5_saved_model')

# convert SavedModel to ONNX via CLI
subprocess.run([
    'python3', '-m', 'tf2onnx.convert',
    '--saved-model', 'lenet5_saved_model',
    '--output', 'lenet5.onnx'
], check=True)

# verify ONNX model
onnx_model = onnx.load('lenet5.onnx')
onnx.checker.check_model(onnx_model)
print("ONNX model is valid")

# configure hls4ml
hls_config = hls4ml.utils.config_from_keras_model(model, granularity='model')
hls_config['Model']['Precision'] = 'ap_fixed<16,6>'
hls_config['Model']['ReuseFactor'] = 64

# convert to HLS project
hls_model = hls4ml.converters.convert_from_keras_model(
    model,
    hls_config=hls_config,
    output_dir='../hls4ml/lenet5_project',
    part='xc7z010clg400-1',
    backend='Vitis'
)

# plot model architecture
hls4ml.utils.plot_model(hls_model, show_shapes=True, show_precision=True, to_file='hls4ml_model.png')
print("hls4ml compatibility check passed")

# generate C++ HLS code
hls_model.compile()
print("HLS project generated at ../hls4ml/lenet5_project")
