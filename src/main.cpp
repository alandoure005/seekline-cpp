#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> words;
    std::string word;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            word += static_cast<char>(c >= 'A' && c <= 'Z' ? c + 32 : c);
        else if (!word.empty()) { words.push_back(word); word.clear(); }
    }
    if (!word.empty()) words.push_back(word);
    return words;
}

std::string json_string(const std::string& value) {
    std::string out = "\"";
    const char* hex = "0123456789abcdef";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += static_cast<char>(c); }
        else if (c < 32) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += static_cast<char>(c);
    }
    return out + "\"";
}

struct Document { std::string path; std::size_t length; };
struct Hit { std::size_t id; double score; };

int main(int argc, char** argv) {
    if (argc < 3) { std::cerr << "Usage: seekline DIRECTORY QUERY [QUERY ...]\n"; return 2; }
    try {
        fs::path root = fs::canonical(argv[1]);
        if (!fs::is_directory(root)) throw std::runtime_error("Input must be a directory");
        std::set<std::string> query;
        for (int i = 2; i < argc; ++i)
            for (const auto& word : tokenize(argv[i])) query.insert(word);
        if (query.empty()) throw std::runtime_error("Query must contain ASCII letters or digits");
        std::vector<fs::path> paths;
        std::size_t skipped = 0;
        const std::set<std::string> extensions = {".txt", ".md", ".log"};
        for (const auto& entry : fs::recursive_directory_iterator(root)) {
            if (entry.is_symlink()) { ++skipped; continue; }
            if (entry.is_regular_file() && extensions.count(entry.path().extension().string())) paths.push_back(entry.path());
        }
        std::sort(paths.begin(), paths.end());
        std::vector<Document> docs;
        std::map<std::string, std::map<std::size_t, std::size_t>> index;
        std::size_t total_tokens = 0;
        constexpr std::size_t max_file_bytes = 1024 * 1024;
        for (const auto& path : paths) {
            if (fs::file_size(path) > max_file_bytes) { ++skipped; continue; }
            std::ifstream stream(path, std::ios::binary);
            if (!stream) throw std::runtime_error("Cannot read " + path.string());
            std::string text(max_file_bytes + 1, '\0');
            stream.read(text.data(), static_cast<std::streamsize>(text.size()));
            if (stream.bad()) throw std::runtime_error("Read failed: " + path.string());
            text.resize(static_cast<std::size_t>(stream.gcount()));
            if (text.size() > max_file_bytes || text.find('\0') != std::string::npos) { ++skipped; continue; }
            auto words = tokenize(text);
            std::size_t id = docs.size();
            docs.push_back({fs::relative(path, root).generic_string(), words.size()});
            total_tokens += words.size();
            for (const auto& word : words) ++index[word][id];
        }
        std::map<std::size_t, double> scores;
        bool first = true;
        double average = docs.empty() ? 1.0 : static_cast<double>(total_tokens) / static_cast<double>(docs.size());
        if (average == 0) average = 1;
        for (const auto& term : query) {
            auto found = index.find(term);
            if (found == index.end()) { scores.clear(); break; }
            const auto& posting = found->second;
            double idf = std::log(1.0 + (static_cast<double>(docs.size() - posting.size()) + 0.5) /
                                      (static_cast<double>(posting.size()) + 0.5));
            std::map<std::size_t, double> next;
            for (const auto& [id, count] : posting) {
                if (!first && !scores.count(id)) continue;
                double frequency = static_cast<double>(count);
                double norm = frequency + 1.2 * (0.25 + 0.75 * static_cast<double>(docs[id].length) / average);
                next[id] = (first ? 0 : scores.at(id)) + idf * (frequency * 2.2 / norm);
            }
            scores = std::move(next);
            first = false;
        }
        std::vector<Hit> hits;
        for (const auto& [id, score] : scores) hits.push_back({id, score});
        std::sort(hits.begin(), hits.end(), [&](const Hit& a, const Hit& b) {
            if (a.score != b.score) return a.score > b.score;
            return docs[a.id].path < docs[b.id].path;
        });
        std::cout << "{\"documents\":" << docs.size() << ",\"skipped\":" << skipped << ",\"matches\":[";
        for (std::size_t i = 0; i < hits.size(); ++i) {
            if (i) std::cout << ',';
            std::cout << "{\"path\":" << json_string(docs[hits[i].id].path) << ",\"score\":" << hits[i].score << '}';
        }
        std::cout << "]}\n";
    } catch (const std::exception& error) {
        std::cerr << "seekline: " << error.what() << '\n'; return 2;
    }
}
