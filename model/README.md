---
library_name: transformers
license: apple-amlr
metrics:
- accuracy
pipeline_tag: image-feature-extraction
tags:
- vision
- image-feature-extraction
- mlx
- pytorch
---
# Introduction
[[`AIMv2 Paper`](https://arxiv.org/abs/2411.14402)] [[`BibTeX`](#citation)]

We introduce the AIMv2 family of vision models pre-trained with a multimodal autoregressive objective.
AIMv2 pre-training is simple and straightforward to train and scale effectively. Some AIMv2 highlights include:

1. Outperforms OAI CLIP and SigLIP on the majority of multimodal understanding benchmarks.
2. Outperforms DINOv2 on open-vocabulary object detection and referring expression comprehension.
3. Exhibits strong recognition performance with AIMv2-3B achieving *89.5% on ImageNet using a frozen trunk*.

<img src="aimv2_overview_light.png" alt="AIMv2 Overview"/>

## Usage

### PyTorch
```python
import requests
from PIL import Image
from transformers import AutoImageProcessor, AutoModel

url = "http://images.cocodataset.org/val2017/000000039769.jpg"
image = Image.open(requests.get(url, stream=True).raw)

processor = AutoImageProcessor.from_pretrained(
    "apple/aimv2-large-patch14-native",
)
model = AutoModel.from_pretrained(
    "apple/aimv2-large-patch14-native",
    trust_remote_code=True,
)

inputs = processor(images=image, return_tensors="pt")
outputs = model(**inputs)
```

### JAX
```python
import requests
from PIL import Image
from transformers import AutoImageProcessor, FlaxAutoModel

url = "http://images.cocodataset.org/val2017/000000039769.jpg"
image = Image.open(requests.get(url, stream=True).raw)

processor = AutoImageProcessor.from_pretrained(
    "apple/aimv2-large-patch14-native",
)
model = FlaxAutoModel.from_pretrained(
    "apple/aimv2-large-patch14-native",
    trust_remote_code=True,
)

inputs = processor(images=image, return_tensors="jax")
outputs = model(**inputs)
```

## Citation
If you find our work useful, please consider citing us as:
```bibtex
@misc{fini2024multimodalautoregressivepretraininglarge,
  author      = {Fini, Enrico and Shukor, Mustafa and Li, Xiujun and Dufter, Philipp and Klein, Michal and Haldimann, David and Aitharaju, Sai and da Costa, Victor Guilherme Turrisi and Béthune, Louis and Gan, Zhe and Toshev, Alexander T and Eichner, Marcin and Nabi, Moin and Yang, Yinfei and Susskind, Joshua M. and El-Nouby, Alaaeldin},
  url         = {https://arxiv.org/abs/2411.14402},
  eprint      = {2411.14402},
  eprintclass = {cs.CV},
  eprinttype  = {arXiv},
  title       = {Multimodal Autoregressive Pre-training of Large Vision Encoders},
  year        = {2024},
}
```

