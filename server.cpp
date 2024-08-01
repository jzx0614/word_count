#include <boost/asio.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <queue>
#include <unordered_map>
#include <regex>

namespace fs = std::filesystem;

using namespace std;
using namespace boost::asio;

class CalculateWordCount {
public:
    CalculateWordCount& operator+=(const CalculateWordCount& rhs) {
        for (const auto& entry : rhs.word_map_) {
            word_map_[entry.first] += entry.second;
        }
        return *this;
    }
    
    bool calculate_file_path(const string& file_path) {
        ifstream file(fs::u8path(file_path));
        if (!file.is_open()) {
            return false;
        }
        string word;
        while (file >> word) {
            std::regex word_regex(R"(\b[a-zA-Z]+\b)");
            std::vector token_list(
                std::sregex_iterator(word.begin(), word.end(), word_regex),
                std::sregex_iterator()
            );
            for (const auto& item : token_list) {
                word_map_[item.str()]++;
            }
        }

        return true;
    }

    void print() {
        for (const auto& entry : word_map_) {
            cout << entry.first << " " << entry.second << endl;
        }
    }

private:
    unordered_map<string, int> word_map_;
};

class ParelledlWordCount {
public:
    ParelledlWordCount(int num_thread) : num_thread_(num_thread) {
    }

    void work(int thread_idx, std::shared_ptr<CalculateWordCount> word_count) {

        while (true) {
            string file_path;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [&] { return !task_list_.empty() || done_; });
                if (task_list_.empty()) {
                    // cout << "thread:" << thread_idx << " finish------\n";
                    return;
                }

                file_path = task_list_.front();
                task_list_.pop();
                
            }
            
            word_count->calculate_file_path(file_path);
        }
    }

    std::shared_ptr<CalculateWordCount> calculate_word_count(std::vector<string> path_list) {
        for (const auto& path : path_list) {
            task_list_.push(path);
        }
        
        std::vector<std::thread> thread_list;
        std::vector<std::shared_ptr<CalculateWordCount>> word_count_list;
        for (auto i = 0; i < num_thread_; ++i) {
            auto word_count = std::make_shared<CalculateWordCount>();
            word_count_list.push_back(word_count);
            thread_list.emplace_back([=]() { work(i, word_count); });
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            done_ = true;
        }
        cv_.notify_all();

        for (auto &thread : thread_list) {
            thread.join();
        }

        auto total_word_count = std::make_shared<CalculateWordCount>();
        for (auto word_count : word_count_list) {
            *total_word_count += *word_count;
        }
        return total_word_count;
    }

private:
    int num_thread_ = 1;
    
    std::condition_variable cv_;
    std::mutex mutex_;
    std::queue<std::string> task_list_;
    bool done_ = false;
};

std::vector<string> handle_client(ip::tcp::socket socket) {
    vector<string> path_list;
    try {
        uint32_t array_size = 0;
        boost::asio::read(socket, boost::asio::buffer(&array_size, sizeof(array_size)));
        std::size_t path_size = ntohl(array_size);

        // cout << "Recv vector:" << path_size << endl;
        
        while(path_list.size() < path_size) {
            boost::asio::streambuf buf;
            boost::system::error_code err_code;
            boost::asio::read(socket, buf, err_code);

            istream input(&buf);
            string file_path;
            while(getline(input, file_path)) {
                path_list.emplace_back(file_path);
                // cout << path_list.size() << " " << file_path << endl;
            }

        }
    } catch (std::exception& e) {
        cerr << "Error: " << e.what() << std::endl;
    }
    return path_list;
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "Usage: ./server <num_thread>" << endl;
        return 1;
    }
    const int num_thread = std::atoi(argv[1]);

    try {
        io_context io_context;
        ip::tcp::acceptor acceptor(io_context, ip::tcp::endpoint(ip::tcp::v4(), 8888));

        while (true) {
            ip::tcp::socket socket(io_context);
            acceptor.accept(socket);
            auto path_list = handle_client(std::move(socket));

            ParelledlWordCount word_count(num_thread);
            auto total_word_count = word_count.calculate_word_count(path_list);
            total_word_count->print();
        }
    } catch (std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
    }

    return 0;
}