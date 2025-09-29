//
//  parallel_level_client.cpp
//  sequential
//
//  Created by Erik Villanueva Gomez on 9/23/25.
//

#include <iostream>
#include <string>
#include <vector>
#include <unordered_set>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <mutex>
#include <algorithm>
#include <curl/curl.h>
#include "rapidjson/error/error.h"
#include "rapidjson/reader.h"
#include <rapidjson/document.h>
#define RAPIDJSON_PARSE_ERROR_NORETURN(code, offset) \ throw ParseException(code, #code, offset)



struct ParseException : std::runtime_error, rapidjson::ParseResult{
    ParseException(rapidjson::ParseErrorCode code, const char* msg, size_t offset) :
        std::runtime_error(msg),
        rapidjson::ParseResult(code, offset){}
};

bool debug = false;

const std::string SERVICE_URL = "http://hollywood-graph-crawler.bridgesuncc.org/neighbors/";

std::string url_encode(CURL* curl, const std::string& input){
    char* out = curl_easy_escape(curl, input.c_str(), input.size());
    std::string s = out;
    curl_free(out);
    return s;
}

size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* output){
    size_t totalSize = size * nmemb;
    output->append((char*)contents, totalSize);
    return totalSize;
}

std::string fetch_neighbors(CURL* curl, const std::string& node){
    std::string url = SERVICE_URL + url_encode(curl, node);
    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "User-Agent: C++-Client/1.0");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    
    if(res != CURLE_OK){
        std::cerr << "CURL error: " << curl_easy_strerror(res) << std::endl;
        return "{}";
    }
    return response;
}
    
std::vector<std::string> get_neighbors(const std::string& json_str){
    std::vector<std::string> neighbors;
    try{
        rapidjson::Document doc;
        doc.Parse(json_str.c_str());
        
        if(doc.HasMember("neighbors") && doc["neighbors"].IsArray()){
            for(const auto& neighbor : doc["neighbors"].GetArray()){
                neighbors.push_back(neighbor.GetString());
            }
        }
    } catch(const ParseException& e){
        std::cerr << "Error while parsing JSON: " << json_str << std::endl;
        throw;
    }
    return neighbors;
}

void expand_nodes_worker(std::vector<std::string>::const_iterator start_it,
                         std::vector<std::string>::const_iterator end_it,
                         std::unordered_set<std::string>& visited,
                         std::vector<std::string>& next_level,
                         std::mutex& mtx){
    CURL* curl = curl_easy_init();
    if(!curl){
        std::cerr << "Failed to initialize CURL in thread\n";
        return;
    }
    
    for(auto it = start_it; it != end_it; ++it){
        const std::string& node = *it;
        try{
            std::string json_response = fetch_neighbors(curl, node);
            std::vector<std::string> neighbors = get_neighbors(json_response);
            
            std::lock_guard<std::mutex> lock(mtx);
            for(const auto& neighbor : neighbors){
                if(visited.find(neighbor) == visited.end()){
                    visited.insert(neighbor);
                    next_level.push_back(neighbor);
                }
            }
        } catch(const ParseException& e){
            std::cerr << "Error processing node: " << node << std::endl;
        }
    }
    curl_easy_cleanup(curl);
}

std::vector<std::vector<std::string>> bfs_parallel(const std::string& start, int depth, int max_threads){
    std::vector<std::vector<std::string>> levels;
    std::unordered_set<std::string> visited;
    std::mutex mtx;
    
    levels.push_back({start});
    visited.insert(start);
    
    for(int d = 0; d <depth; ++d){
        const auto& current_level_nodes = levels[d];
        if(current_level_nodes.empty()) break;
        
        std::vector<std::string> next_level_nodes;
        std::vector<std::thread> threads;
        
        int num_threads = std::min((int) current_level_nodes.size(), max_threads);
        
        int base_chunk_size = current_level_nodes.size() / num_threads;
        int remainder = current_level_nodes.size() % num_threads;
        auto it = current_level_nodes.cbegin();
        
        for(int i = 0; i < num_threads; ++i){
            int chunk_size = base_chunk_size + (i < remainder ? 1 : 0);
            auto end_it = std::next(it, chunk_size);
            
            threads.emplace_back(expand_nodes_worker,
                                 it,
                                 end_it,
                                 std::ref(visited),
                                 std::ref(next_level_nodes),
                                 std::ref(mtx));
            it = end_it;
        }
        
        for(auto& t : threads){
            t.join();
        }
        
        if(next_level_nodes.empty()) break;
        levels.push_back(next_level_nodes);
        
    }
    return levels;
}

int main(int argc, char* argv[]){
    if(argc < 3 || argc > 4){
        std::cerr << "Usage: " << argv[0] << " <node_name> <depth> [max_threads]\n";
        return 1;
    }
    
    std::string start_node = argv[1];
    int depth;
    int max_threads = 8;
    
    try{
        depth = std::stoi(argv[2]);
        if(argc == 4){
            max_threads = std::stoi(argv[3]);
        }
    } catch(const std::exception& e){
        std::cerr << "Error: Depth and max_threads must be integers.\n";
        return 1;
    }
    
    curl_global_init(CURL_GLOBAL_ALL);
    
    const auto start_time = std::chrono::steady_clock::now();
    auto all_levels = bfs_parallel(start_node, depth, max_threads);
    
    for(size_t i = 0; i < all_levels.size(); ++i){
        std::cout << "--- Level " << i << " (" << all_levels[i].size() << " nodes) ---\n";
        for(size_t j =0; j < all_levels[i].size() && j < 10; ++j){
            std::cout << "- " << all_levels[i][j] << "\n";
        }
        if(all_levels[i].size() > 10){
            std::cout << "- ... and " << all_levels[i].size() - 10 << " more.\n";
        }
    }
    
    const auto end_time = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed_seconds = end_time - start_time;
    
    size_t total_nodes = 0;
    for(const auto& level : all_levels){
        total_nodes += level.size();
    }
    
    std::cout << "\n----------------------------------------\n";
    std::cout << "Total nodes visited: " << total_nodes << "\n";
    std::cout << "Time to crawl: " << elapsed_seconds.count() << "s\n";
    
    curl_global_cleanup();
    
    return 0;
}
