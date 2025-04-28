
use anyhow::{Context, Result};
use tch::{Kind, Tensor};





#[test]
fn load_tensor()-> Result<()> {
    let npy_file_path = std::path::Path::new("/Users/andrewmayes/Dev/AIMv2-rs/test/assets/test_1.npy")
    .canonicalize() // Get absolute path
    .context("Failed to find test_tensor.npy. Did you run create_npy.py?")?;
    let npy_file_path_str = npy_file_path
        .to_str()
        .context("Failed to convert path to string")?;
    let rust_tensor = Tensor::read_npy(&npy_file_path)
    .context("Failed to load tensor using tch::Tensor::read_npy")?;

    println!("{:?}", npy_file_path_str);


    Ok(())

}