/*************************************************************************
readFile - reads or writes files (images, data, models) for the neural network
                             -------------------
    copyright            : (C) 2024 by Enzo DOS ANJOS
*************************************************************************/

//---------- Implementation of the module <readFile> (file readFile.cpp) ---------------

/////////////////////////////////////////////////////////////////  INCLUDE
//--------------------------------------------------------- System Include
#include <iostream>
using namespace std;

//------------------------------------------------------- Personal Include
#include "readFile.h"
#include "NN.h"

// readImg
#define STB_IMAGE_IMPLEMENTATION
#include "utilities/stb_image.h"

// readData
#include <fstream>
#include <sstream>

/////////////////////////////////////////////////////////////////  PRIVATE
//-------------------------------------------------------------- Constants

//------------------------------------------------------------------ Types

//------------------------------------------------------- Static Variables

//------------------------------------------------------ Private Functions

//////////////////////////////////////////////////////////////////  PUBLIC
//------------------------------------------------------- Public Functions
bool readImg (const string filename, float *&pixArr, int &width, int &height, int &channels)
{   
    // Load the image file
    unsigned char *temp = nullptr;
    temp = stbi_load(filename.c_str(), &width, &height, &channels, 0);

    if (temp == nullptr) {
        cerr << "Error: Could not load the image file" << endl;
        return false;
    }

    // copy the pixel values to the array
    pixArr = new float[width * height * channels];
    for (int i = 0; i < width * height * channels; i++) {
        pixArr[i] = static_cast<float>(temp[i]);
    }

    // Free the image memory when done
    stbi_image_free(temp);

    return true;

} //----- end of readImg


bool Normalize(float *&pixArr, int width, int height, int newWidth, int newHeight, int nb_channels) {
    // Convert to grayscale if needed
    if (nb_channels == 3 || nb_channels == 4) {
        cout << "Converting to grayscale" << endl;
        float *greyPixArr = new float[width * height];
        for (int i = 0; i < width * height; i++) {
            // Use only the RGB channels (ignore the alpha channel for pngs)
            greyPixArr[i] = 0.299f * pixArr[i * nb_channels] +
                            0.587f * pixArr[i * nb_channels + 1] +
                            0.114f * pixArr[i * nb_channels + 2];
        }
        delete[] pixArr;
        pixArr = greyPixArr;
    }

    // Resize the image if needed
    if (width != newWidth || height != newHeight) {
        cout << "Resizing image" << endl;
        float *resizedPixArr = new float[newWidth * newHeight];
        float x_ratio = static_cast<float>(width) / newWidth;
        float y_ratio = static_cast<float>(height) / newHeight;
        float px, py;
        for (int i = 0; i < newHeight; i++) {
            for (int j = 0; j < newWidth; j++) {
                px = floor(j * x_ratio);
                py = floor(i * y_ratio);
                resizedPixArr[(i * newWidth) + j] = pixArr[(static_cast<int>(py) * width) + static_cast<int>(px)];
            }
        }
        delete[] pixArr;
        pixArr = resizedPixArr;
    }

    // Normalize the pixel values (divide by 255)
    int total_pixels = newWidth * newHeight;
    float total_intensity = 0.0f;

    for (int i = 0; i < total_pixels; i++) {
        pixArr[i] = pixArr[i] / 255.0f;
        total_intensity += pixArr[i];
    }

    // Calculate mean intensity to determine if the background is white
    float mean_intensity = total_intensity / total_pixels;

    if (mean_intensity > 0.5f) { // Invert if most pixels are white
        for (int i = 0; i < total_pixels; i++) {
            pixArr[i] = 1.0f - pixArr[i];
        }
    }

    return true;
} //----- end of Normalize


bool readData (string csv_path, Data *data, int batch_size) {
    // Input CSV file
    ifstream csvFile(csv_path);
    if (!csvFile) {
        cerr << "Failed to open the CSV file." << endl;
        return false;
    }

    string line;

    getline(csvFile, line); // Skip the header row

    // Read data into the array
    int index = 0;
    while (getline(csvFile, line) && index < batch_size) {
        istringstream lineStream(line);
        string path, label_str;

        // Parse the CSV line
        if (getline(lineStream, path, ',') && getline(lineStream, label_str, ',')) {
            // Remove quotes if present
            if (!path.empty() && path.front() == '"' && path.back() == '"') {
                path = path.substr(1, path.size() - 2);
            }
            if (!label_str.empty() && label_str.front() == '"' && label_str.back() == '"') {
                label_str = stoi(label_str.substr(1, label_str.size() - 2));
            }

            int label = stoi(label_str);  //convert the label to an integer

            // Add the data to the array
            data[index++] = {path, label};
        }
    }

    csvFile.close();
    return true;
} //----- end of readData


void saveModel (const string filename, vector<Layer> &NN) {
    ofstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: Unable to open file for saving model." << endl;
        return;
    }

    // Save the number of layers
    int num_layers = NN.size();
    file.write(reinterpret_cast<char*>(&num_layers), sizeof(int));

    // Save each layer
    for (int i = 0; i < num_layers; i++) {
        Layer layer = NN[i];

        // Save the layer type
        int type_length = layer.type.length();
        file.write(reinterpret_cast<char*>(&type_length), sizeof(int));
        file.write(layer.type.c_str(), type_length);

        // Save the layer input and output sizes
        file.write(reinterpret_cast<char*>(&layer.input_size), sizeof(int));
        file.write(reinterpret_cast<char*>(&layer.output_size), sizeof(int));

        // Save the activation function parameters
        int num_params = layer.activation_func_params.size();
        file.write(reinterpret_cast<char*>(&num_params), sizeof(int));
        if (num_params > 0) {
            file.write(reinterpret_cast<char*>(layer.activation_func_params.data()), num_params * sizeof(float));
        }

        // Save the layer weights and biases
        file.write(reinterpret_cast<char*>(layer.weights), layer.input_size * layer.output_size * sizeof(float));
        file.write(reinterpret_cast<char*>(layer.biases), layer.output_size * sizeof(float));
    }

    file.close();
    cout << "Model saved to " << filename << endl;
} //----- end of saveModel


void loadModel(const string filename, vector<Layer> &NN) {
    ifstream file(filename, ios::binary);
    if (!file) {
        cerr << "Error: Unable to open file for loading model." << endl;
        return;
    }

    // destroy the current model if needed
    if (NN.size() > 0) {
        destroyNN(NN);
    }

    // Load the number of layers
    int num_layers;
    file.read(reinterpret_cast<char*>(&num_layers), sizeof(int));

    // Load each layer
    for (int i = 0; i < num_layers; i++) {
        // Load the type
        int type_length;
        file.read(reinterpret_cast<char*>(&type_length), sizeof(int));

        char *type = new char[type_length + 1];
        file.read(type, type_length);
        type[type_length] = '\0';  // Null-terminate the string
        string layer_type = type;
        delete[] type;

        // Load the input and output sizes
        int input_size, output_size;
        file.read(reinterpret_cast<char*>(&input_size), sizeof(int));
        file.read(reinterpret_cast<char*>(&output_size), sizeof(int));

        // Load the activation function parameters
        int num_params;
        file.read(reinterpret_cast<char*>(&num_params), sizeof(int));
        vector<float> activation_func_params(num_params);
        if (num_params > 0) {
            file.read(reinterpret_cast<char*>(activation_func_params.data()), num_params * sizeof(float));
        }

        // Add the layer to the model
        addLayer(NN, layer_type, output_size, input_size, activation_func_params);

        // Load the weights and biases
        file.read(reinterpret_cast<char*>(NN.back().weights), input_size * output_size * sizeof(float));
        file.read(reinterpret_cast<char*>(NN.back().biases), output_size * sizeof(float));
    }

    file.close();
    cout << "Model loaded from " << filename << endl;
} //----- end of loadModel