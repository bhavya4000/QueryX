#define _WIN32_WINNT 0x0600

#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <map>
#include <cmath>
#include <algorithm>
#include <chrono>  // For timing
#include "E:\\webscraper\\include\\json.hpp"
#include <cctype>

#pragma comment(lib, "Ws2_32.lib")

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

vector<string> tokenize_and_normalize(const string& text) {
    vector<string> tokens;
    string token;
    for (char ch : text) {
        if (isalnum(ch)) { // keep letters and digits
            token += tolower(ch);
        } else {
            if (!token.empty()) {
                tokens.push_back(token);
                token.clear();
            }
        }
    }
    if (!token.empty())
        tokens.push_back(token);
    return tokens;
}


struct Link {
    string url;
    double score; // Heuristic score
};

double calculate_tf(int term_count, int total_terms) {
    return static_cast<double>(term_count) / total_terms;
}

double calculate_idf(int doc_count, int total_docs) {
    return log(static_cast<double>(total_docs) / (1 + doc_count));
}

map<string, double> calculate_tf_idf(
    const unordered_map<string, int>& wordFrequencyMap,
    const unordered_map<string, int>& documentFrequencyMap,
    int totalDocs) {

    map<string, double> tfIdfScores;

    for (const auto& pair : wordFrequencyMap) {
        double tf = calculate_tf(pair.second, wordFrequencyMap.size());
        double idf = calculate_idf(documentFrequencyMap.at(pair.first), totalDocs);
        tfIdfScores[pair.first] = tf * idf;
    }

    return tfIdfScores;
}

void store_in_map(const string& content, unordered_map<string, int>& wordFrequencyMap) {
    istringstream stream(content);
    string word;
    while (stream >> word) {
        ++wordFrequencyMap[word];
    }
}

// New function: Retrieve documents containing query terms (without ranking)
vector<json> retrieve_documents(const json& data, const string& query) {
    vector<json> results;

    vector<string> queryTerms = tokenize_and_normalize(query);
    unordered_map<string, bool> queryTermMap;
    for (const auto& term : queryTerms) queryTermMap[term] = true;

    for (const auto& item : data["items"]) {
        string snippet = item["snippet"];
        vector<string> snippetWords = tokenize_and_normalize(snippet);

        bool containsTerm = false;
        for (const auto& word : snippetWords) {
            if (queryTermMap.find(word) != queryTermMap.end()) {
                containsTerm = true;
                break;
            }
        }

        if (containsTerm) {
            results.push_back(item);
        }
    }
    return results;
}

// Modified rank_links to accept only the filtered subset
vector<Link> rank_links(const vector<json>& filteredItems, const string& query) {
    unordered_map<string, int> documentFrequencyMap;
    int totalDocs = filteredItems.size();
    vector<Link> rankedLinks;

    // Populate document frequency map
    for (const auto& item : filteredItems) {
        string content = item["snippet"];
        unordered_map<string, int> wordFrequencyMap;
        store_in_map(content, wordFrequencyMap);
        for (const auto& term : wordFrequencyMap) {
            documentFrequencyMap[term.first]++;
        }
    }

    for (const auto& item : filteredItems) {
        string url = item["link"];
        string snippet = item["snippet"];
        double score = 0;

        unordered_map<string, int> wordFrequencyMap;
        store_in_map(snippet, wordFrequencyMap);

        for (const auto& term : wordFrequencyMap) {
            if (query.find(term.first) != string::npos) {
                score += calculate_tf(wordFrequencyMap[term.first], wordFrequencyMap.size());
                score *= calculate_idf(documentFrequencyMap[term.first], totalDocs);
            }
        }

        rankedLinks.push_back({url, score});
    }

    sort(rankedLinks.begin(), rankedLinks.end(),
         [](const Link& a, const Link& b) { return a.score > b.score; });

    return rankedLinks;
}


// Modified fetch_and_save_data with timing and toggle
void fetch_and_save_data(const string& query, const string& output_file, bool useHashMap) {
    using namespace std::chrono;
    auto startProcessing = high_resolution_clock::now(); // Start processing timer

    SOCKET sock;
    struct sockaddr_in server;
    char buffer[4096];

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        cerr << "Could not create socket: " << WSAGetLastError() << std::endl;
        return;
    }

    hostent* host = gethostbyname("localhost");
    if (host == nullptr) {
        cerr << "Could not resolve hostname: " << WSAGetLastError() << std::endl;
        closesocket(sock);
        return;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(3000);
    memcpy(&server.sin_addr, host->h_addr, host->h_length);

    if (connect(sock, (struct sockaddr*)&server, sizeof(server)) < 0) {
        cerr << "Connection failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        return;
    }

    string path = "/search?q=" + query;
    string request = "GET " + path + " HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";

    if (send(sock, request.c_str(), request.size(), 0) == SOCKET_ERROR) {
        cerr << "Send failed: " << WSAGetLastError() << endl;
        closesocket(sock);
        return;
    }

    // Start timing data retrieval
    auto startRetrieval = high_resolution_clock::now();

    string response;
    int received;
    while ((received = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[received] = '\0';
        response += buffer;
    }

    auto endRetrieval = high_resolution_clock::now();
    auto dataRetrievalTime = duration_cast<milliseconds>(endRetrieval - startRetrieval).count();

    closesocket(sock);

    size_t json_start = response.find("\r\n\r\n");
    if (json_start != string::npos) {
        string json_data = response.substr(json_start + 4);

        try {
            json parsedJson = json::parse(json_data);
            ofstream ofs(output_file);
            if (ofs) ofs << parsedJson.dump(4);

            // Validate that 'items' exists and is an array
            if (!parsedJson.contains("items") || !parsedJson["items"].is_array()) {
                cerr << "Error: 'items' not found or is not an array in the JSON response.\n";
                return;
            }

            auto rankStart = high_resolution_clock::now();

            vector<Link> rankedLinks;
            if (useHashMap) {
                cout << "\n[Using Hash Map for Ranking]\n";
                vector<json> filteredDocs = retrieve_documents(parsedJson, query);
                rankedLinks = rank_links(filteredDocs, query);
            } else {
                cout << "\n[Using Linear Structure for Ranking]\n";

                unordered_map<string, int> documentFrequencyMap;
                int totalDocs = parsedJson["items"].size();

                for (const auto& item : parsedJson["items"]) {
                    string content = item["snippet"];
                    unordered_map<string, int> freq;
                    store_in_map(content, freq);
                    for (const auto& term : freq)
                        documentFrequencyMap[term.first]++;
                }

                for (const auto& item : parsedJson["items"]) {
                    string url = item["link"];
                    string snippet = item["snippet"];
                    double score = 0;

                    unordered_map<string, int> freq;
                    store_in_map(snippet, freq);

                    for (const auto& term : freq) {
                        if (query.find(term.first) != string::npos) {
                            score += calculate_tf(term.second, freq.size());
                            score *= calculate_idf(documentFrequencyMap[term.first], totalDocs);
                        }
                    }

                    rankedLinks.push_back({url, score});
                }

                sort(rankedLinks.begin(), rankedLinks.end(),
                     [](const Link& a, const Link& b) { return a.score > b.score; });
            }

            auto rankEnd = high_resolution_clock::now();
            auto totalProcessingTime = duration_cast<milliseconds>(rankEnd - startProcessing).count();

            cout << "\nRanked Links:\n";
            for (const auto& link : rankedLinks)
                cout << link.url << " (Score: " << link.score << ")\n";

            cout << "\n--- Timing Information ---\n";
            cout << "Data Retrieval Time: " << dataRetrievalTime << " ms\n";
            cout << "Total Processing Time: " << totalProcessingTime << " ms\n";

        } catch (const json::parse_error& e) {
            cerr << "JSON parse error: " << e.what() << endl;
        } catch (const json::type_error& e) {
            cerr << "JSON type error: " << e.what() << endl;
        } catch (const exception& e) {
            cerr << "Unexpected error: " << e.what() << endl;
        }

    } else {
        cerr << "No JSON data found in the response.\n";
    }
}


int main() {
    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed: " << WSAGetLastError() << endl;
        return 1;
    }

    cout << "Winsock initialized successfully.\n\n";

    string query;
    cout << "Enter your search query: ";
    getline(cin, query);

    char choice;
    cout << "Use Hash Map for ranking? (y/n): ";
    cin >> choice;

    bool useHashMap = (choice == 'y' || choice == 'Y');

    string output_file = "results.json";
    fetch_and_save_data(query, output_file, useHashMap);

    WSACleanup();
    return 0;
}
