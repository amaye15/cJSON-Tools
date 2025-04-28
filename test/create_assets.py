import itertools
import numpy as np
import os # Import the os module

# --- Constants ---
MIN = 2
MAX = 2048
COMBO = 2 # We specifically want pairs (width, height)
CHANNELS = 3 # Number of channels for the arrays
DIR = "test/assets" # Output directory

# --- Ensure Output Directory Exists ---
# Create the directory if it doesn't exist, including parent directories if needed
# exist_ok=True prevents an error if the directory already exists
os.makedirs(DIR, exist_ok=True)
print(f"Output directory: {DIR}")

# --- Generate Powers of Two ---
powers_of_two = []
current_power = 1
while current_power < MIN:
    current_power *= 2
while current_power <= MAX:
    powers_of_two.append(current_power)
    current_power *= 2

print(f"\nGenerated Powers of Two: {powers_of_two}")

# --- Generate Combinations (Pairs) ---
# Using COMBO=2 ensures we get pairs
all_combinations = list(itertools.combinations(powers_of_two, COMBO))
all_combinations.extend([(val2, val1) for val1, val2 in all_combinations])
print(f"\nGenerated {len(all_combinations)} combinations of size {COMBO}.")

# --- Generate and Save Arrays ---
print(f"\nGenerating and saving {len(all_combinations)} numpy arrays...")

for idx, (width, height) in enumerate(all_combinations):
    # Define the shape of the array: (channels, width, height)
    # Note: Often image dimensions are (height, width, channels) or
    # (channels, height, width). We'll use (channels, width, height) as implied.
    shape = (CHANNELS, width, height)

    print(shape)

    # Generate random integer data
    # Example: integers between 0 and 255 (inclusive), common for image data (uint8)
    # np.random.randint uses exclusive high value, so use 256 for 0-255 range
    random_array = np.random.randint(0, 256, size=shape, dtype=np.uint8)

    # Construct the filename and full path safely
    filename = f"test_{idx + 1}.npy"
    # Use os.path.join to combine directory and filename correctly
    filepath = os.path.join(DIR, filename)

    # Save the numpy array to the file
    np.save(filepath, random_array)

    # Optional: Print progress feedback
    # print(f"Saved: {filepath} with shape {shape}") # Uncomment to see each file saved

print(f"\nFinished saving {len(all_combinations)} arrays to {DIR}")
