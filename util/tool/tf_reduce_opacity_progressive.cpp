#include <CLI/App.hpp>
#include <CLI/CLI.hpp>
#include <TFEditor.h>

int main(int argc, char **argv) {
    float reduce_by = .0f;
    std::string tf_path;

    CLI::App app{"Transfer Function Progressive Opacity Reducer: Reduces the opacity of the transfer function step by "
                 "step, until the total amount reduced goes over 1. Also applies smoothing to the given transfer "
                 "function, and saves the TF at each step as a separate file."};
    app.add_option("--reduce_by", reduce_by, "The amount to reduce at each step, between 0 and 1")->required();
    app.add_option("tf_path", tf_path, "The file path to the transfer function")->required();

    CLI11_PARSE(app, argc, argv);

    std::cout << "reduce_by: " << reduce_by << std::endl;
    std::cout << "tf_path: " << tf_path << std::endl;

    hs::TFEditor tf;
    tf.saveTFSteps(tf_path.c_str(), reduce_by);

    return 0;
}