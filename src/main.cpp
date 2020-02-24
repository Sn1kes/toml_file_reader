#include <iostream>
#include <string>
#include <cstdio>
#include <thread>
#include <future>
#include <chrono>

#include <QCoreApplication>
#include <QTimer>
#include "include\app.hpp"

//Thread execution function
void thread_process(const std::string&& in_name, const std::string&& out_name,
    const std::size_t speed, std::promise<uint8_t>&& res)
{
    size_t chunk_size = 1;
    constexpr size_t max_size_half = 0x8000; //64 KiB max buffer size
    std::vector<unsigned char> buf;
    try {
        buf.resize(chunk_size);
    }
    catch(...) {
        res.set_exception(std::current_exception());
        return;
    }
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> in_file {std::fopen(in_name.c_str(), "rb"), std::fclose};
    if(!in_file.get()) {
        res.set_value(1);
        return;
    }
    const std::unique_ptr<std::FILE, decltype(&std::fclose)> out_file {std::fopen(out_name.c_str(), "w+b"), std::fclose};
    if(!out_file.get()) {
        res.set_value(2);
        return;
    }
    do {
        const auto t1 = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now());
        const size_t num_read = std::fread(buf.data(), 1, chunk_size, in_file.get());
        const auto t2 = std::chrono::time_point_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now());
        const auto req_dur = std::chrono::nanoseconds(static_cast<uint64_t>((static_cast<float>(chunk_size) * 1000000000.f / static_cast<float>(speed))));
        const auto t3 = t1 + req_dur;
        std::this_thread::sleep_until(t3);
        if(std::ferror(in_file.get())) {
            res.set_value(3);
            return;
        }
        std::fwrite(buf.data(), 1, num_read, out_file.get());
        if(std::ferror(in_file.get())) {
            res.set_value(4);
            return;
        }
        try {
            if((t2 < t3) && (chunk_size <= max_size_half)) {
                chunk_size <<= 1;
                buf.resize(chunk_size);
            } else if((t2 > t3) && chunk_size > 1) {
                chunk_size >>= 1;
                buf.resize(chunk_size);
            }
        } catch(...) {}
    } while(!std::feof(in_file.get()));
    res.set_value(0);
}

App::App(QObject *parent) :
    QObject(parent)
{
    qapp = QCoreApplication::instance();
}

void App::run()
{
    std::string path{};
    std::string config{};
    QStringList args = qapp->arguments();
    if(args.size() > 1) {
        if(args.size() != 3) {
            std::cerr << "Invalid number of arguments!\n";
            qapp->exit(EXIT_FAILURE);
            return;
        }
        const std::string arg_1 {args.at(1).toStdString()};
        if(!(arg_1 == std::string{"-config"})) {
            std::cerr << "Invalid argument!\n";
            qapp->exit(EXIT_FAILURE);
        }
        std::string arg_2 {args.at(2).toStdString()};
        if(arg_2[arg_2.length() - 1] != '\\' && arg_2[arg_2.length() - 1] != '/') {
            arg_2 += "/";
        }
        path = arg_2;
        config = arg_2 + "config.toml";
    } else {
        config = "config.toml";
    }
    toml::value data;
    try {
        data = toml::parse(config);
    } catch(std::runtime_error& e) {
        std::cerr << "Error opening toml config!\n" << e.what();
        qapp->exit(EXIT_FAILURE);
    } catch(toml::syntax_error& e) {
        std::cerr << "Syntax error in toml config:\n" << e.what();
        qapp->exit(EXIT_FAILURE);
    }
    const auto& table = toml_find<toml::basic_value<toml::discard_comments, std::unordered_map, std::vector>>(data, "params");
    const auto& in = toml_find<std::vector<std::string>>(table, "input");
    const auto& out = toml_find<std::vector<std::string>>(table, "output");
    const auto speed = toml_find<std::size_t>(table, "speed");
    const std::size_t files_num = in.size();
    if(files_num != out.size()) {
        std::cerr << "Error: sizes of input and ouput arrays mimatch\n";
        qapp->exit(EXIT_FAILURE);
    }
    std::vector<std::thread> threads_arr;
    std::vector<std::future<uint8_t>> res_arr;
    try {
        threads_arr.reserve(files_num);
        threads_arr.resize(files_num);
        res_arr.reserve(files_num);
        res_arr.resize(files_num);
    }
    catch(const std::length_error& e) {
        std::cerr << "Error: number of files is too big:\n" << e.what();
        qapp->exit(EXIT_FAILURE);
    }
    catch(const std::bad_alloc& e) {
        std::cerr << "Error while allocating buffers(probably too many files has been specified):\n" << e.what();
        qapp->exit(EXIT_FAILURE);
    }
    for(std::size_t i = 0; i < files_num; ++i) {
        const std::string in_name = path + in[i];
        const std::string out_name = path + out[i];
        std::promise<uint8_t> pr;
        res_arr[i] = pr.get_future();
        threads_arr[i] = std::thread(thread_process, std::move(in_name), std::move(out_name), speed, std::move(pr));
    }
    for(std::size_t i = 0; i < files_num; ++i) {
        threads_arr[i].join();
    }
    int ret_val = EXIT_SUCCESS;
    for(std::size_t i = 0; i < files_num; ++i) {
        uint8_t res;
        try {
            res = res_arr[i].get();
        }
        catch(const std::length_error& e) {
            std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                "\"\n\tLength of chunk buffer is too big:\n" << e.what();
            ret_val = EXIT_FAILURE;
            continue;
        }
        catch(const std::bad_alloc& e) {
            std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                "\"\n\tAllocating chunk buffer raised exception:\n" << e.what();
            ret_val = EXIT_FAILURE;
            continue;
        }
        switch (res) {
            case 1: {
                std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                    "\"\n\tCannot open file \"" << in[i] << "\"\n";
                ret_val = EXIT_FAILURE;
            }
            break;
            case 2: {
                std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                    "\"\n\tCannot open file \"" << out[i] << "\"\n";
                ret_val = EXIT_FAILURE;
            }
            break;
            case 3: {
                std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                    "\"\n\tReading error occured\n";
                ret_val = EXIT_FAILURE;
            }
            break;
            case 4: {
                std::cerr << "Failure copying from \"" << in[i] << "\" to \"" << out[i] <<
                    "\"\n\tWriting error occured\n";
                ret_val = EXIT_FAILURE;
            }
            break;
            default: {
                std::cout << "Sucess copying from \"" << in[i] << "\" to \"" << out[i] << "\"\n";
            }
            break;
        }
    }
    qapp->exit(ret_val);
}

//Program takes "-config" parameter with path to "config.toml" file.
//If no parameter was specified, root path is taken
int main(int argc, char *argv[])
{
    QCoreApplication qapp(argc, argv);
    App app;

    QTimer::singleShot(10, &app, SLOT(run()));
    return qapp.exec();
}
