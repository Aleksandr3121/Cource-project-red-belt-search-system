#pragma once

#include <istream>
#include <ostream>
#include <set>
#include <list>
#include <vector>
#include <map>
#include <string>
#include <string_view>
#include <future>
#include <functional>
#include <mutex>
using namespace std;



class InvertedIndex {
public:
    InvertedIndex() {
        docs.reserve(50000);
    }

    void Add(string document);
    const vector<pair<size_t, size_t>>& Lookup(string_view word) const;

    const string& GetDocument(size_t id) const {
        return docs[id];
    }

    size_t Size() {
        return docs.size();
    }

private:
    map<string_view, vector<pair<size_t, size_t>>> index;
    vector<string> docs;
    vector<pair<size_t, size_t>> emptyVector;
};

class SearchServer {
public:
    SearchServer() = default;
    explicit SearchServer(istream& document_input);
    void UpdateDocumentBaseOneThread(istream& document_input);
    void AddQueriesStreamOneThread(istream& query_input, ostream& search_results_output);
    void AddQueriesStream(istream& query_input, ostream& search_results_output);
    void UpdateDocumentBase(istream& document_input);

    ~SearchServer() {
        for (auto& f : futures)f.get();
    }

private:
    vector<future<void>> futures;
    mutex m;
    InvertedIndex index;
};
