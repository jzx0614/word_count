#include <iostream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <string>
#include <boost/asio.hpp>


namespace fs = std::filesystem;
using namespace std;
using namespace boost::asio::ip;

bool check_rctime(const fs::file_time_type rctime, const chrono::system_clock::time_point& timestamp) {
    auto sctp = chrono::time_point_cast<chrono::system_clock::duration>(rctime - fs::file_time_type::clock::now() + chrono::system_clock::now());
    return sctp > timestamp;
}

vector<string> get_modified_path_list(const fs::path& root, const chrono::system_clock::time_point& timestamp) {
    vector<string> path_list;

    for (const auto& entry : fs::directory_iterator(root)) {
        if (fs::is_regular_file(entry.status()) && check_rctime(fs::last_write_time(entry.path()), timestamp)) {
            path_list.push_back(fs::absolute(entry.path()).string());
        } else if (fs::is_directory(entry.status()) && check_rctime(fs::last_write_time(entry.path()), timestamp)) {
            auto sub_path_list = get_modified_path_list(entry.path(), timestamp);
            path_list.insert(path_list.end(), sub_path_list.begin(), sub_path_list.end());
        }
    }

    return path_list;
}

void send_file_to_server(const vector<string>& path_list) {
    const string localhost_ip = "127.0.0.1";
    const string port_str = "8888";
    boost::asio::io_context io_context;
    tcp::resolver resolver(io_context);
    tcp::resolver::results_type endpoints = resolver.resolve(localhost_ip, port_str);

    tcp::socket socket(io_context);
    boost::asio::connect(socket, endpoints);

    cout << "Send path count:" << path_list.size() << endl;

    try {
        uint32_t path_size = htonl(static_cast<uint32_t>(path_list.size()));
        boost::asio::write(socket, boost::asio::buffer(&path_size, sizeof(path_size)));

        for (const string& path : path_list) {
            boost::asio::write(socket, boost::asio::buffer(path + "\n"));
        }
    } catch (std::exception& e) {
        cerr << "Error: " << e.what() << std::endl;
    }

}

int main(int argc, char *argv[]) {
    const string directory = "./directory_big";
    if (argc != 2) {
        cerr << "Usage: ./client <timestamp>" << endl;
        return 1;
    }
    time_t timestamp_input = std::stoll(argv[1]);
    auto timestamp = chrono::system_clock::from_time_t(timestamp_input);

    auto file_list = get_modified_path_list(directory, timestamp);
    send_file_to_server(file_list);

    return 0;
}
