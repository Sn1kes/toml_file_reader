//#include <QCoreApplication>
#include <iostream>
#include <string>
#include <cstdio>
#include <include\toml.hpp>

template<typename T>
T toml_find(const toml::basic_value<toml::discard_comments, std::unordered_map, std::vector>& table, const char *val)
{
    try {
        return toml::find<T>(table, val);
    } catch (const std::out_of_range& e) {
        std::cerr << "Value \"" << val << "\" not found in config:\n" << e.what();
        exit(1);
    } catch (const toml::type_error& e) {
        std::cerr << "Invalid type of value \"" << val << "\"\n" << e.what();
        exit(1);
    }
}

std::unique_ptr<std::FILE, decltype(&std::fclose)> file_open(const char *filename, const char *mode)
{
    std::unique_ptr<std::FILE, decltype(&std::fclose)> file {std::fopen(filename, mode), &std::fclose};
    if(!file.get()) {
        std::cerr << "Cannot open file \"" << filename << "\"\n";
        exit(1);
    }
    return file;
}

int main(int argc, char *argv[])
{
    std::string path{};
    std::string config{};
    if(argc > 1) {
        if(argc != 3) {
            std::cerr << "Invalid number of arguments!\n";
            return 1;
        }
        const std::string arg_1 {argv[1]};
        if(!(arg_1 == std::string{"-config"})) {
            std::cerr << "Invalid argument!\n";
            return 1;
        }
        std::string arg_2 {argv[2]};
        if(arg_2[arg_2.length() - 1] != '\\') {
            arg_2 += "\\";
        }
        path = arg_2;
        config = arg_2 + "config.toml";
    } else {
        config = "config.toml";
    }
    toml::value data;
    try {
        data = toml::parse(config);
    } catch (std::runtime_error& e) {
        std::cerr << "Error opening toml config!\n" << e.what();
        return 1;
    } catch (toml::syntax_error& e) {
        std::cerr << "Syntax error in toml config:\n" << e.what();
        return 1;
    }
    const auto& table = toml_find<toml::basic_value<toml::discard_comments, std::unordered_map, std::vector>>(data, "params");
    const auto& in = toml_find<std::vector<std::string>>(table, "input");
    const auto& out = toml_find<std::vector<std::string>>(table, "output");
    const auto chunk_size = toml_find<uint64_t>(table, "chunk_size");
    std::unique_ptr<char[]> buf;
    try {
        buf = std::unique_ptr<char[]>{new char[chunk_size]};
    }
    catch (const std::bad_alloc& e) {
        std::cerr << "Error while allocating chunk buffer(probably chunk size is too big):\n" << e.what();
        return 1;
    }
    if(in.size() != out.size()) {
        std::cerr << "Error: sizes of input and ouput arrays mimatch\n";
        return 1;
    }
    for(uint64_t i = 0; i < in.size(); ++i) {
        const std::string& in_name = path + in[i];
        const std::string& out_name = path + out[i];
        std::cout << "Copying from \"" << in_name << "\" to \"" << out_name << "\"\n";
        const std::unique_ptr<std::FILE, decltype(&std::fclose)> in_file = file_open(in_name.c_str(), "rb");
        const std::unique_ptr<std::FILE, decltype(&std::fclose)> out_file = file_open(out_name.c_str(), "w+b");
        do {
            const size_t num_read = std::fread(buf.get(), 1, chunk_size, in_file.get());
            if(std::ferror(in_file.get())) {
                std::cerr << "Reading error occured\n";
                return 1;
            }
            std::fwrite(buf.get(), 1, num_read, out_file.get());
            if(std::ferror(in_file.get())) {
                std::cerr << "Writing error occured\n";
                return 1;
            }
        } while(!std::feof(in_file.get()));
    }

    //QCoreApplication a(argc, argv);
    //return a.exec();
    return 0;
}
