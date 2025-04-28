use anyhow::{Result, Context};
use tch::{
    nn::{self, Module, ModuleT}, // Added ModuleT
    Kind, Tensor, Device, vision
};

use std::path::Path;

// use aimv2_model_rs::norm::{RMSNorm};
use aimv2_model_rs::utils::{auto_device};
// use aimv2_model_rs::ffn::{AIMv2SwiGLUFFN};
use aimv2_model_rs::config::{AIMv2Config};
// use aimv2_model_rs::attention::{AIMv2Attention};

use aimv2_model_rs::preprocessor::{AIMv2ViTPreprocessor};

// use aimv2_model_rs::block::{AIMv2Block};

use aimv2_model_rs::transformer::{AIMv2Transformer};


// /// Transformer Block for AIMv2.
// #[derive(Debug)]
// pub struct AIMv2Block {
//     attn: AIMv2Attention,
//     norm_1: RMSNorm,
//     mlp: AIMv2SwiGLUFFN,
//     norm_2: RMSNorm,
// }

// impl AIMv2Block {
//     pub fn new(vs: &nn::Path, config: &AIMv2Config) -> Self {
//         let dim = config.hidden_size;
//         let eps = config.rms_norm_eps;

//         let attn = AIMv2Attention::new(&(vs / "attn"), config);
//         let norm_1 = RMSNorm::new(&(vs / "norm_1"), dim, eps);
//         let mlp = AIMv2SwiGLUFFN::new(&(vs / "mlp"), config);
//         let norm_2 = RMSNorm::new(&(vs / "norm_2"), dim, eps);

//         Self { attn, norm_1, mlp, norm_2 }
//     }
// }

// impl nn::ModuleT for AIMv2Block {
//     fn forward_t(&self, xs: &Tensor, train: bool) -> Tensor {
//         let residual_1 = xs + self.attn.forward_t(&self.norm_1.forward(xs), train);
//         residual_1.shallow_clone() + self.mlp.forward(&self.norm_2.forward(&residual_1))
//     }
// }


/// The main Transformer Trunk for AIMv2.
// #[derive(Debug)]
// pub struct AIMv2Transformer {
//     blocks: Vec<AIMv2Block>,
//     post_trunk_norm: RMSNorm,
// }

// impl AIMv2Transformer {
//     pub fn new(vs: &nn::Path, config: &AIMv2Config) -> Self {
//         let num_hidden_layers = config.num_hidden_layers;
//         let hidden_size = config.hidden_size;
//         let rms_norm_eps = config.rms_norm_eps;

//         let mut blocks = Vec::with_capacity(num_hidden_layers as usize);
//         let blocks_vs = vs / "blocks"; // Path for the blocks module list

//         for i in 0..num_hidden_layers {
//             // Correctly construct path for each block: trunk.blocks.<i>
//             let block_path = blocks_vs.clone() / i.to_string();
//             blocks.push(AIMv2Block::new(&block_path, config));
//         }

//         let post_trunk_norm = RMSNorm::new(&(vs / "post_trunk_norm"), hidden_size, rms_norm_eps);

//         Self { blocks, post_trunk_norm }
//     }

//     pub fn forward_(&self, tokens: &Tensor, train: bool) -> Tensor {
//         let mut current_tokens = tokens.shallow_clone();
//         for block in &self.blocks {
//             current_tokens = block.forward_t(&current_tokens, train);
//         }
//         self.post_trunk_norm.forward(&current_tokens)
//     }
// }

// impl nn::ModuleT for AIMv2Transformer {
//     fn forward_t(&self, xs: &Tensor, train: bool) -> Tensor {
//         self.forward_(xs, train)
//     }
// }

// --- Main Model ---
/// The complete AIMv2 Model.
#[derive(Debug)]
pub struct AIMv2Model {
    preprocessor: AIMv2ViTPreprocessor,
    trunk: AIMv2Transformer,
}

impl AIMv2Model {
    /// Creates a new AIMv2 Model from configuration and VarStore path.
    pub fn new(vs: &nn::Path, config: &AIMv2Config) -> Self {
        let preprocessor = AIMv2ViTPreprocessor::new(&(vs / "preprocessor"), config);
        let trunk = AIMv2Transformer::new(&(vs / "trunk"), config);
        Self { preprocessor, trunk }
    }
}

impl nn::ModuleT for AIMv2Model {
    /// Forward pass for the AIMv2 Model. Takes pixel_values (NCHW).
    fn forward_t(&self, pixel_values: &Tensor, train: bool) -> Tensor {
        let tokens = self.preprocessor.forward(pixel_values);
        self.trunk.forward_t(&tokens, train)
    }
}

// --- Main Function ---
fn main() -> Result<()> {
    // Set device
    let device = auto_device();

    // --- Preprocessing (Matches Hugging Face ViTImageProcessor) ---
    let image_path = "/Users/andrewmayes/Dev/aimv2-large-patch14-native/coco_cat.jpg"; // Make sure this image exists or change the path
    let config = AIMv2Config::aimv2_large_patch14(); // Or the appropriate config

    // 1. Load Image
    let original_img = vision::image::load(image_path)
        .with_context(|| format!("Failed to load image: {}", image_path))?;
    

    // 4. Add batch dimension and move to device
    let input_tensor = original_img.unsqueeze(0).to_device(device).to_kind(Kind::Float);
    println!("Final input tensor shape: {:?}, device: {:?}", input_tensor.size(), input_tensor.device());
    

    // --- Model Loading and Inference ---
    let mut vs = nn::VarStore::new(device);
    let model = AIMv2Model::new(&vs.root(), &config);
    println!("Model structure created.");



    let weights_path = "/Users/andrewmayes/Dev/AIMv2-rs/model/model.safetensors"; // Adjust path if needed
    println!("Loading weights from: {}", weights_path);
    vs.load(weights_path)
        .with_context(|| format!("Failed to load weights from '{}'", weights_path))?;
    println!("Weights loaded successfully.");

    vs.freeze(); // Set to evaluation mode


    println!("Running Rust inference...");
    let output_rust = tch::no_grad(|| model.forward_t(&input_tensor, false)); // train = false
    println!("Rust Inference complete.");
    println!("Rust output shape: {:?}, dtype: {:?}", output_rust.size(), output_rust.kind());

    // --- Save Rust Output ---
    let rust_output_path = "rust_output.npy";
    println!("Saving Rust output tensor to {}...", rust_output_path);
    // It's often safer to move to CPU before saving to NumPy format
    output_rust.to_device(Device::Cpu).write_npy(rust_output_path)?;
    println!("Rust output saved.");

    Ok(())
}