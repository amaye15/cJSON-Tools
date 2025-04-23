// Create a module namespace for lib
pub mod lib {
    // Include the modules inside the lib module
    pub mod embed;
    pub mod norm;
    pub mod utils;
    pub mod ffn;
    pub mod config;
    pub mod patch;
    pub mod preprocessor;
    pub mod attention;
    pub mod block;
    pub mod transformer;
}

// Optional: Re-export if you want to use them directly without the lib:: prefix
pub use lib::embed;
pub use lib::norm;
pub use lib::utils;
pub use lib::ffn;
pub use lib::config;
pub use lib::patch;
pub use lib::preprocessor;
pub use lib::attention;
pub use lib::block;
pub use lib::transformer;